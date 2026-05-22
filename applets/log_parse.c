#include "libpipeline.h"
#include "stream_logger.h"

#include <regex.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>

typedef enum {
    FORMAT_JSON = 0,
    FORMAT_CSV = 1
} output_format_t;

typedef struct {
    char *key;
    char *value;
} filter_t;

typedef struct {
    char **names;
    char **values;
    size_t count;
} record_t;

typedef struct {
    regex_t regex;
    record_t record;
} regex_state_t;

/* CLI and line helpers. */

static void print_usage(FILE *stream, const char *prog_name) {
    fprintf(stream, "Usage: %s [OPTIONS]\n\n", prog_name);
    fprintf(stream, "Description:\n");
    fprintf(stream, "  A lightweight structured record processor for UNIX pipelines.\n");
    fprintf(stream, "  Reads from stdin and writes to stdout. Diagnostic logs to stderr.\n\n");
    fprintf(stream, "Options:\n");
    fprintf(stream, "  -r, --regex <pattern>  Extract fields using POSIX extended regular expressions\n");
    fprintf(stream, "  -e, --fields <f1,f2>   Comma-separated field names for regex mode\n");
    fprintf(stream, "  -f, --filter <k=v>     Only emit JSON Lines where the key matches the value\n");
    fprintf(stream, "      --format <fmt>     Set output format (json|csv) (default: json)\n");
    fprintf(stream, "  -h, --help             Show this help message and exit\n");
}

static void trim_line(char *line) {
    char *start = line;
    while (isspace((unsigned char)*start)) {
        ++start;
    }
    if (start != line) {
        memmove(line, start, strlen(start) + 1);
    }

    size_t len = strlen(line);
    while (len > 0 && isspace((unsigned char)line[len - 1])) {
        line[--len] = '\0';
    }
}

static int parse_filter(char *arg, filter_t *filter) {
    char *eq = strchr(arg, '=');
    if (eq == NULL || eq == arg || eq[1] == '\0') {
        return -1;
    }
    *eq = '\0';
    filter->key = arg;
    filter->value = eq + 1;
    return 0;
}

/* Regex record storage and filtering. */

static int split_fields(char *arg, record_t *record) {
    size_t count = 1;
    for (char *p = arg; *p != '\0'; ++p) {
        if (*p == ',') {
            ++count;
        }
    }

    record->names = calloc(count, sizeof(record->names[0]));
    record->values = calloc(count, sizeof(record->values[0]));
    if (record->names == NULL || record->values == NULL) {
        free(record->names);
        free(record->values);
        record->names = NULL;
        record->values = NULL;
        return -1;
    }

    size_t idx = 0;
    char *save = NULL;
    char *tok = strtok_r(arg, ",", &save);
    while (tok != NULL) {
        if (*tok == '\0') {
            return -1;
        }
        record->names[idx++] = tok;
        tok = strtok_r(NULL, ",", &save);
    }
    record->count = idx;
    return idx == count ? 0 : -1;
}

static void free_record_values(record_t *record) {
    for (size_t i = 0; i < record->count; ++i) {
        free(record->values[i]);
        record->values[i] = NULL;
    }
}

static void free_record(record_t *record) {
    free_record_values(record);
    free(record->names);
    free(record->values);
}

static const char *record_get(const record_t *record, const char *key) {
    for (size_t i = 0; i < record->count; ++i) {
        if (strcmp(record->names[i], key) == 0) {
            return record->values[i];
        }
    }
    return NULL;
}

static int record_matches_filter(const record_t *record, const filter_t *filter) {
    if (filter->key == NULL) {
        return 1;
    }
    const char *value = record_get(record, filter->key);
    return value != NULL && strcmp(value, filter->value) == 0;
}

/* Structured output formatting. */

static int buffer_append_json_string(pipeline_buffer_t *buf, const char *s) {
    if (pipeline_buffer_append_char(buf, '"') != 0) {
        return -1;
    }
    for (const unsigned char *p = (const unsigned char *)s; *p != '\0'; ++p) {
        switch (*p) {
            case '"':
            case '\\':
                if (pipeline_buffer_append_char(buf, '\\') != 0 || pipeline_buffer_append_char(buf, (char)*p) != 0) {
                    return -1;
                }
                break;
            case '\n':
                if (pipeline_buffer_append_str(buf, "\\n") != 0) {
                    return -1;
                }
                break;
            case '\r':
                if (pipeline_buffer_append_str(buf, "\\r") != 0) {
                    return -1;
                }
                break;
            case '\t':
                if (pipeline_buffer_append_str(buf, "\\t") != 0) {
                    return -1;
                }
                break;
            default:
                if (pipeline_buffer_append_char(buf, (char)*p) != 0) {
                    return -1;
                }
                break;
        }
    }
    return pipeline_buffer_append_char(buf, '"');
}

static int emit_json(const record_t *record) {
    pipeline_buffer_t buf = {0};
    if (pipeline_buffer_append_char(&buf, '{') != 0) {
        goto buffer_fail;
    }
    for (size_t i = 0; i < record->count; ++i) {
        if (i > 0) {
            if (pipeline_buffer_append_char(&buf, ',') != 0) {
                goto buffer_fail;
            }
        }
        if (buffer_append_json_string(&buf, record->names[i]) != 0) {
            goto buffer_fail;
        }
        if (pipeline_buffer_append_char(&buf, ':') != 0) {
            goto buffer_fail;
        }
        if (buffer_append_json_string(&buf, record->values[i]) != 0) {
            goto buffer_fail;
        }
    }
    if (pipeline_buffer_append_str(&buf, "}\n") != 0) {
        goto buffer_fail;
    }
    fputs(buf.data, stdout);
    pipeline_buffer_free(&buf);
    return 0;

buffer_fail:
    pipeline_buffer_free(&buf);
    return -1;
}

static void write_csv_field(FILE *out, const char *s) {
    int quote = strchr(s, ',') != NULL ||
                strchr(s, '"') != NULL ||
                strchr(s, '\n') != NULL ||
                strchr(s, '\r') != NULL;
    if (!quote) {
        fputs(s, out);
        return;
    }
    fputc('"', out);
    for (const char *p = s; *p != '\0'; ++p) {
        if (*p == '"') {
            fputc('"', out);
        }
        fputc(*p, out);
    }
    fputc('"', out);
}

static void emit_csv(const record_t *record) {
    for (size_t i = 0; i < record->count; ++i) {
        if (i > 0) {
            fputc(',', stdout);
        }
        write_csv_field(stdout, record->values[i]);
    }
    fputc('\n', stdout);
}

/* Raw log regex parsing. */

static int parse_regex_record(const char *line, regex_t *regex, record_t *record)
{
    size_t nmatch = record->count + 1;
    regmatch_t *matches = calloc(nmatch, sizeof(matches[0]));
    if (matches == NULL) {
        return -1;
    }

    int rc = regexec(regex, line, nmatch, matches, 0);
    if (rc != 0) {
        free(matches);
        return 1;
    }

    for (size_t i = 0; i < record->count; ++i) {
        regmatch_t m = matches[i + 1];
        if (m.rm_so < 0 || m.rm_eo < m.rm_so) {
            free_record_values(record);
            free(matches);
            return 1;
        }
        record->values[i] = pipeline_strndup(line + m.rm_so, (size_t)(m.rm_eo - m.rm_so));
        if (record->values[i] == NULL) {
            free_record_values(record);
            free(matches);
            return -1;
        }
    }

    free(matches);
    return 0;
}

/* JSON Lines filtering for integration mode. */

static int looks_like_json_object(const char *line)
{
    const char *start = line;
    while (*start == ' ' || *start == '\t') {
        ++start;
    }
    if (*start != '{') {
        return 0;
    }

    const char *end = line + strlen(line);
    while (end > start && (end[-1] == ' ' || end[-1] == '\t')) {
        --end;
    }
    return end > start && end[-1] == '}';
}

static int process_json_line(const char *line, const filter_t *filter)
{
    if (!looks_like_json_object(line)) {
        return 1;
    }

    if (filter->key == NULL) {
        puts(line);
        return 0;
    }

    char *value = pipeline_json_find_string(line, filter->key);
    if (value == NULL) {
        return 0;
    }
    int matched = strcmp(value, filter->value) == 0;
    free(value);
    if (matched) {
        puts(line);
    }
    return 0;
}


int main(int argc, char *argv[]) {
    stream_logger_set_tag("log_parse");

    char *regex_pattern = NULL;
    char *fields_arg = NULL;
    char *format_arg = NULL;
    char *filter_kv = NULL;
    filter_t filter = {0};
    output_format_t format = FORMAT_JSON;
    regex_state_t regex_state = {0};
    int regex_mode = 0;
    int regex_ready = 0;
    int exit_code = 0;
    
    int opt;
    static struct option long_options[] = {
        {"regex",  required_argument, 0, 'r'},
        {"fields", required_argument, 0, 'e'},
        {"filter", required_argument, 0, 'f'},
        {"format", required_argument, 0, 1000},
        {"help",   no_argument,       0, 'h'},
        {0, 0, 0, 0}
    };

    while ((opt = getopt_long(argc, argv, "r:e:f:h", long_options, NULL)) != -1) {
        switch (opt) {
            case 'r':
                regex_pattern = optarg;
                break;
            case 'e':
                fields_arg = optarg;
                break;
            case 'f':
                filter_kv = optarg;
                if (parse_filter(filter_kv, &filter) != 0) {
                    LOG_ERROR("invalid filter syntax; expected key=value");
                    return 1;
                }
                break;
            case 1000: // --format
                format_arg = optarg;
                break;
            case 'h':
                print_usage(stdout, argv[0]);
                exit(EXIT_SUCCESS);
            case '?':
                print_usage(stderr, argv[0]);
                exit(EXIT_FAILURE);
            default:
                exit(EXIT_FAILURE);
        }
    }

    if (format_arg != NULL) {
        if (strcmp(format_arg, "json") == 0) {
            format = FORMAT_JSON;
        } else if (strcmp(format_arg, "csv") == 0) {
            format = FORMAT_CSV;
        } else {
            LOG_ERROR("unsupported format=%s", format_arg);
            return 1;
        }
    }

    if (regex_pattern != NULL) {
        regex_mode = 1;
        if (fields_arg == NULL) {
            LOG_ERROR("--fields is required with --regex");
            return 1;
        }
        if (split_fields(fields_arg, &regex_state.record) != 0 || regex_state.record.count == 0) {
            LOG_ERROR("invalid --fields value");
            free_record(&regex_state.record);
            return 1;
        }

        int rc = regcomp(&regex_state.regex, regex_pattern, REG_EXTENDED);
        if (rc != 0) {
            char err[256];
            regerror(rc, &regex_state.regex, err, sizeof(err));
            LOG_ERROR("invalid regex: %s", err);
            free_record(&regex_state.record);
            return 1;
        }
        regex_ready = 1;
    } else if (fields_arg != NULL || format_arg != NULL) {
        LOG_ERROR("--fields and --format require --regex");
        return 1;
    }

    char *line = NULL;
    size_t cap = 0;
    while (getline(&line, &cap, stdin) > 0) {
        trim_line(line);

        if (regex_mode) {
            int rc = parse_regex_record(line, &regex_state.regex, &regex_state.record);
            if (rc < 0) {
                LOG_ERROR("out of memory while parsing line");
                exit_code = 2;
                break;
            }
            if (rc > 0) {
                LOG_WARN("line did not match regex; skipping");
                continue;
            }

            if (record_matches_filter(&regex_state.record, &filter)) {
                if (format == FORMAT_JSON) {
                    if (emit_json(&regex_state.record) != 0) {
                        LOG_ERROR("out of memory while formatting JSON output");
                        free_record_values(&regex_state.record);
                        exit_code = 2;
                        break;
                    }
                } else {
                    emit_csv(&regex_state.record);
                }
            }
            free_record_values(&regex_state.record);
        } else if (process_json_line(line, &filter) != 0) {
            LOG_WARN("malformed JSON line; skipping");
        }
    }

    if (ferror(stdin)) {
        LOG_ERROR("stdin read error");
        exit_code = 2;
    }

    free(line);
    if (regex_ready) {
        regfree(&regex_state.regex);
    }
    if (regex_mode) {
        free_record(&regex_state.record);
    }
    fflush(stdout);

    return exit_code;
}