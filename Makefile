# ws_pipeline_dispatcher — v1 minimal Makefile
#
# Targets:
#   make            — build all binaries into build/
#   make test       — build & run unit tests
#   make smoke      — end-to-end skeleton smoke test
#   make clean      — remove build artifacts
#
# Layout:
#   lib/     shared code (libpipeline, stream_logger)
#   applets/ one .c per executable (pipeline_dispatcher + 3 stubs)
#   tests/   unit tests
#   build/   build outputs (created at build time)

CC        ?= cc
CFLAGS    ?= -std=c11 -O2 -g -Wall -Wextra -Wpedantic -D_POSIX_C_SOURCE=200809L
CPPFLAGS  ?= -Ilib
LDFLAGS   ?=
LDLIBS    ?=

BUILD_DIR := build
LIB_SRCS  := lib/libpipeline.c lib/stream_logger.c
LIB_OBJS  := $(BUILD_DIR)/libpipeline.o $(BUILD_DIR)/stream_logger.o

APPLETS   := pipeline_dispatcher stream_merge log_parse clip_store
BINS      := $(addprefix $(BUILD_DIR)/,$(APPLETS))

TEST_BINS    := $(BUILD_DIR)/test_libpipeline $(BUILD_DIR)/test_stream_logger
TEST_SCRIPTS := tests/test_log_parse.sh

.PHONY: all clean test smoke

all: $(BINS)

$(BUILD_DIR):
	@mkdir -p $@

$(BUILD_DIR)/libpipeline.o: lib/libpipeline.c lib/libpipeline.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

$(BUILD_DIR)/stream_logger.o: lib/stream_logger.c lib/stream_logger.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

# Each applet links against the shared lib objects.
$(BUILD_DIR)/%: applets/%.c $(LIB_OBJS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(CPPFLAGS) $< $(LIB_OBJS) $(LDFLAGS) $(LDLIBS) -o $@

$(BUILD_DIR)/test_libpipeline: tests/test_libpipeline.c $(LIB_OBJS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(CPPFLAGS) $< $(LIB_OBJS) $(LDFLAGS) $(LDLIBS) -o $@

$(BUILD_DIR)/test_stream_logger: tests/test_stream_logger.c $(LIB_OBJS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(CPPFLAGS) $< $(LIB_OBJS) $(LDFLAGS) $(LDLIBS) -o $@

test: $(TEST_BINS) $(BUILD_DIR)/log_parse
	@for test_bin in $(TEST_BINS); do $$test_bin || exit $$?; done
	@for test_script in $(TEST_SCRIPTS); do LOG_PARSE=$(BUILD_DIR)/log_parse sh $$test_script || exit $$?; done

# End-to-end smoke: run dispatcher with skeleton stubs from build/.
smoke: all
	@mkdir -p /tmp/ws_pipeline_smoke
	@cd $(BUILD_DIR) && ./pipeline_dispatcher \
	    smoke_session /tmp/ws_pipeline_smoke /tmp/ws_pipeline_smoke/clips.db 300

clean:
	rm -rf $(BUILD_DIR)
