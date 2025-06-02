# SPINDEX Makefile

# Compiler and Tools
CC = gcc
LD = gcc
# CFLAGS for compilation (warnings, debug symbols, standard)
CFLAGS = -Wall -Wextra -g -std=c11 -fPIC
# Include directories
INCLUDE_DIRS = -I./include -I./core
# LDFLAGS for linking
LDFLAGS = -ldl -rdynamic -lcurl # For dlopen, dlsym, plugins, and libcurl

# Directories
CORE_DIR = ./core
INCLUDE_DIR = ./include
PLUGIN_DIR = ./plugins
BUILD_DIR = ./build
TARGET_APP = $(BUILD_DIR)/spindex_app

# Core source files and object files
# Assuming main.c will be created later and link against these objects
CORE_SRCS = $(wildcard $(CORE_DIR)/*.c) # Finds all .c files in core
# Filter out test files from CORE_SRCS if they follow a pattern e.g. test_*.c
CORE_SRCS := $(filter-out $(CORE_DIR)/test_%.c, $(CORE_SRCS))
CORE_OBJS = $(patsubst $(CORE_DIR)/%.c, $(BUILD_DIR)/core/%.o, $(CORE_SRCS))

# Test program sources (example, can be expanded)
TEST_CONFIG_LOADER_SRC = $(CORE_DIR)/test_config_manager.c
TEST_CONFIG_LOADER_OBJ = $(BUILD_DIR)/core/test_config_manager.o
TEST_CONFIG_LOADER_DEPS = $(BUILD_DIR)/core/config_manager.o
TEST_CONFIG_LOADER_TARGET = $(BUILD_DIR)/test_config_manager

TEST_PLUGIN_LOADER_SRC = $(CORE_DIR)/test_plugin_loader.c
TEST_PLUGIN_LOADER_OBJ = $(BUILD_DIR)/core/test_plugin_loader.o
TEST_PLUGIN_LOADER_DEPS = $(BUILD_DIR)/core/plugin_loader.o $(BUILD_DIR)/core/config_manager.o # if it uses config
TEST_PLUGIN_LOADER_TARGET = $(BUILD_DIR)/test_plugin_loader

TEST_LOGGER_SRC = $(CORE_DIR)/test_logger.c
TEST_LOGGER_OBJ = $(BUILD_DIR)/core/test_logger.o # Though it's simple, consistency
TEST_LOGGER_DEPS = $(BUILD_DIR)/core/utils.o $(BUILD_DIR)/core/config_manager.o # Config for LogLevel definition
TEST_LOGGER_TARGET = $(BUILD_DIR)/test_logger

TEST_HW_DETECT_SRC = $(CORE_DIR)/test_hardware_detection.c
TEST_HW_DETECT_DEPS = $(BUILD_DIR)/core/hardware_detection.o
TEST_HW_DETECT_TARGET = $(BUILD_DIR)/test_hardware_detection

TEST_PIPELINE_SRC = $(CORE_DIR)/test_ingestion_pipeline.c
TEST_PIPELINE_DEPS = $(BUILD_DIR)/core/ingestion_pipeline.o $(BUILD_DIR)/core/plugin_loader.o \
                     $(BUILD_DIR)/core/config_manager.o $(BUILD_DIR)/core/utils.o $(BUILD_DIR)/core/hardware_detection.o \
                     plugins # Ensure all plugins defined under 'plugins' target are built
TEST_PIPELINE_TARGET = $(BUILD_DIR)/test_ingestion_pipeline

# Directory for staging plugins for tests that need multiple plugin types
TEST_PLUGINS_STAGING_DIR = $(BUILD_DIR)/plugins_for_test

# Common directory for plugin .o files and .so files within BUILD_DIR
PLUGIN_OBJ_DIR = $(BUILD_DIR)/plugins/obj
PLUGIN_LIB_DIR = $(BUILD_DIR)/plugins/lib

TEST_ES_CLIENT_SRC = $(CORE_DIR)/test_elasticsearch_client.c
TEST_ES_CLIENT_DEPS = $(BUILD_DIR)/core/elasticsearch_client.o $(BUILD_DIR)/core/utils.o # Assuming es_client uses utils for logging
TEST_ES_CLIENT_TARGET = $(BUILD_DIR)/test_elasticsearch_client

TEST_ES_INDEXING_SRC = $(CORE_DIR)/test_elasticsearch_indexing.c
TEST_ES_INDEXING_DEPS = \
    $(BUILD_DIR)/core/config_manager.o \
    $(BUILD_DIR)/core/ingestion_pipeline.o \
    $(BUILD_DIR)/core/plugin_loader.o \
    $(BUILD_DIR)/core/utils.o \
    $(BUILD_DIR)/core/elasticsearch_client.o \
    plugins # Depends on all plugins being built and staged
TEST_ES_INDEXING_TARGET = $(BUILD_DIR)/test_elasticsearch_indexing

# For the rule below, we need the .o files for linking, and 'plugins' to ensure .so are built for copying.
TEST_ES_INDEXING_LINK_OBJS = $(filter %.o,$(TEST_ES_INDEXING_DEPS))


# Plugin sources & build artifact definitions
DUMMY_PLUGIN_SRC_DIR = $(PLUGIN_DIR)/dummy_plugin
DUMMY_PLUGIN_SRC = $(DUMMY_PLUGIN_SRC_DIR)/dummy_plugin.c
DUMMY_PLUGIN_OBJ = $(PLUGIN_OBJ_DIR)/dummy_plugin/dummy_plugin.o
DUMMY_PLUGIN_SO = $(PLUGIN_LIB_DIR)/dummy_plugin.so

JSON_PREPROCESSOR_PLUGIN_SRC_DIR = $(PLUGIN_DIR)/preprocessors
JSON_PREPROCESSOR_PLUGIN_SRC = $(JSON_PREPROCESSOR_PLUGIN_SRC_DIR)/json_preprocessor.c
JSON_PREPROCESSOR_PLUGIN_OBJ = $(PLUGIN_OBJ_DIR)/preprocessors/json_preprocessor.o
JSON_PREPROCESSOR_PLUGIN_SO = $(PLUGIN_LIB_DIR)/json_preprocessor.so

JSON_PARSER_PLUGIN_SRC_DIR = $(PLUGIN_DIR)/parsers
JSON_PARSER_PLUGIN_SRC = $(JSON_PARSER_PLUGIN_SRC_DIR)/json_parser.c
JSON_PARSER_PLUGIN_OBJ = $(PLUGIN_OBJ_DIR)/parsers/json_parser.o
JSON_PARSER_PLUGIN_SO = $(PLUGIN_LIB_DIR)/json_parser.so

ELASTICSEARCH_INDEXER_PLUGIN_SRC_DIR = $(PLUGIN_DIR)/indexers
ELASTICSEARCH_INDEXER_PLUGIN_SRC = $(ELASTICSEARCH_INDEXER_PLUGIN_SRC_DIR)/elasticsearch_indexer.c
ELASTICSEARCH_INDEXER_PLUGIN_OBJ = $(PLUGIN_OBJ_DIR)/indexers/elasticsearch_indexer.o
ELASTICSEARCH_INDEXER_PLUGIN_SO = $(PLUGIN_LIB_DIR)/elasticsearch_indexer.so

TXT_PREPROCESSOR_PLUGIN_SRC_DIR = $(PLUGIN_DIR)/preprocessors
TXT_PREPROCESSOR_PLUGIN_SRC = $(TXT_PREPROCESSOR_PLUGIN_SRC_DIR)/txt_preprocessor.c
TXT_PREPROCESSOR_PLUGIN_OBJ = $(PLUGIN_OBJ_DIR)/preprocessors/txt_preprocessor.o
TXT_PREPROCESSOR_PLUGIN_SO = $(PLUGIN_LIB_DIR)/txt_preprocessor.so

TXT_PARSER_PLUGIN_SRC_DIR = $(PLUGIN_DIR)/parsers
TXT_PARSER_PLUGIN_SRC = $(TXT_PARSER_PLUGIN_SRC_DIR)/txt_parser.c
TXT_PARSER_PLUGIN_OBJ = $(PLUGIN_OBJ_DIR)/parsers/txt_parser.o
TXT_PARSER_PLUGIN_SO = $(PLUGIN_LIB_DIR)/txt_parser.so

CSV_PREPROCESSOR_PLUGIN_SRC_DIR = $(PLUGIN_DIR)/preprocessors
CSV_PREPROCESSOR_PLUGIN_SRC = $(CSV_PREPROCESSOR_PLUGIN_SRC_DIR)/csv_preprocessor.c
CSV_PREPROCESSOR_PLUGIN_OBJ = $(PLUGIN_OBJ_DIR)/preprocessors/csv_preprocessor.o
CSV_PREPROCESSOR_PLUGIN_SO = $(PLUGIN_LIB_DIR)/csv_preprocessor.so

CSV_PARSER_PLUGIN_SRC_DIR = $(PLUGIN_DIR)/parsers
CSV_PARSER_PLUGIN_SRC = $(CSV_PARSER_PLUGIN_SRC_DIR)/csv_parser.c
CSV_PARSER_PLUGIN_OBJ = $(PLUGIN_OBJ_DIR)/parsers/csv_parser.o
CSV_PARSER_PLUGIN_SO = $(PLUGIN_LIB_DIR)/csv_parser.so

# Decompressor Plugin Base Directory
DECOMPRESSORS_PLUGIN_SRC_DIR = $(PLUGIN_DIR)/decompressors

# ZIP Decompressor Plugin
ZIP_DECOMPRESSOR_PLUGIN_SRC = $(DECOMPRESSORS_PLUGIN_SRC_DIR)/zip_decompressor.c
ZIP_DECOMPRESSOR_PLUGIN_OBJ = $(PLUGIN_OBJ_DIR)/decompressors/zip_decompressor.o
ZIP_DECOMPRESSOR_PLUGIN_SO = $(PLUGIN_LIB_DIR)/zip_decompressor.so

# Gzip Decompressor Plugin
GZIP_DECOMPRESSOR_PLUGIN_SRC = $(DECOMPRESSORS_PLUGIN_SRC_DIR)/gzip_decompressor.c
GZIP_DECOMPRESSOR_PLUGIN_OBJ = $(PLUGIN_OBJ_DIR)/decompressors/gzip_decompressor.o
GZIP_DECOMPRESSOR_PLUGIN_SO = $(PLUGIN_LIB_DIR)/gzip_decompressor.so

# Zstd Decompressor Plugin
ZSTD_DECOMPRESSOR_PLUGIN_SRC = $(DECOMPRESSORS_PLUGIN_SRC_DIR)/zstd_decompressor.c
ZSTD_DECOMPRESSOR_PLUGIN_OBJ = $(PLUGIN_OBJ_DIR)/decompressors/zstd_decompressor.o
ZSTD_DECOMPRESSOR_PLUGIN_SO = $(PLUGIN_LIB_DIR)/zstd_decompressor.so

# Phony targets
.PHONY: all clean plugins tests

all: $(TARGET_APP) plugins tests

# Rule to build the main application
$(TARGET_APP): $(CORE_OBJS) $(CORE_DIR)/main.c # Add main.c when it exists
	@mkdir -p $(@D) # Ensure build directory exists
	$(LD) $(CFLAGS) $(INCLUDE_DIRS) $(CORE_DIR)/main.c $(CORE_OBJS) -o $@ $(LDFLAGS)
	@echo "Built main application: $@"

# Rule to compile core C files to object files
$(BUILD_DIR)/core/%.o: $(CORE_DIR)/%.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(INCLUDE_DIRS) -c $< -o $@
	@echo "Compiled core object: $@"

# Specific rule for elasticsearch_client.o to list its dependencies more clearly
$(BUILD_DIR)/core/elasticsearch_client.o: $(CORE_DIR)/elasticsearch_client.c $(INCLUDE_DIR)/elasticsearch_client.h $(INCLUDE_DIR)/utils.h
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(INCLUDE_DIRS) -c $(CORE_DIR)/elasticsearch_client.c -o $@
	@echo "Compiled core object (specific rule): $@"


# --- Test Program Rules ---
tests: $(TEST_CONFIG_LOADER_TARGET) $(TEST_PLUGIN_LOADER_TARGET) $(TEST_LOGGER_TARGET) $(TEST_HW_DETECT_TARGET) $(TEST_PIPELINE_TARGET) $(TEST_ES_CLIENT_TARGET) $(TEST_ES_INDEXING_TARGET)

$(TEST_CONFIG_LOADER_TARGET): $(TEST_CONFIG_LOADER_SRC) $(TEST_CONFIG_LOADER_DEPS)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(INCLUDE_DIRS) $^ -o $@ $(LDFLAGS)
	@echo "Built test program: $@"

$(TEST_ES_INDEXING_TARGET): $(TEST_ES_INDEXING_SRC) $(TEST_ES_INDEXING_LINK_OBJS) plugins
	@mkdir -p $(@D)
	# Ensure plugins are staged for this test
	@mkdir -p $(TEST_PLUGINS_STAGING_DIR)
	cp $(PLUGIN_LIB_DIR)/dummy_plugin.so $(TEST_PLUGINS_STAGING_DIR)/
	cp $(PLUGIN_LIB_DIR)/json_preprocessor.so $(TEST_PLUGINS_STAGING_DIR)/
	cp $(PLUGIN_LIB_DIR)/json_parser.so $(TEST_PLUGINS_STAGING_DIR)/
	cp $(PLUGIN_LIB_DIR)/elasticsearch_indexer.so $(TEST_PLUGINS_STAGING_DIR)/
	cp $(PLUGIN_LIB_DIR)/txt_preprocessor.so $(TEST_PLUGINS_STAGING_DIR)/
	cp $(PLUGIN_LIB_DIR)/txt_parser.so $(TEST_PLUGINS_STAGING_DIR)/
	cp $(PLUGIN_LIB_DIR)/csv_preprocessor.so $(TEST_PLUGINS_STAGING_DIR)/
	cp $(PLUGIN_LIB_DIR)/csv_parser.so $(TEST_PLUGINS_STAGING_DIR)/
	cp $(PLUGIN_LIB_DIR)/zip_decompressor.so $(TEST_PLUGINS_STAGING_DIR)/
	cp $(PLUGIN_LIB_DIR)/gzip_decompressor.so $(TEST_PLUGINS_STAGING_DIR)/
	cp $(PLUGIN_LIB_DIR)/zstd_decompressor.so $(TEST_PLUGINS_STAGING_DIR)/
	$(CC) $(CFLAGS) $(INCLUDE_DIRS) $(TEST_ES_INDEXING_SRC) $(TEST_ES_INDEXING_LINK_OBJS) -o $@ $(LDFLAGS)
	@echo "Built test program: $@"
	@echo "Staged plugins for test in $(TEST_PLUGINS_STAGING_DIR)"

$(TEST_ES_CLIENT_TARGET): $(TEST_ES_CLIENT_SRC) $(TEST_ES_CLIENT_DEPS)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(INCLUDE_DIRS) $^ -o $@ $(LDFLAGS)
	@echo "Built test program: $@"

$(TEST_PIPELINE_TARGET): $(TEST_PIPELINE_SRC) $(TEST_PIPELINE_DEPS)
	@mkdir -p $(@D)
	@mkdir -p $(TEST_PLUGINS_STAGING_DIR)
	# Ensure plugins are built first, then copy from PLUGIN_LIB_DIR
	cp $(PLUGIN_LIB_DIR)/dummy_plugin.so $(TEST_PLUGINS_STAGING_DIR)/
	cp $(PLUGIN_LIB_DIR)/json_preprocessor.so $(TEST_PLUGINS_STAGING_DIR)/
	cp $(PLUGIN_LIB_DIR)/json_parser.so $(TEST_PLUGINS_STAGING_DIR)/
	cp $(PLUGIN_LIB_DIR)/elasticsearch_indexer.so $(TEST_PLUGINS_STAGING_DIR)/
	cp $(PLUGIN_LIB_DIR)/txt_preprocessor.so $(TEST_PLUGINS_STAGING_DIR)/
	cp $(PLUGIN_LIB_DIR)/txt_parser.so $(TEST_PLUGINS_STAGING_DIR)/
	cp $(PLUGIN_LIB_DIR)/csv_preprocessor.so $(TEST_PLUGINS_STAGING_DIR)/
	cp $(PLUGIN_LIB_DIR)/csv_parser.so $(TEST_PLUGINS_STAGING_DIR)/
	cp $(PLUGIN_LIB_DIR)/zip_decompressor.so $(TEST_PLUGINS_STAGING_DIR)/
	cp $(PLUGIN_LIB_DIR)/gzip_decompressor.so $(TEST_PLUGINS_STAGING_DIR)/
	cp $(PLUGIN_LIB_DIR)/zstd_decompressor.so $(TEST_PLUGINS_STAGING_DIR)/
	$(CC) $(CFLAGS) $(INCLUDE_DIRS) $(TEST_PIPELINE_SRC) \
		$(filter %.o,$^) -o $@ $(LDFLAGS) # Link only .o files from DEPS, not .so
	@echo "Built test program: $@"
	@echo "Staged plugins for test in $(TEST_PLUGINS_STAGING_DIR)"

$(TEST_HW_DETECT_TARGET): $(TEST_HW_DETECT_SRC) $(TEST_HW_DETECT_DEPS)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(INCLUDE_DIRS) $^ -o $@ $(LDFLAGS)
	@echo "Built test program: $@"

$(TEST_LOGGER_TARGET): $(TEST_LOGGER_SRC) $(TEST_LOGGER_DEPS)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(INCLUDE_DIRS) $^ -o $@ $(LDFLAGS)
	@echo "Built test program: $@"

$(TEST_PLUGIN_LOADER_TARGET): $(TEST_PLUGIN_LOADER_SRC) $(TEST_PLUGIN_LOADER_DEPS)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(INCLUDE_DIRS) $^ -o $@ $(LDFLAGS)
	@echo "Built test program: $@"


# --- Plugin Rules ---
plugins: $(DUMMY_PLUGIN_SO) $(JSON_PREPROCESSOR_PLUGIN_SO) $(JSON_PARSER_PLUGIN_SO) $(ELASTICSEARCH_INDEXER_PLUGIN_SO) \
         $(TXT_PREPROCESSOR_PLUGIN_SO) $(TXT_PARSER_PLUGIN_SO) $(CSV_PREPROCESSOR_PLUGIN_SO) $(CSV_PARSER_PLUGIN_SO) \
         $(ZIP_DECOMPRESSOR_PLUGIN_SO) $(GZIP_DECOMPRESSOR_PLUGIN_SO) $(ZSTD_DECOMPRESSOR_PLUGIN_SO)
	@echo "All specified plugins built."

# Rule for dummy_plugin object file
$(DUMMY_PLUGIN_OBJ): $(DUMMY_PLUGIN_SRC) $(INCLUDE_DIR)/plugin_interface.h $(INCLUDE_DIR)/data_types.h $(INCLUDE_DIR)/utils.h
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(INCLUDE_DIRS) -c $< -o $@
	@echo "Compiled plugin object: $@"

# Rule for json_preprocessor object file
$(JSON_PREPROCESSOR_PLUGIN_OBJ): $(JSON_PREPROCESSOR_PLUGIN_SRC) $(INCLUDE_DIR)/plugin_interface.h $(INCLUDE_DIR)/data_types.h $(INCLUDE_DIR)/utils.h
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(INCLUDE_DIRS) -c $< -o $@
	@echo "Compiled plugin object: $@"

# Rule for json_parser object file
$(JSON_PARSER_PLUGIN_OBJ): $(JSON_PARSER_PLUGIN_SRC) $(INCLUDE_DIR)/plugin_interface.h $(INCLUDE_DIR)/data_types.h $(INCLUDE_DIR)/utils.h
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(INCLUDE_DIRS) -c $< -o $@
	@echo "Compiled plugin object: $@"

# Rule for elasticsearch_indexer object file
$(ELASTICSEARCH_INDEXER_PLUGIN_OBJ): $(ELASTICSEARCH_INDEXER_PLUGIN_SRC) $(INCLUDE_DIR)/plugin_interface.h $(INCLUDE_DIR)/data_types.h $(INCLUDE_DIR)/utils.h $(INCLUDE_DIR)/elasticsearch_client.h
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(INCLUDE_DIRS) -c $< -o $@
	@echo "Compiled plugin object: $@"

# Rule for txt_preprocessor object file
$(TXT_PREPROCESSOR_PLUGIN_OBJ): $(TXT_PREPROCESSOR_PLUGIN_SRC) $(INCLUDE_DIR)/plugin_interface.h $(INCLUDE_DIR)/data_types.h $(INCLUDE_DIR)/utils.h $(INCLUDE_DIR)/config.h
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(INCLUDE_DIRS) -c $< -o $@
	@echo "Compiled plugin object: $@"

# Rule for txt_parser object file
$(TXT_PARSER_PLUGIN_OBJ): $(TXT_PARSER_PLUGIN_SRC) $(INCLUDE_DIR)/plugin_interface.h $(INCLUDE_DIR)/data_types.h $(INCLUDE_DIR)/utils.h
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(INCLUDE_DIRS) -c $< -o $@
	@echo "Compiled plugin object: $@"

# Rule for csv_preprocessor object file
$(CSV_PREPROCESSOR_PLUGIN_OBJ): $(CSV_PREPROCESSOR_PLUGIN_SRC) $(INCLUDE_DIR)/plugin_interface.h $(INCLUDE_DIR)/data_types.h $(INCLUDE_DIR)/utils.h $(INCLUDE_DIR)/config.h
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(INCLUDE_DIRS) -c $< -o $@
	@echo "Compiled plugin object: $@"

# Rule for csv_parser object file
$(CSV_PARSER_PLUGIN_OBJ): $(CSV_PARSER_PLUGIN_SRC) $(INCLUDE_DIR)/plugin_interface.h $(INCLUDE_DIR)/data_types.h $(INCLUDE_DIR)/utils.h
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(INCLUDE_DIRS) -c $< -o $@
	@echo "Compiled plugin object: $@"

# Rule for zip_decompressor object file
$(ZIP_DECOMPRESSOR_PLUGIN_OBJ): $(ZIP_DECOMPRESSOR_PLUGIN_SRC) $(INCLUDE_DIR)/plugin_interface.h $(INCLUDE_DIR)/data_types.h $(INCLUDE_DIR)/utils.h
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(INCLUDE_DIRS) -c $< -o $@
	@echo "Compiled plugin object: $@"

# Rule for gzip_decompressor object file
$(GZIP_DECOMPRESSOR_PLUGIN_OBJ): $(GZIP_DECOMPRESSOR_PLUGIN_SRC) $(INCLUDE_DIR)/plugin_interface.h $(INCLUDE_DIR)/data_types.h $(INCLUDE_DIR)/utils.h
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(INCLUDE_DIRS) -c $< -o $@
	@echo "Compiled plugin object: $@"

# Rule for zstd_decompressor object file
$(ZSTD_DECOMPRESSOR_PLUGIN_OBJ): $(ZSTD_DECOMPRESSOR_PLUGIN_SRC) $(INCLUDE_DIR)/plugin_interface.h $(INCLUDE_DIR)/data_types.h $(INCLUDE_DIR)/utils.h
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(INCLUDE_DIRS) -c $< -o $@
	@echo "Compiled plugin object: $@"

# Rule to build dummy_plugin.so from its object file
$(DUMMY_PLUGIN_SO): $(DUMMY_PLUGIN_OBJ)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -shared $^ -o $@ $(LDFLAGS) # Use LDFLAGS for plugins if they link other libs
	@echo "Built plugin: $@"

# Rule to build json_preprocessor.so from its object file
$(JSON_PREPROCESSOR_PLUGIN_SO): $(JSON_PREPROCESSOR_PLUGIN_OBJ)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -shared $^ -o $@ $(LDFLAGS)
	@echo "Built plugin: $@"

# Rule to build json_parser.so from its object file
$(JSON_PARSER_PLUGIN_SO): $(JSON_PARSER_PLUGIN_OBJ)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -shared $^ -o $@ $(LDFLAGS)
	@echo "Built plugin: $@"

# Rule to build elasticsearch_indexer.so from its object file
# It needs to link with elasticsearch_client.o
$(ELASTICSEARCH_INDEXER_PLUGIN_SO): $(ELASTICSEARCH_INDEXER_PLUGIN_OBJ) $(BUILD_DIR)/core/elasticsearch_client.o
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -shared $^ -o $@ $(LDFLAGS)
	@echo "Built plugin: $@"

# Rule to build txt_preprocessor.so from its object file
$(TXT_PREPROCESSOR_PLUGIN_SO): $(TXT_PREPROCESSOR_PLUGIN_OBJ)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -shared $^ -o $@ $(LDFLAGS)
	@echo "Built plugin: $@"

# Rule to build txt_parser.so from its object file
$(TXT_PARSER_PLUGIN_SO): $(TXT_PARSER_PLUGIN_OBJ)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -shared $^ -o $@ $(LDFLAGS)
	@echo "Built plugin: $@"

# Rule to build csv_preprocessor.so from its object file
$(CSV_PREPROCESSOR_PLUGIN_SO): $(CSV_PREPROCESSOR_PLUGIN_OBJ)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -shared $^ -o $@ $(LDFLAGS)
	@echo "Built plugin: $@"

# Rule to build csv_parser.so from its object file
$(CSV_PARSER_PLUGIN_SO): $(CSV_PARSER_PLUGIN_OBJ)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -shared $^ -o $@ $(LDFLAGS)
	@echo "Built plugin: $@"

# Rule to build zip_decompressor.so from its object file
$(ZIP_DECOMPRESSOR_PLUGIN_SO): $(ZIP_DECOMPRESSOR_PLUGIN_OBJ)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -shared $^ -o $@ $(LDFLAGS)
	@echo "Built plugin: $@"

# Rule to build gzip_decompressor.so from its object file
$(GZIP_DECOMPRESSOR_PLUGIN_SO): $(GZIP_DECOMPRESSOR_PLUGIN_OBJ)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -shared $^ -o $@ $(LDFLAGS) -lz
	@echo "Built plugin (linked with -lz): $@"

# Rule to build zstd_decompressor.so from its object file
$(ZSTD_DECOMPRESSOR_PLUGIN_SO): $(ZSTD_DECOMPRESSOR_PLUGIN_OBJ)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -shared $^ -o $@ $(LDFLAGS) -lzstd
	@echo "Built plugin (linked with -lzstd): $@"

# A more generic rule for plugins (if structure is consistent)
# For example, if each plugin is in its own dir under $(PLUGIN_DIR)
# and has one main .c file named after the directory:
# PLUGINS = $(wildcard $(PLUGIN_DIR)/*/)
# %.so: $(PLUGIN_DIR)/%/%.c
#	$(CC) $(CFLAGS) $(INCLUDE_DIRS) -shared $< -o $@

clean:
	@echo "Cleaning build artifacts..."
	rm -rf $(BUILD_DIR) # This removes core objects, test executables, and now also plugin build artifacts
	# Explicitly remove old .so files from source tree if they existed from previous builds
	rm -f $(PLUGIN_DIR)/dummy_plugin/dummy_plugin.so
	rm -f $(PLUGIN_DIR)/preprocessors/json_preprocessor.so
	rm -f $(PLUGIN_DIR)/parsers/json_parser.so
	rm -f $(PLUGIN_DIR)/indexers/elasticsearch_indexer.so
	rm -f $(PLUGIN_DIR)/preprocessors/txt_preprocessor.so
	rm -f $(PLUGIN_DIR)/parsers/txt_parser.so
	rm -f $(PLUGIN_DIR)/preprocessors/csv_preprocessor.so
	rm -f $(PLUGIN_DIR)/parsers/csv_parser.so
	@echo "Clean complete."

# Note: main.c doesn't exist yet. The $(TARGET_APP) rule will fail until it's created.
# The current CORE_SRCS will pick up existing .c files in core/.
# The filter for test_*.c is to avoid them being linked into the main app if they stay in core/.
# The build directory $(BUILD_DIR) is used for object files and final executables to keep source dirs clean.
# Test dependencies for test_plugin_loader might need adjustment if it uses config_manager.o
# The TEST_PLUGIN_LOADER_DEPS currently includes $(BUILD_DIR)/core/config_manager.o which is fine.
# If main.c is not ready, one can still build 'plugins' or 'tests'.
