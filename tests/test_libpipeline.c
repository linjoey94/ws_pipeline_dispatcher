/*
 * test_libpipeline.c — smoke tests for the v1 libpipeline API.
 */

#include "libpipeline.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static int failures = 0;
#define CHECK(cond) do { \
    if (!(cond)) { \
        fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #cond); \
        ++failures; \
    } \
} while (0)

static void test_sentinel(void)
{
    CHECK(pipeline_is_sentinel(".pipeline_end") == 1);
    CHECK(pipeline_is_sentinel("chunk_0000.bin") == 0);
    CHECK(pipeline_is_sentinel("") == 0);
    CHECK(pipeline_is_sentinel(NULL) == 0);
}

static void test_now_ms(void)
{
    int64_t a = pipeline_now_ms();
    int64_t b = pipeline_now_ms();
    CHECK(a > 0);
    CHECK(b >= a);
}

static void test_compress_json(void)
{
    char dst[64];
    int n = pipeline_compress_json("  { \"a\" : 1 }\n", dst, sizeof(dst));
    CHECK(n > 0);
    CHECK(strcmp(dst, "{\"a\":1}") == 0);

    /* dst too small */
    char small[3];
    CHECK(pipeline_compress_json("abcd", small, sizeof(small)) == -1);

    /* NULL inputs */
    CHECK(pipeline_compress_json(NULL, dst, sizeof(dst)) == -1);
    CHECK(pipeline_compress_json("x", NULL, 10) == -1);
}

int main(void)
{
    test_sentinel();
    test_now_ms();
    test_compress_json();
    if (failures == 0) {
        printf("OK: all libpipeline tests passed\n");
        return 0;
    }
    fprintf(stderr, "FAILED: %d check(s)\n", failures);
    return 1;
}
