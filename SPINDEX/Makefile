# SPINDEX Makefile

# Compiler and Tools
CC = gcc
LD = gcc
# CFLAGS for compilation (warnings, debug symbols, standard)
CFLAGS = -Wall -Wextra -g -std=c11 -fPIC
# Include directories
INCLUDE_DIRS = -I./include -I./core
# LDFLAGS for linking
LDFLAGS = -ldl -rdynamic # For dlopen, dlsym, and allowing plugins to access host symbols

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

# --- Test Program Rules ---
tests: $(TEST_CONFIG_LOADER_TARGET) $(TEST_PLUGIN_LOADER_TARGET) $(TEST_LOGGER_TARGET) $(TEST_HW_DETECT_TARGET) $(TEST_PIPELINE_TARGET)

$(TEST_CONFIG_LOADER_TARGET): $(TEST_CONFIG_LOADER_SRC) $(TEST_CONFIG_LOADER_DEPS)
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
plugins: $(DUMMY_PLUGIN_SO) $(JSON_PREPROCESSOR_PLUGIN_SO) $(JSON_PARSER_PLUGIN_SO)
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
	@echo "Clean complete."

# Note: main.c doesn't exist yet. The $(TARGET_APP) rule will fail until it's created.
# The current CORE_SRCS will pick up existing .c files in core/.
# The filter for test_*.c is to avoid them being linked into the main app if they stay in core/.
# The build directory $(BUILD_DIR) is used for object files and final executables to keep source dirs clean.
# Test dependencies for test_plugin_loader might need adjustment if it uses config_manager.o
# The TEST_PLUGIN_LOADER_DEPS currently includes $(BUILD_DIR)/core/config_manager.o which is fine.
# If main.c is not ready, one can still build 'plugins' or 'tests'.
