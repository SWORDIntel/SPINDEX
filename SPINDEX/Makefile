# SPINDEX Makefile

# Compiler and Tools
CC = gcc
LD = gcc
# CFLAGS for compilation (warnings, debug symbols, standard)
CFLAGS = -Wall -Wextra -g -std=c11 -fPIC
# Include directories
INCLUDE_DIRS = -I./include -I./core
# LDFLAGS for linking
LDFLAGS = -ldl -rdynamic -lcurl -lpthread # For dlopen, dlsym, plugins, libcurl, and pthreads

# Jansson flags
JANSSON_CFLAGS = $(shell pkg-config --cflags jansson || echo "-I/usr/include/jansson")
JANSSON_LIBS = $(shell pkg-config --libs jansson || echo "-ljansson")
CFLAGS += $(JANSSON_CFLAGS)
LDFLAGS += $(JANSSON_LIBS)

# GEOS Library Flags (using geos-config)
GEOS_CFLAGS = $(shell geos-config --cflags)
GEOS_LIBS = $(shell geos-config --libs)

CFLAGS += $(GEOS_CFLAGS)
LDFLAGS += $(GEOS_LIBS)

# Attempt to find OpenVINO
OPENVINO_DIR ?= $(INTEL_OPENVINO_DIR)
ifeq ($(OPENVINO_DIR),)
  # Try a common default path if INTEL_OPENVINO_DIR is not set
  OPENVINO_DIR_DEFAULT := /opt/intel/openvino_2023 # Or another common path
  ifeq ($(wildcard $(OPENVINO_DIR_DEFAULT)/setupvars.sh),)
    OPENVINO_DIR_DEFAULT2 := /opt/intel/openvino
    ifeq ($(wildcard $(OPENVINO_DIR_DEFAULT2)/setupvars.sh),)
      $(warning OpenVINO directory not found via INTEL_OPENVINO_DIR or default paths. OpenVINO plugin may fail to build.)
      OPENVINO_FOUND := 0
    else
      OPENVINO_DIR := $(OPENVINO_DIR_DEFAULT2)
      OPENVINO_FOUND := 1
    endif
  else
    OPENVINO_DIR := $(OPENVINO_DIR_DEFAULT)
    OPENVINO_FOUND := 1
  endif
else
  OPENVINO_FOUND := 1
endif

OPENVINO_INCLUDES :=
OPENVINO_LIBS :=
OPENVINO_RPATH :=

ifeq ($(OPENVINO_FOUND),1)
  # Adjust these paths based on actual OpenVINO structure
  # Common path for OpenVINO 2022.1+ for runtime includes
  OPENVINO_INCLUDE_PATH_RUNTIME := $(OPENVINO_DIR)/runtime/include
  # Fallback for older OpenVINO versions or different structures for dev tools
  OPENVINO_INCLUDE_PATH_DEV := $(OPENVINO_DIR)/development_tools/inference_engine/include

  ifeq ($(wildcard $(OPENVINO_INCLUDE_PATH_RUNTIME)/openvino/c/openvino.h),)
    ifeq ($(wildcard $(OPENVINO_INCLUDE_PATH_DEV)/openvino/c/openvino.h),) # Check for older C API path
        ifeq ($(wildcard $(OPENVINO_INCLUDE_PATH_DEV)/inference_engine.hpp),) # Check for older C++ API path as a proxy
            $(warning Could not find OpenVINO headers in expected locations: $(OPENVINO_INCLUDE_PATH_RUNTIME) or $(OPENVINO_INCLUDE_PATH_DEV))
        else
            OPENVINO_INCLUDES := -I$(OPENVINO_INCLUDE_PATH_DEV)
        endif
    else # Found C API in dev path
        OPENVINO_INCLUDES := -I$(OPENVINO_INCLUDE_PATH_DEV)
    endif
  else # Found C API in runtime path
    OPENVINO_INCLUDES := -I$(OPENVINO_INCLUDE_PATH_RUNTIME)
  endif

  # Common path for OpenVINO 2022.1+ for runtime libraries (adjust intel64 as needed)
  OPENVINO_LIB_PATH_RUNTIME := $(OPENVINO_DIR)/runtime/lib/intel64
  # Fallback for older OpenVINO versions
  OPENVINO_LIB_PATH_DEV := $(OPENVINO_DIR)/deployment_tools/inference_engine/lib/intel64

  # Check for actual library files to be more robust
  OPENVINO_LIB_PATH_ACTUAL :=
  ifeq ($(wildcard $(OPENVINO_LIB_PATH_RUNTIME)/libopenvino_c.so),) # Or .dylib on macOS
    ifeq ($(wildcard $(OPENVINO_LIB_PATH_DEV)/libopenvino_c.so),)
        $(warning Could not find OpenVINO libraries in expected locations: $(OPENVINO_LIB_PATH_RUNTIME) or $(OPENVINO_LIB_PATH_DEV))
    else
        OPENVINO_LIB_PATH_ACTUAL := $(OPENVINO_LIB_PATH_DEV)
    endif
  else
    OPENVINO_LIB_PATH_ACTUAL := $(OPENVINO_LIB_PATH_RUNTIME)
  endif

  ifneq ($(OPENVINO_LIB_PATH_ACTUAL),)
    OPENVINO_LIBS := -L$(OPENVINO_LIB_PATH_ACTUAL) -lopenvino_c -lopenvino
    # Ensure rpath is set if necessary, or that LD_LIBRARY_PATH is used at runtime
    OPENVINO_RPATH := -Wl,-rpath,$(OPENVINO_LIB_PATH_ACTUAL)
  endif

  # Add to general CFLAGS and LDFLAGS
  CFLAGS += $(OPENVINO_INCLUDES)
  LDFLAGS += $(OPENVINO_LIBS) $(OPENVINO_RPATH)
endif

# Directories
CORE_DIR = ./core
INCLUDE_DIR = ./include
PLUGIN_DIR = ./plugins
BUILD_DIR = ./build
TARGET_APP = $(BUILD_DIR)/spindex_app

# Core source files and object files
CORE_SRCS = $(wildcard $(CORE_DIR)/*.c) # Finds all .c files in core
CORE_SRCS := $(filter-out $(CORE_DIR)/test_%.c, $(CORE_SRCS))
CORE_OBJS = $(patsubst $(CORE_DIR)/%.c, $(BUILD_DIR)/core/%.o, $(CORE_SRCS))

# Test program sources (definitions as before)
# ... (all test program definitions remain unchanged) ...
TEST_CONFIG_LOADER_SRC = $(CORE_DIR)/test_config_manager.c
TEST_CONFIG_LOADER_DEPS = $(BUILD_DIR)/core/config_manager.o $(BUILD_DIR)/core/utils.o
TEST_CONFIG_LOADER_TARGET = $(BUILD_DIR)/test_config_manager

TEST_PLUGIN_LOADER_SRC = $(CORE_DIR)/test_plugin_loader.c
TEST_PLUGIN_LOADER_DEPS = $(BUILD_DIR)/core/plugin_loader.o $(BUILD_DIR)/core/config_manager.o $(BUILD_DIR)/core/utils.o
TEST_PLUGIN_LOADER_TARGET = $(BUILD_DIR)/test_plugin_loader

TEST_LOGGER_SRC = $(CORE_DIR)/test_logger.c
TEST_LOGGER_DEPS = $(BUILD_DIR)/core/utils.o $(BUILD_DIR)/core/config_manager.o
TEST_LOGGER_TARGET = $(BUILD_DIR)/test_logger

TEST_HW_DETECT_SRC = $(CORE_DIR)/test_hardware_detection.c
TEST_HW_DETECT_DEPS = $(BUILD_DIR)/core/hardware_detection.o
TEST_HW_DETECT_TARGET = $(BUILD_DIR)/test_hardware_detection

TEST_PIPELINE_SRC = $(CORE_DIR)/test_ingestion_pipeline.c
TEST_PIPELINE_DEPS = $(BUILD_DIR)/core/ingestion_pipeline.o $(BUILD_DIR)/core/plugin_loader.o \
                     $(BUILD_DIR)/core/config_manager.o $(BUILD_DIR)/core/utils.o $(BUILD_DIR)/core/hardware_detection.o \
                     $(BUILD_DIR)/core/threading.o $(AEGIS_MODULE_OBJS) plugins
TEST_PIPELINE_TARGET = $(BUILD_DIR)/test_ingestion_pipeline

TEST_ES_CLIENT_SRC = $(CORE_DIR)/test_elasticsearch_client.c
TEST_ES_CLIENT_DEPS = $(BUILD_DIR)/core/elasticsearch_client.o $(BUILD_DIR)/core/utils.o
TEST_ES_CLIENT_TARGET = $(BUILD_DIR)/test_elasticsearch_client

TEST_ES_INDEXING_SRC = $(CORE_DIR)/test_elasticsearch_indexing.c
TEST_ES_INDEXING_DEPS = \
    $(BUILD_DIR)/core/config_manager.o \
    $(BUILD_DIR)/core/ingestion_pipeline.o \
    $(BUILD_DIR)/core/plugin_loader.o \
    $(BUILD_DIR)/core/utils.o \
    $(BUILD_DIR)/core/elasticsearch_client.o \
    $(BUILD_DIR)/core/threading.o \
    $(AEGIS_MODULE_OBJS) \
    plugins
TEST_ES_INDEXING_TARGET = $(BUILD_DIR)/test_elasticsearch_indexing
TEST_ES_INDEXING_LINK_OBJS = $(filter %.o,$(TEST_ES_INDEXING_DEPS))

TEST_DOC_PARSER_SRC = $(PLUGIN_DIR)/parsers/test_doc_parser.c
TEST_DOC_PARSER_DEPS = $(DOC_PARSER_PLUGIN_OBJ) $(BUILD_DIR)/core/utils.o
TEST_DOC_PARSER_TARGET = $(BUILD_DIR)/test_doc_parser

TEST_DOC_PREPROCESSOR_SRC = $(PLUGIN_DIR)/preprocessors/test_doc_preprocessor.c
TEST_DOC_PREPROCESSOR_DEPS = $(DOC_PREPROCESSOR_PLUGIN_OBJ) $(BUILD_DIR)/core/utils.o
TEST_DOC_PREPROCESSOR_TARGET = $(BUILD_DIR)/test_doc_preprocessor

TEST_XLS_PARSER_SRC = $(PLUGIN_DIR)/parsers/test_xls_parser.c
TEST_XLS_PARSER_DEPS = $(XLS_PARSER_PLUGIN_OBJ) $(BUILD_DIR)/core/utils.o
TEST_XLS_PARSER_TARGET = $(BUILD_DIR)/test_xls_parser

TEST_XLS_PREPROCESSOR_SRC = $(PLUGIN_DIR)/preprocessors/test_xls_preprocessor.c
TEST_XLS_PREPROCESSOR_DEPS = $(XLS_PREPROCESSOR_PLUGIN_OBJ) $(BUILD_DIR)/core/utils.o
TEST_XLS_PREPROCESSOR_TARGET = $(BUILD_DIR)/test_xls_preprocessor

TEST_PDF_PARSER_SRC = $(PLUGIN_DIR)/parsers/test_pdf_parser.c
TEST_PDF_PARSER_DEPS = $(PDF_PARSER_PLUGIN_OBJ) $(BUILD_DIR)/core/utils.o
TEST_PDF_PARSER_TARGET = $(BUILD_DIR)/test_pdf_parser

TEST_PDF_PREPROCESSOR_SRC = $(PLUGIN_DIR)/preprocessors/test_pdf_preprocessor.c
TEST_PDF_PREPROCESSOR_DEPS = $(PDF_PREPROCESSOR_PLUGIN_OBJ) $(BUILD_DIR)/core/utils.o
TEST_PDF_PREPROCESSOR_TARGET = $(BUILD_DIR)/test_pdf_preprocessor

TEST_RTF_PARSER_SRC = $(PLUGIN_DIR)/parsers/test_rtf_parser.c
TEST_RTF_PARSER_DEPS = $(RTF_PARSER_PLUGIN_OBJ) $(BUILD_DIR)/core/utils.o
TEST_RTF_PARSER_TARGET = $(BUILD_DIR)/test_rtf_parser

TEST_RTF_PREPROCESSOR_SRC = $(PLUGIN_DIR)/preprocessors/test_rtf_preprocessor.c
TEST_RTF_PREPROCESSOR_DEPS = $(RTF_PREPROCESSOR_PLUGIN_OBJ) $(BUILD_DIR)/core/utils.o
TEST_RTF_PREPROCESSOR_TARGET = $(BUILD_DIR)/test_rtf_preprocessor

TEST_DIC_PARSER_SRC = $(PLUGIN_DIR)/parsers/test_dic_parser.c
TEST_DIC_PARSER_DEPS = $(DIC_PARSER_PLUGIN_OBJ) $(BUILD_DIR)/core/utils.o
TEST_DIC_PARSER_TARGET = $(BUILD_DIR)/test_dic_parser

TEST_DIC_PREPROCESSOR_SRC = $(PLUGIN_DIR)/preprocessors/test_dic_preprocessor.c
TEST_DIC_PREPROCESSOR_DEPS = $(DIC_PREPROCESSOR_PLUGIN_OBJ) $(BUILD_DIR)/core/utils.o
TEST_DIC_PREPROCESSOR_TARGET = $(BUILD_DIR)/test_dic_preprocessor

TEST_SQLITE_PARSER_SRC = $(PLUGIN_DIR)/parsers/test_sqlite_parser.c
TEST_SQLITE_PARSER_DEPS = $(SQLITE_PARSER_PLUGIN_OBJ) $(BUILD_DIR)/core/utils.o
TEST_SQLITE_PARSER_TARGET = $(BUILD_DIR)/test_sqlite_parser

TEST_SQLITE_PREPROCESSOR_SRC = $(PLUGIN_DIR)/preprocessors/test_sqlite_preprocessor.c
TEST_SQLITE_PREPROCESSOR_DEPS = $(SQLITE_PREPROCESSOR_PLUGIN_OBJ) $(BUILD_DIR)/core/utils.o
TEST_SQLITE_PREPROCESSOR_TARGET = $(BUILD_DIR)/test_sqlite_preprocessor

TEST_COMPLEX_GEOMETRY_SRC = $(CORE_DIR)/test_complex_geometry_handler.c
TEST_COMPLEX_GEOMETRY_DEPS_OBJS = $(COMPLEX_GEOMETRY_HANDLER_OBJ) $(BUILD_DIR)/core/config_manager.o $(BUILD_DIR)/core/utils.o
TEST_COMPLEX_GEOMETRY_TARGET = $(BUILD_DIR)/test_complex_geometry_handler

TEST_NLP_NER_PARSER_SRC = $(CORE_DIR)/test_nlp_ner_parser.c
# Dependencies: the nlp_ner_parser.o itself, config_manager.o, utils.o, and jansson for the test's own parsing if any (though nlp_ner_parser.o links jansson)
TEST_NLP_NER_PARSER_DEPS_OBJS = $(BUILD_DIR)/plugins/aegis_modules/nlp_ner_parser.o $(BUILD_DIR)/core/config_manager.o $(BUILD_DIR)/core/utils.o
TEST_NLP_NER_PARSER_TARGET = $(BUILD_DIR)/test_nlp_ner_parser

TEST_GEOMETRIC_VALIDATOR_SRC = $(CORE_DIR)/test_geometric_validator.c
# Dependencies: geometric_validator.o, config_manager.o, utils.o.
# Jansson and GEOS libs are linked via global LDFLAGS.
TEST_GEOMETRIC_VALIDATOR_DEPS_OBJS = \
    $(BUILD_DIR)/plugins/aegis_modules/geometric_validator.o \
    $(BUILD_DIR)/core/config_manager.o \
    $(BUILD_DIR)/core/utils.o
TEST_GEOMETRIC_VALIDATOR_TARGET = $(BUILD_DIR)/test_geometric_validator

# AVX Search Test Program
TEST_AVX_SEARCH_SRC = $(PLUGIN_DIR)/search_algorithms/test_avx512_phrase_search.c
# Dependencies: the plugin object itself, core utils, and core hardware detection
TEST_AVX_SEARCH_DEPS_OBJS = $(AVX512_PLUGIN_OBJ) $(BUILD_DIR)/core/utils.o $(BUILD_DIR)/core/hardware_detection.o
TEST_AVX_SEARCH_TARGET = $(BUILD_DIR)/test_avx512_phrase_search

# Test Harness for OpenVINO Hardware-Specific Validation (Manual Execution)
TEST_OPENVINO_HW_SRC = ./tests/test_openvino_classifier_hw.c
TEST_OPENVINO_HW_TARGET = $(BUILD_DIR)/test_openvino_classifier_hw


# Directory for staging plugins for tests
TEST_PLUGINS_STAGING_DIR = $(BUILD_DIR)/plugins_for_test
PLUGIN_OBJ_DIR = $(BUILD_DIR)/plugins/obj
PLUGIN_LIB_DIR = $(BUILD_DIR)/plugins/lib

# AEGIS Module Definitions
AEGIS_MODULES_DIR = $(PLUGIN_DIR)/aegis_modules
# Add other AEGIS module objects here if they are created later
AEGIS_MODULE_OBJS = \
    $(BUILD_DIR)/plugins/aegis_modules/complex_geometry_handler.o \
    $(BUILD_DIR)/plugins/aegis_modules/nlp_ner_parser.o \
    $(BUILD_DIR)/plugins/aegis_modules/geometric_validator.o \
    $(BUILD_DIR)/plugins/aegis_modules/geojson_processor.o

# Plugin definitions (Enabled ones first)
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

CONTENT_ANALYZER_PLUGIN_SRC_DIR = $(PLUGIN_DIR)/analyzers
CONTENT_ANALYZER_PLUGIN_SRC = $(CONTENT_ANALYZER_PLUGIN_SRC_DIR)/content_analyzer.c
CONTENT_ANALYZER_PLUGIN_OBJ = $(PLUGIN_OBJ_DIR)/analyzers/content_analyzer.o
CONTENT_ANALYZER_PLUGIN_SO = $(PLUGIN_LIB_DIR)/content_analyzer.so

DOC_PARSER_PLUGIN_SRC_DIR = $(PLUGIN_DIR)/parsers
DOC_PARSER_PLUGIN_SRC = $(DOC_PARSER_PLUGIN_SRC_DIR)/doc_parser.c
DOC_PARSER_PLUGIN_OBJ = $(PLUGIN_OBJ_DIR)/parsers/doc_parser.o
DOC_PARSER_PLUGIN_SO = $(PLUGIN_LIB_DIR)/doc_parser.so

DOC_PREPROCESSOR_PLUGIN_SRC_DIR = $(PLUGIN_DIR)/preprocessors
DOC_PREPROCESSOR_PLUGIN_SRC = $(DOC_PREPROCESSOR_PLUGIN_SRC_DIR)/doc_preprocessor.c
DOC_PREPROCESSOR_PLUGIN_OBJ = $(PLUGIN_OBJ_DIR)/preprocessors/doc_preprocessor.o
DOC_PREPROCESSOR_PLUGIN_SO = $(PLUGIN_LIB_DIR)/doc_preprocessor.so

XLS_PARSER_PLUGIN_SRC_DIR = $(PLUGIN_DIR)/parsers
XLS_PARSER_PLUGIN_SRC = $(XLS_PARSER_PLUGIN_SRC_DIR)/xls_parser.c
XLS_PARSER_PLUGIN_OBJ = $(PLUGIN_OBJ_DIR)/parsers/xls_parser.o
XLS_PARSER_PLUGIN_SO = $(PLUGIN_LIB_DIR)/xls_parser.so

XLS_PREPROCESSOR_PLUGIN_SRC_DIR = $(PLUGIN_DIR)/preprocessors
XLS_PREPROCESSOR_PLUGIN_SRC = $(XLS_PREPROCESSOR_PLUGIN_SRC_DIR)/xls_preprocessor.c
XLS_PREPROCESSOR_PLUGIN_OBJ = $(PLUGIN_OBJ_DIR)/preprocessors/xls_preprocessor.o
XLS_PREPROCESSOR_PLUGIN_SO = $(PLUGIN_LIB_DIR)/xls_preprocessor.so

PDF_PARSER_PLUGIN_SRC_DIR = $(PLUGIN_DIR)/parsers
PDF_PARSER_PLUGIN_SRC = $(PDF_PARSER_PLUGIN_SRC_DIR)/pdf_parser.c
PDF_PARSER_PLUGIN_OBJ = $(PLUGIN_OBJ_DIR)/parsers/pdf_parser.o
PDF_PARSER_PLUGIN_SO = $(PLUGIN_LIB_DIR)/pdf_parser.so

PDF_PREPROCESSOR_PLUGIN_SRC_DIR = $(PLUGIN_DIR)/preprocessors
PDF_PREPROCESSOR_PLUGIN_SRC = $(PDF_PREPROCESSOR_PLUGIN_SRC_DIR)/pdf_preprocessor.c
PDF_PREPROCESSOR_PLUGIN_OBJ = $(PLUGIN_OBJ_DIR)/preprocessors/pdf_preprocessor.o
PDF_PREPROCESSOR_PLUGIN_SO = $(PLUGIN_LIB_DIR)/pdf_preprocessor.so

RTF_PARSER_PLUGIN_SRC_DIR = $(PLUGIN_DIR)/parsers
RTF_PARSER_PLUGIN_SRC = $(RTF_PARSER_PLUGIN_SRC_DIR)/rtf_parser.c
RTF_PARSER_PLUGIN_OBJ = $(PLUGIN_OBJ_DIR)/parsers/rtf_parser.o
RTF_PARSER_PLUGIN_SO = $(PLUGIN_LIB_DIR)/rtf_parser.so

RTF_PREPROCESSOR_PLUGIN_SRC_DIR = $(PLUGIN_DIR)/preprocessors
RTF_PREPROCESSOR_PLUGIN_SRC = $(RTF_PREPROCESSOR_PLUGIN_SRC_DIR)/rtf_preprocessor.c
RTF_PREPROCESSOR_PLUGIN_OBJ = $(PLUGIN_OBJ_DIR)/preprocessors/rtf_preprocessor.o
RTF_PREPROCESSOR_PLUGIN_SO = $(PLUGIN_LIB_DIR)/rtf_preprocessor.so

DIC_PARSER_PLUGIN_SRC_DIR = $(PLUGIN_DIR)/parsers
DIC_PARSER_PLUGIN_SRC = $(DIC_PARSER_PLUGIN_SRC_DIR)/dic_parser.c
DIC_PARSER_PLUGIN_OBJ = $(PLUGIN_OBJ_DIR)/parsers/dic_parser.o
DIC_PARSER_PLUGIN_SO = $(PLUGIN_LIB_DIR)/dic_parser.so

DIC_PREPROCESSOR_PLUGIN_SRC_DIR = $(PLUGIN_DIR)/preprocessors
DIC_PREPROCESSOR_PLUGIN_SRC = $(DIC_PREPROCESSOR_PLUGIN_SRC_DIR)/dic_preprocessor.c
DIC_PREPROCESSOR_PLUGIN_OBJ = $(PLUGIN_OBJ_DIR)/preprocessors/dic_preprocessor.o
DIC_PREPROCESSOR_PLUGIN_SO = $(PLUGIN_LIB_DIR)/dic_preprocessor.so

SQLITE_PARSER_PLUGIN_SRC_DIR = $(PLUGIN_DIR)/parsers
SQLITE_PARSER_PLUGIN_SRC = $(SQLITE_PARSER_PLUGIN_SRC_DIR)/sqlite_parser.c
SQLITE_PARSER_PLUGIN_OBJ = $(PLUGIN_OBJ_DIR)/parsers/sqlite_parser.o
SQLITE_PARSER_PLUGIN_SO = $(PLUGIN_LIB_DIR)/sqlite_parser.so

SQLITE_PREPROCESSOR_PLUGIN_SRC_DIR = $(PLUGIN_DIR)/preprocessors
SQLITE_PREPROCESSOR_PLUGIN_SRC = $(SQLITE_PREPROCESSOR_PLUGIN_SRC_DIR)/sqlite_preprocessor.c
SQLITE_PREPROCESSOR_PLUGIN_OBJ = $(PLUGIN_OBJ_DIR)/preprocessors/sqlite_preprocessor.o
SQLITE_PREPROCESSOR_PLUGIN_SO = $(PLUGIN_LIB_DIR)/sqlite_preprocessor.so

OPENVINO_CLASSIFIER_PLUGIN_SRC_DIR = $(PLUGIN_DIR)/classifiers
OPENVINO_CLASSIFIER_PLUGIN_SRC = $(OPENVINO_CLASSIFIER_PLUGIN_SRC_DIR)/openvino_classifier.c
OPENVINO_CLASSIFIER_PLUGIN_OBJ = $(PLUGIN_OBJ_DIR)/classifiers/openvino_classifier.o
OPENVINO_CLASSIFIER_PLUGIN_SO = $(PLUGIN_LIB_DIR)/openvino_classifier.so

SEARCH_ALGORITHMS_PLUGIN_SRC_DIR = $(PLUGIN_DIR)/search_algorithms
AVX512_PLUGIN_SRC = $(SEARCH_ALGORITHMS_PLUGIN_SRC_DIR)/avx512_phrase_search.c
AVX512_PLUGIN_OBJ = $(PLUGIN_OBJ_DIR)/search_algorithms/avx512_phrase_search.o
AVX512_PLUGIN_SO = $(PLUGIN_LIB_DIR)/avx512_phrase_search.so
AVX_CFLAGS = -mavx512f -mavx512bw # Specific CFLAGS for AVX-512 plugin

# Disabled Plugin Definitions
DECOMPRESSORS_PLUGIN_SRC_DIR = $(PLUGIN_DIR)/decompressors
# ZIP_DECOMPRESSOR_PLUGIN_SRC = $(DECOMPRESSORS_PLUGIN_SRC_DIR)/zip_decompressor.c
# ... (other disabled plugins commented out) ...

# ML_CLASSIFIER_PLUGIN_SRC_DIR = $(PLUGIN_DIR)/classifiers
# ML_CLASSIFIER_PLUGIN_SRC = $(ML_CLASSIFIER_PLUGIN_SRC_DIR)/ml_classifier.c
# ...

# Phony targets
.PHONY: all clean plugins tests

all: $(TARGET_APP) plugins # Removed tests from 'all' for now, can be added back if AEGIS modules are tested through 'tests'
# Consider adding a specific target for AEGIS module related tests if needed

# Rule to build the main application
$(TARGET_APP): $(CORE_OBJS) $(AEGIS_MODULE_OBJS)
	@mkdir -p $(@D)
	$(LD) $(CFLAGS) $(INCLUDE_DIRS) $(CORE_OBJS) $(AEGIS_MODULE_OBJS) -o $@ $(LDFLAGS)
	@echo "Built main application: $@"

# Rule to compile core C files to object files
$(BUILD_DIR)/core/%.o: $(CORE_DIR)/%.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(INCLUDE_DIRS) -c $< -o $@
	@echo "Compiled core object: $@"

# Rule to compile AEGIS module C files to object files
$(BUILD_DIR)/plugins/aegis_modules/%.o: $(AEGIS_MODULES_DIR)/%.c $(INCLUDE_DIR)/config_manager.h $(INCLUDE_DIR)/data_types.h $(INCLUDE_DIR)/utils.h
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(INCLUDE_DIRS) -I$(AEGIS_MODULES_DIR) -c $< -o $@
	@echo "Compiled AEGIS module object: $@"

# Specific rule for elasticsearch_client.o if it has unique dependencies not covered by pattern
$(BUILD_DIR)/core/elasticsearch_client.o: $(CORE_DIR)/elasticsearch_client.c $(INCLUDE_DIR)/elasticsearch_client.h $(INCLUDE_DIR)/utils.h
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(INCLUDE_DIRS) -c $(CORE_DIR)/elasticsearch_client.c -o $@
	@echo "Compiled core object (specific rule for es_client): $@"

# --- Test Program Rules ---
# ... (test rules remain largely unchanged, but ensure they link against necessary objects) ...
tests: $(TEST_CONFIG_LOADER_TARGET) $(TEST_PLUGIN_LOADER_TARGET) $(TEST_LOGGER_TARGET) $(TEST_HW_DETECT_TARGET) $(TEST_PIPELINE_TARGET) $(TEST_ES_CLIENT_TARGET) $(TEST_ES_INDEXING_TARGET) $(TEST_DOC_PARSER_TARGET) $(TEST_DOC_PREPROCESSOR_TARGET) $(TEST_XLS_PARSER_TARGET) $(TEST_XLS_PREPROCESSOR_TARGET) $(TEST_PDF_PARSER_TARGET) $(TEST_PDF_PREPROCESSOR_TARGET) $(TEST_RTF_PARSER_TARGET) $(TEST_RTF_PREPROCESSOR_TARGET) $(TEST_DIC_PARSER_TARGET) $(TEST_DIC_PREPROCESSOR_TARGET) $(TEST_SQLITE_PARSER_TARGET) $(TEST_SQLITE_PREPROCESSOR_TARGET) $(TEST_AVX_SEARCH_TARGET) $(TEST_COMPLEX_GEOMETRY_TARGET) $(TEST_NLP_NER_PARSER_TARGET) $(TEST_GEOMETRIC_VALIDATOR_TARGET)

$(TEST_CONFIG_LOADER_TARGET): $(TEST_CONFIG_LOADER_SRC) $(TEST_CONFIG_LOADER_DEPS)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(INCLUDE_DIRS) $^ -o $@ $(LDFLAGS)
	@echo "Built test program: $@"

$(TEST_AVX_SEARCH_TARGET): $(TEST_AVX_SEARCH_SRC) $(TEST_AVX_SEARCH_DEPS_OBJS)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(INCLUDE_DIRS) $(TEST_AVX_SEARCH_SRC) $(TEST_AVX_SEARCH_DEPS_OBJS) -o $@ $(LDFLAGS) -lm # Add -lm if any math functions are used, good practice
	@echo "Built test program: $@"

$(TEST_COMPLEX_GEOMETRY_TARGET): $(TEST_COMPLEX_GEOMETRY_SRC) $(TEST_COMPLEX_GEOMETRY_DEPS_OBJS)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(INCLUDE_DIRS) -I$(AEGIS_MODULES_DIR) $(TEST_COMPLEX_GEOMETRY_SRC) $(TEST_COMPLEX_GEOMETRY_DEPS_OBJS) -o $@ $(LDFLAGS)
	@echo "Built test program: $@"

$(TEST_NLP_NER_PARSER_TARGET): $(TEST_NLP_NER_PARSER_SRC) $(TEST_NLP_NER_PARSER_DEPS_OBJS) $(AEGIS_MODULES_DIR)/nlp_ner_parser.h
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(INCLUDE_DIRS) -I$(AEGIS_MODULES_DIR) $(TEST_NLP_NER_PARSER_SRC) $(TEST_NLP_NER_PARSER_DEPS_OBJS) -o $@ $(LDFLAGS)
	@echo "Built test program: $@"

$(TEST_GEOMETRIC_VALIDATOR_TARGET): $(TEST_GEOMETRIC_VALIDATOR_SRC) $(TEST_GEOMETRIC_VALIDATOR_DEPS_OBJS) $(AEGIS_MODULES_DIR)/geometric_validator.h
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(INCLUDE_DIRS) -I$(AEGIS_MODULES_DIR) $(TEST_GEOMETRIC_VALIDATOR_SRC) $(TEST_GEOMETRIC_VALIDATOR_DEPS_OBJS) -o $@ $(LDFLAGS)
	@echo "Built test program: $@"

# Rule to build the OpenVINO hardware test harness
$(TEST_OPENVINO_HW_TARGET): $(TEST_OPENVINO_HW_SRC) # Does not depend on plugin .so, as it loads it dynamically
	@mkdir -p $(@D)
	# CFLAGS includes general flags and OpenVINO includes.
	# LDFLAGS includes general flags and OpenVINO libs/rpath. We add -ldl for dynamic loading.
	$(CC) $(CFLAGS) $(INCLUDE_DIRS) $(TEST_OPENVINO_HW_SRC) -o $@ $(LDFLAGS) -ldl
	@echo "Built OpenVINO Hardware Test Harness: $@ (Run manually on target hardware)"

# ... (similar updates for other test targets if their dependencies changed, ensure linking core objects)

$(TEST_ES_INDEXING_TARGET): $(TEST_ES_INDEXING_SRC) $(TEST_ES_INDEXING_LINK_OBJS) plugins
	@mkdir -p $(@D)
	@mkdir -p $(TEST_PLUGINS_STAGING_DIR)
	# Copy all .so files from PLUGIN_LIB_DIR to TEST_PLUGINS_STAGING_DIR
	cp $(PLUGIN_LIB_DIR)/*.so $(TEST_PLUGINS_STAGING_DIR)/ || true # Allow if no .so files yet for some
	$(CC) $(CFLAGS) $(INCLUDE_DIRS) $(TEST_ES_INDEXING_SRC) $(TEST_ES_INDEXING_LINK_OBJS) -o $@ $(LDFLAGS)
	@echo "Built test program: $@"
	@echo "Staged plugins for test in $(TEST_PLUGINS_STAGING_DIR)"

$(TEST_PIPELINE_TARGET): $(TEST_PIPELINE_SRC) $(filter %.o,$(TEST_PIPELINE_DEPS)) plugins
	@mkdir -p $(@D)
	@mkdir -p $(TEST_PLUGINS_STAGING_DIR)
	cp $(PLUGIN_LIB_DIR)/*.so $(TEST_PLUGINS_STAGING_DIR)/ || true
	$(CC) $(CFLAGS) $(INCLUDE_DIRS) $(TEST_PIPELINE_SRC) \
		$(filter %.o,$(TEST_PIPELINE_DEPS)) -o $@ $(LDFLAGS)
	@echo "Built test program: $@"
	@echo "Staged plugins for test in $(TEST_PLUGINS_STAGING_DIR)"

# --- Plugin Rules ---
plugins: $(DUMMY_PLUGIN_SO) \
         $(JSON_PREPROCESSOR_PLUGIN_SO) $(JSON_PARSER_PLUGIN_SO) \
         $(ELASTICSEARCH_INDEXER_PLUGIN_SO) \
         $(TXT_PREPROCESSOR_PLUGIN_SO) $(TXT_PARSER_PLUGIN_SO) \
         $(CSV_PREPROCESSOR_PLUGIN_SO) $(CSV_PARSER_PLUGIN_SO) \
         $(CONTENT_ANALYZER_PLUGIN_SO) \
         $(DOC_PARSER_PLUGIN_SO) $(DOC_PREPROCESSOR_PLUGIN_SO) \
         $(XLS_PARSER_PLUGIN_SO) $(XLS_PREPROCESSOR_PLUGIN_SO) \
         $(PDF_PARSER_PLUGIN_SO) $(PDF_PREPROCESSOR_PLUGIN_SO) \
         $(RTF_PARSER_PLUGIN_SO) $(RTF_PREPROCESSOR_PLUGIN_SO) \
         $(DIC_PARSER_PLUGIN_SO) $(DIC_PREPROCESSOR_PLUGIN_SO) \
         $(SQLITE_PARSER_PLUGIN_SO) $(SQLITE_PREPROCESSOR_PLUGIN_SO) \
         $(OPENVINO_CLASSIFIER_PLUGIN_SO) \
         $(AVX512_PLUGIN_SO) # Added AVX-512 plugin to the list
	@echo "All specified plugins built."

# Generic Plugin Object File Rule (if applicable, or use specific ones below)
$(PLUGIN_OBJ_DIR)/%.o: $(PLUGIN_DIR)/%.c $(INCLUDE_DIR)/plugin_interface.h $(INCLUDE_DIR)/data_types.h
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(INCLUDE_DIRS) -c $< -o $@
	@echo "Compiled plugin object (generic rule): $@"

# Specific Plugin Object File Rules (adjust paths and dependencies as needed)
$(DUMMY_PLUGIN_OBJ): $(DUMMY_PLUGIN_SRC); @mkdir -p $(@D); $(CC) $(CFLAGS) $(INCLUDE_DIRS) -c $< -o $@; @echo "Compiled $@"
$(JSON_PREPROCESSOR_PLUGIN_OBJ): $(JSON_PREPROCESSOR_PLUGIN_SRC); @mkdir -p $(@D); $(CC) $(CFLAGS) $(INCLUDE_DIRS) -c $< -o $@; @echo "Compiled $@"
$(JSON_PARSER_PLUGIN_OBJ): $(JSON_PARSER_PLUGIN_SRC); @mkdir -p $(@D); $(CC) $(CFLAGS) $(INCLUDE_DIRS) -c $< -o $@; @echo "Compiled $@"
$(ELASTICSEARCH_INDEXER_PLUGIN_OBJ): $(ELASTICSEARCH_INDEXER_PLUGIN_SRC) $(BUILD_DIR)/core/elasticsearch_client.o; @mkdir -p $(@D); $(CC) $(CFLAGS) $(INCLUDE_DIRS) -c $< -o $@; @echo "Compiled $@"
$(TXT_PREPROCESSOR_PLUGIN_OBJ): $(TXT_PREPROCESSOR_PLUGIN_SRC); @mkdir -p $(@D); $(CC) $(CFLAGS) $(INCLUDE_DIRS) -c $< -o $@; @echo "Compiled $@"
$(TXT_PARSER_PLUGIN_OBJ): $(TXT_PARSER_PLUGIN_SRC); @mkdir -p $(@D); $(CC) $(CFLAGS) $(INCLUDE_DIRS) -c $< -o $@; @echo "Compiled $@"
$(CSV_PREPROCESSOR_PLUGIN_OBJ): $(CSV_PREPROCESSOR_PLUGIN_SRC); @mkdir -p $(@D); $(CC) $(CFLAGS) $(INCLUDE_DIRS) -c $< -o $@; @echo "Compiled $@"
$(CSV_PARSER_PLUGIN_OBJ): $(CSV_PARSER_PLUGIN_SRC); @mkdir -p $(@D); $(CC) $(CFLAGS) $(INCLUDE_DIRS) -c $< -o $@; @echo "Compiled $@"
$(CONTENT_ANALYZER_PLUGIN_OBJ): $(CONTENT_ANALYZER_PLUGIN_SRC); @mkdir -p $(@D); $(CC) $(CFLAGS) $(INCLUDE_DIRS) -c $< -o $@; @echo "Compiled $@"
$(DOC_PARSER_PLUGIN_OBJ): $(DOC_PARSER_PLUGIN_SRC); @mkdir -p $(@D); $(CC) $(CFLAGS) $(INCLUDE_DIRS) -c $< -o $@; @echo "Compiled $@"
$(DOC_PREPROCESSOR_PLUGIN_OBJ): $(DOC_PREPROCESSOR_PLUGIN_SRC); @mkdir -p $(@D); $(CC) $(CFLAGS) $(INCLUDE_DIRS) -c $< -o $@; @echo "Compiled $@"
$(XLS_PARSER_PLUGIN_OBJ): $(XLS_PARSER_PLUGIN_SRC); @mkdir -p $(@D); $(CC) $(CFLAGS) $(INCLUDE_DIRS) -c $< -o $@; @echo "Compiled $@"
$(XLS_PREPROCESSOR_PLUGIN_OBJ): $(XLS_PREPROCESSOR_PLUGIN_SRC); @mkdir -p $(@D); $(CC) $(CFLAGS) $(INCLUDE_DIRS) -c $< -o $@; @echo "Compiled $@"
$(PDF_PARSER_PLUGIN_OBJ): $(PDF_PARSER_PLUGIN_SRC); @mkdir -p $(@D); $(CC) $(CFLAGS) $(INCLUDE_DIRS) -c $< -o $@; @echo "Compiled $@"
$(PDF_PREPROCESSOR_PLUGIN_OBJ): $(PDF_PREPROCESSOR_PLUGIN_SRC); @mkdir -p $(@D); $(CC) $(CFLAGS) $(INCLUDE_DIRS) -c $< -o $@; @echo "Compiled $@"
$(RTF_PARSER_PLUGIN_OBJ): $(RTF_PARSER_PLUGIN_SRC); @mkdir -p $(@D); $(CC) $(CFLAGS) $(INCLUDE_DIRS) -c $< -o $@; @echo "Compiled $@"
$(RTF_PREPROCESSOR_PLUGIN_OBJ): $(RTF_PREPROCESSOR_PLUGIN_SRC); @mkdir -p $(@D); $(CC) $(CFLAGS) $(INCLUDE_DIRS) -c $< -o $@; @echo "Compiled $@"
$(DIC_PARSER_PLUGIN_OBJ): $(DIC_PARSER_PLUGIN_SRC); @mkdir -p $(@D); $(CC) $(CFLAGS) $(INCLUDE_DIRS) -c $< -o $@; @echo "Compiled $@"
$(DIC_PREPROCESSOR_PLUGIN_OBJ): $(DIC_PREPROCESSOR_PLUGIN_SRC); @mkdir -p $(@D); $(CC) $(CFLAGS) $(INCLUDE_DIRS) -c $< -o $@; @echo "Compiled $@"
$(SQLITE_PARSER_PLUGIN_OBJ): $(SQLITE_PARSER_PLUGIN_SRC); @mkdir -p $(@D); $(CC) $(CFLAGS) $(INCLUDE_DIRS) -c $< -o $@; @echo "Compiled $@"
$(SQLITE_PREPROCESSOR_PLUGIN_OBJ): $(SQLITE_PREPROCESSOR_PLUGIN_SRC); @mkdir -p $(@D); $(CC) $(CFLAGS) $(INCLUDE_DIRS) -c $< -o $@; @echo "Compiled $@"

$(OPENVINO_CLASSIFIER_PLUGIN_OBJ): $(OPENVINO_CLASSIFIER_PLUGIN_SRC) $(PLUGIN_DIR)/classifiers/openvino_classifier.h $(INCLUDE_DIR)/ml_interface.h $(INCLUDE_DIR)/data_types.h
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(INCLUDE_DIRS) -c $< -o $@
	@echo "Compiled plugin object: $@"

# AVX-512 Plugin Object File Rule
$(AVX512_PLUGIN_OBJ): $(AVX512_PLUGIN_SRC) $(INCLUDE_DIR)/plugin_interface.h $(INCLUDE_DIR)/data_types.h $(INCLUDE_DIR)/utils.h $(INCLUDE_DIR)/hardware_detection.h $(INCLUDE_DIR)/avx_search_params.h
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(AVX_CFLAGS) $(INCLUDE_DIRS) -c $< -o $@
	@echo "Compiled AVX-512 plugin object: $@"


# Plugin Shared Object (.so) Rules (Generic, then specific overrides if needed)
$(PLUGIN_LIB_DIR)/%.so: $(PLUGIN_OBJ_DIR)/%.o
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -shared $^ -o $@ $(LDFLAGS)
	@echo "Built plugin (generic .so rule): $@"

# Specific .so rule for elasticsearch_indexer if it needs special linking (e.g. core objects)
$(ELASTICSEARCH_INDEXER_PLUGIN_SO): $(ELASTICSEARCH_INDEXER_PLUGIN_OBJ) $(BUILD_DIR)/core/elasticsearch_client.o $(BUILD_DIR)/core/utils.o
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -shared $^ -o $@ $(LDFLAGS)
	@echo "Built plugin (specific .so rule for elasticsearch_indexer): $@"

# Specific .so rule for AVX-512 plugin if it needs special linking (AVX_CFLAGS already applied to .o)
$(AVX512_PLUGIN_SO): $(AVX512_PLUGIN_OBJ) $(BUILD_DIR)/core/hardware_detection.o $(BUILD_DIR)/core/utils.o
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -shared $^ -o $@ $(LDFLAGS)
	@echo "Built plugin (specific .so rule for avx512_phrase_search): $@"

$(OPENVINO_CLASSIFIER_PLUGIN_SO): $(OPENVINO_CLASSIFIER_PLUGIN_OBJ) $(BUILD_DIR)/core/utils.o
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -shared $^ -o $@ $(LDFLAGS) # OPENVINO_LIBS and RPATH are already in LDFLAGS
	@echo "Built plugin: $@"


clean:
	@echo "Cleaning build artifacts..."
	rm -rf $(BUILD_DIR)
	# Optionally remove all .so files from plugin lib directory
	# rm -f $(PLUGIN_LIB_DIR)/*.so
	@echo "Clean complete."
