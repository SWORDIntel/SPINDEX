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

# Test program sources
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
                     $(BUILD_DIR)/core/threading.o plugins
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
    plugins
TEST_ES_INDEXING_TARGET = $(BUILD_DIR)/test_elasticsearch_indexing
TEST_ES_INDEXING_LINK_OBJS = $(filter %.o,$(TEST_ES_INDEXING_DEPS))

TEST_DOC_PARSER_SRC = $(PLUGIN_DIR)/parsers/test_doc_parser.c
TEST_DOC_PARSER_DEPS = $(DOC_PARSER_PLUGIN_OBJ) $(BUILD_DIR)/core/utils.o
TEST_DOC_PARSER_TARGET = $(BUILD_DIR)/test_doc_parser

TEST_DOC_PREPROCESSOR_SRC = $(PLUGIN_DIR)/preprocessors/test_doc_preprocessor.c
TEST_DOC_PREPROCESSOR_DEPS = $(DOC_PREPROCESSOR_PLUGIN_OBJ) $(BUILD_DIR)/core/utils.o
TEST_DOC_PREPROCESSOR_TARGET = $(BUILD_DIR)/test_doc_preprocessor

# Test XLS Parser
TEST_XLS_PARSER_SRC = $(PLUGIN_DIR)/parsers/test_xls_parser.c
TEST_XLS_PARSER_DEPS = $(XLS_PARSER_PLUGIN_OBJ) $(BUILD_DIR)/core/utils.o
TEST_XLS_PARSER_TARGET = $(BUILD_DIR)/test_xls_parser

# Test XLS Preprocessor
TEST_XLS_PREPROCESSOR_SRC = $(PLUGIN_DIR)/preprocessors/test_xls_preprocessor.c
TEST_XLS_PREPROCESSOR_DEPS = $(XLS_PREPROCESSOR_PLUGIN_OBJ) $(BUILD_DIR)/core/utils.o
TEST_XLS_PREPROCESSOR_TARGET = $(BUILD_DIR)/test_xls_preprocessor

# Test PDF Parser
TEST_PDF_PARSER_SRC = $(PLUGIN_DIR)/parsers/test_pdf_parser.c
TEST_PDF_PARSER_DEPS = $(PDF_PARSER_PLUGIN_OBJ) $(BUILD_DIR)/core/utils.o
TEST_PDF_PARSER_TARGET = $(BUILD_DIR)/test_pdf_parser

# Test PDF Preprocessor
TEST_PDF_PREPROCESSOR_SRC = $(PLUGIN_DIR)/preprocessors/test_pdf_preprocessor.c
TEST_PDF_PREPROCESSOR_DEPS = $(PDF_PREPROCESSOR_PLUGIN_OBJ) $(BUILD_DIR)/core/utils.o
TEST_PDF_PREPROCESSOR_TARGET = $(BUILD_DIR)/test_pdf_preprocessor

# Test RTF Parser
TEST_RTF_PARSER_SRC = $(PLUGIN_DIR)/parsers/test_rtf_parser.c
TEST_RTF_PARSER_DEPS = $(RTF_PARSER_PLUGIN_OBJ) $(BUILD_DIR)/core/utils.o
TEST_RTF_PARSER_TARGET = $(BUILD_DIR)/test_rtf_parser

# Test RTF Preprocessor
TEST_RTF_PREPROCESSOR_SRC = $(PLUGIN_DIR)/preprocessors/test_rtf_preprocessor.c
TEST_RTF_PREPROCESSOR_DEPS = $(RTF_PREPROCESSOR_PLUGIN_OBJ) $(BUILD_DIR)/core/utils.o
TEST_RTF_PREPROCESSOR_TARGET = $(BUILD_DIR)/test_rtf_preprocessor

# Test DICOM Parser
TEST_DIC_PARSER_SRC = $(PLUGIN_DIR)/parsers/test_dic_parser.c
TEST_DIC_PARSER_DEPS = $(DIC_PARSER_PLUGIN_OBJ) $(BUILD_DIR)/core/utils.o
TEST_DIC_PARSER_TARGET = $(BUILD_DIR)/test_dic_parser

# Test DICOM Preprocessor
TEST_DIC_PREPROCESSOR_SRC = $(PLUGIN_DIR)/preprocessors/test_dic_preprocessor.c
TEST_DIC_PREPROCESSOR_DEPS = $(DIC_PREPROCESSOR_PLUGIN_OBJ) $(BUILD_DIR)/core/utils.o
TEST_DIC_PREPROCESSOR_TARGET = $(BUILD_DIR)/test_dic_preprocessor

# Test SQLite Parser
TEST_SQLITE_PARSER_SRC = $(PLUGIN_DIR)/parsers/test_sqlite_parser.c
TEST_SQLITE_PARSER_DEPS = $(SQLITE_PARSER_PLUGIN_OBJ) $(BUILD_DIR)/core/utils.o
TEST_SQLITE_PARSER_TARGET = $(BUILD_DIR)/test_sqlite_parser

# Test SQLite Preprocessor
TEST_SQLITE_PREPROCESSOR_SRC = $(PLUGIN_DIR)/preprocessors/test_sqlite_preprocessor.c
TEST_SQLITE_PREPROCESSOR_DEPS = $(SQLITE_PREPROCESSOR_PLUGIN_OBJ) $(BUILD_DIR)/core/utils.o
TEST_SQLITE_PREPROCESSOR_TARGET = $(BUILD_DIR)/test_sqlite_preprocessor

# Directory for staging plugins for tests
TEST_PLUGINS_STAGING_DIR = $(BUILD_DIR)/plugins_for_test
PLUGIN_OBJ_DIR = $(BUILD_DIR)/plugins/obj
PLUGIN_LIB_DIR = $(BUILD_DIR)/plugins/lib

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

# Disabled Plugin Definitions
DECOMPRESSORS_PLUGIN_SRC_DIR = $(PLUGIN_DIR)/decompressors
# ZIP_DECOMPRESSOR_PLUGIN_SRC = $(DECOMPRESSORS_PLUGIN_SRC_DIR)/zip_decompressor.c
# ZIP_DECOMPRESSOR_PLUGIN_OBJ = $(PLUGIN_OBJ_DIR)/decompressors/zip_decompressor.o
# ZIP_DECOMPRESSOR_PLUGIN_SO = $(PLUGIN_LIB_DIR)/zip_decompressor.so
# GZIP_DECOMPRESSOR_PLUGIN_SRC = $(DECOMPRESSORS_PLUGIN_SRC_DIR)/gzip_decompressor.c
# GZIP_DECOMPRESSOR_PLUGIN_OBJ = $(PLUGIN_OBJ_DIR)/decompressors/gzip_decompressor.o
# GZIP_DECOMPRESSOR_PLUGIN_SO = $(PLUGIN_LIB_DIR)/gzip_decompressor.so
# ZSTD_DECOMPRESSOR_PLUGIN_SRC = $(DECOMPRESSORS_PLUGIN_SRC_DIR)/zstd_decompressor.c
# ZSTD_DECOMPRESSOR_PLUGIN_OBJ = $(PLUGIN_OBJ_DIR)/decompressors/zstd_decompressor.o
# ZSTD_DECOMPRESSOR_PLUGIN_SO = $(PLUGIN_LIB_DIR)/zstd_decompressor.so

SEARCH_ALGORITHMS_PLUGIN_SRC_DIR = $(PLUGIN_DIR)/search_algorithms
# AVX512_PLUGIN_SRC = $(SEARCH_ALGORITHMS_PLUGIN_SRC_DIR)/avx512_phrase_search.c
# AVX512_PLUGIN_OBJ = $(PLUGIN_OBJ_DIR)/search_algorithms/avx512_phrase_search.o
# AVX512_PLUGIN_SO = $(PLUGIN_LIB_DIR)/avx512_phrase_search.so
# AVX_CFLAGS = -mavx512f -mavx512bw

# ML_CLASSIFIER_PLUGIN_SRC_DIR = $(PLUGIN_DIR)/classifiers
# ML_CLASSIFIER_PLUGIN_SRC = $(ML_CLASSIFIER_PLUGIN_SRC_DIR)/ml_classifier.c
# ML_CLASSIFIER_PLUGIN_OBJ = $(PLUGIN_OBJ_DIR)/classifiers/ml_classifier.o
# ML_CLASSIFIER_PLUGIN_SO = $(PLUGIN_LIB_DIR)/ml_classifier.so

# Phony targets
.PHONY: all clean plugins tests

all: $(TARGET_APP) plugins tests

# Rule to build the main application
$(TARGET_APP): $(CORE_OBJS)
	@mkdir -p $(@D)
	$(LD) $(CFLAGS) $(INCLUDE_DIRS) $(CORE_OBJS) -o $@ $(LDFLAGS)
	@echo "Built main application: $@"

# Rule to compile core C files to object files
$(BUILD_DIR)/core/%.o: $(CORE_DIR)/%.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(INCLUDE_DIRS) -c $< -o $@
	@echo "Compiled core object: $@"

$(BUILD_DIR)/core/elasticsearch_client.o: $(CORE_DIR)/elasticsearch_client.c $(INCLUDE_DIR)/elasticsearch_client.h $(INCLUDE_DIR)/utils.h
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(INCLUDE_DIRS) -c $(CORE_DIR)/elasticsearch_client.c -o $@
	@echo "Compiled core object (specific rule): $@"

# --- Test Program Rules ---
tests: $(TEST_CONFIG_LOADER_TARGET) $(TEST_PLUGIN_LOADER_TARGET) $(TEST_LOGGER_TARGET) $(TEST_HW_DETECT_TARGET) $(TEST_PIPELINE_TARGET) $(TEST_ES_CLIENT_TARGET) $(TEST_ES_INDEXING_TARGET) $(TEST_DOC_PARSER_TARGET) $(TEST_DOC_PREPROCESSOR_TARGET) $(TEST_XLS_PARSER_TARGET) $(TEST_XLS_PREPROCESSOR_TARGET) $(TEST_PDF_PARSER_TARGET) $(TEST_PDF_PREPROCESSOR_TARGET) $(TEST_RTF_PARSER_TARGET) $(TEST_RTF_PREPROCESSOR_TARGET) $(TEST_DIC_PARSER_TARGET) $(TEST_DIC_PREPROCESSOR_TARGET) $(TEST_SQLITE_PARSER_TARGET) $(TEST_SQLITE_PREPROCESSOR_TARGET)

$(TEST_CONFIG_LOADER_TARGET): $(TEST_CONFIG_LOADER_SRC) $(TEST_CONFIG_LOADER_DEPS)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(INCLUDE_DIRS) $^ -o $@ $(LDFLAGS)
	@echo "Built test program: $@"

$(TEST_DOC_PARSER_TARGET): $(TEST_DOC_PARSER_SRC) $(TEST_DOC_PARSER_DEPS) $(DOC_PARSER_PLUGIN_SO)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(INCLUDE_DIRS) $(TEST_DOC_PARSER_SRC) $(filter %.o,$^) -o $@ $(LDFLAGS)
	@echo "Built test program: $@"

$(TEST_DOC_PREPROCESSOR_TARGET): $(TEST_DOC_PREPROCESSOR_SRC) $(TEST_DOC_PREPROCESSOR_DEPS) $(DOC_PREPROCESSOR_PLUGIN_SO)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(INCLUDE_DIRS) $(TEST_DOC_PREPROCESSOR_SRC) $(filter %.o,$^) -o $@ $(LDFLAGS)
	@echo "Built test program: $@"

$(TEST_XLS_PARSER_TARGET): $(TEST_XLS_PARSER_SRC) $(TEST_XLS_PARSER_DEPS) $(XLS_PARSER_PLUGIN_SO)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(INCLUDE_DIRS) $(TEST_XLS_PARSER_SRC) $(filter %.o,$^) -o $@ $(LDFLAGS)
	@echo "Built test program: $@"

$(TEST_XLS_PREPROCESSOR_TARGET): $(TEST_XLS_PREPROCESSOR_SRC) $(TEST_XLS_PREPROCESSOR_DEPS) $(XLS_PREPROCESSOR_PLUGIN_SO)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(INCLUDE_DIRS) $(TEST_XLS_PREPROCESSOR_SRC) $(filter %.o,$^) -o $@ $(LDFLAGS)
	@echo "Built test program: $@"

$(TEST_PDF_PARSER_TARGET): $(TEST_PDF_PARSER_SRC) $(TEST_PDF_PARSER_DEPS) $(PDF_PARSER_PLUGIN_SO)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(INCLUDE_DIRS) $(TEST_PDF_PARSER_SRC) $(filter %.o,$^) -o $@ $(LDFLAGS)
	@echo "Built test program: $@"

$(TEST_PDF_PREPROCESSOR_TARGET): $(TEST_PDF_PREPROCESSOR_SRC) $(TEST_PDF_PREPROCESSOR_DEPS) $(PDF_PREPROCESSOR_PLUGIN_SO)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(INCLUDE_DIRS) $(TEST_PDF_PREPROCESSOR_SRC) $(filter %.o,$^) -o $@ $(LDFLAGS)
	@echo "Built test program: $@"

$(TEST_RTF_PARSER_TARGET): $(TEST_RTF_PARSER_SRC) $(TEST_RTF_PARSER_DEPS) $(RTF_PARSER_PLUGIN_SO)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(INCLUDE_DIRS) $(TEST_RTF_PARSER_SRC) $(filter %.o,$^) -o $@ $(LDFLAGS)
	@echo "Built test program: $@"

$(TEST_RTF_PREPROCESSOR_TARGET): $(TEST_RTF_PREPROCESSOR_SRC) $(TEST_RTF_PREPROCESSOR_DEPS) $(RTF_PREPROCESSOR_PLUGIN_SO)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(INCLUDE_DIRS) $(TEST_RTF_PREPROCESSOR_SRC) $(filter %.o,$^) -o $@ $(LDFLAGS)
	@echo "Built test program: $@"

$(TEST_DIC_PARSER_TARGET): $(TEST_DIC_PARSER_SRC) $(TEST_DIC_PARSER_DEPS) $(DIC_PARSER_PLUGIN_SO)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(INCLUDE_DIRS) $(TEST_DIC_PARSER_SRC) $(filter %.o,$^) -o $@ $(LDFLAGS)
	@echo "Built test program: $@"

$(TEST_DIC_PREPROCESSOR_TARGET): $(TEST_DIC_PREPROCESSOR_SRC) $(TEST_DIC_PREPROCESSOR_DEPS) $(DIC_PREPROCESSOR_PLUGIN_SO)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(INCLUDE_DIRS) $(TEST_DIC_PREPROCESSOR_SRC) $(filter %.o,$^) -o $@ $(LDFLAGS)
	@echo "Built test program: $@"

$(TEST_SQLITE_PARSER_TARGET): $(TEST_SQLITE_PARSER_SRC) $(TEST_SQLITE_PARSER_DEPS) $(SQLITE_PARSER_PLUGIN_SO)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(INCLUDE_DIRS) $(TEST_SQLITE_PARSER_SRC) $(filter %.o,$^) -o $@ $(LDFLAGS)
	@echo "Built test program: $@"

$(TEST_SQLITE_PREPROCESSOR_TARGET): $(TEST_SQLITE_PREPROCESSOR_SRC) $(TEST_SQLITE_PREPROCESSOR_DEPS) $(SQLITE_PREPROCESSOR_PLUGIN_SO)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(INCLUDE_DIRS) $(TEST_SQLITE_PREPROCESSOR_SRC) $(filter %.o,$^) -o $@ $(LDFLAGS)
	@echo "Built test program: $@"

$(TEST_ES_INDEXING_TARGET): $(TEST_ES_INDEXING_SRC) $(TEST_ES_INDEXING_LINK_OBJS) plugins
	@mkdir -p $(@D)
	@mkdir -p $(TEST_PLUGINS_STAGING_DIR)
	cp $(PLUGIN_LIB_DIR)/dummy_plugin.so $(TEST_PLUGINS_STAGING_DIR)/
	cp $(PLUGIN_LIB_DIR)/json_preprocessor.so $(TEST_PLUGINS_STAGING_DIR)/
	cp $(PLUGIN_LIB_DIR)/json_parser.so $(TEST_PLUGINS_STAGING_DIR)/
	cp $(PLUGIN_LIB_DIR)/elasticsearch_indexer.so $(TEST_PLUGINS_STAGING_DIR)/
	cp $(PLUGIN_LIB_DIR)/txt_preprocessor.so $(TEST_PLUGINS_STAGING_DIR)/
	cp $(PLUGIN_LIB_DIR)/txt_parser.so $(TEST_PLUGINS_STAGING_DIR)/
	cp $(PLUGIN_LIB_DIR)/csv_preprocessor.so $(TEST_PLUGINS_STAGING_DIR)/
	cp $(PLUGIN_LIB_DIR)/csv_parser.so $(TEST_PLUGINS_STAGING_DIR)/
	# cp $(PLUGIN_LIB_DIR)/zip_decompressor.so $(TEST_PLUGINS_STAGING_DIR)/ # DISABLED
	# cp $(PLUGIN_LIB_DIR)/gzip_decompressor.so $(TEST_PLUGINS_STAGING_DIR)/ # DISABLED
	# cp $(PLUGIN_LIB_DIR)/zstd_decompressor.so $(TEST_PLUGINS_STAGING_DIR)/ # DISABLED
	# cp $(PLUGIN_LIB_DIR)/avx512_phrase_search.so $(TEST_PLUGINS_STAGING_DIR)/ # DISABLED
	# cp $(PLUGIN_LIB_DIR)/ml_classifier.so $(TEST_PLUGINS_STAGING_DIR)/ # DISABLED
	cp $(PLUGIN_LIB_DIR)/content_analyzer.so $(TEST_PLUGINS_STAGING_DIR)/
	if [ -f $(PLUGIN_LIB_DIR)/doc_parser.so ]; then cp $(PLUGIN_LIB_DIR)/doc_parser.so $(TEST_PLUGINS_STAGING_DIR)/ ; fi
	if [ -f $(PLUGIN_LIB_DIR)/doc_preprocessor.so ]; then cp $(PLUGIN_LIB_DIR)/doc_preprocessor.so $(TEST_PLUGINS_STAGING_DIR)/ ; fi
	if [ -f $(PLUGIN_LIB_DIR)/xls_parser.so ]; then cp $(PLUGIN_LIB_DIR)/xls_parser.so $(TEST_PLUGINS_STAGING_DIR)/ ; fi
	if [ -f $(PLUGIN_LIB_DIR)/xls_preprocessor.so ]; then cp $(PLUGIN_LIB_DIR)/xls_preprocessor.so $(TEST_PLUGINS_STAGING_DIR)/ ; fi
	if [ -f $(PLUGIN_LIB_DIR)/pdf_parser.so ]; then cp $(PLUGIN_LIB_DIR)/pdf_parser.so $(TEST_PLUGINS_STAGING_DIR)/ ; fi
	if [ -f $(PLUGIN_LIB_DIR)/pdf_preprocessor.so ]; then cp $(PLUGIN_LIB_DIR)/pdf_preprocessor.so $(TEST_PLUGINS_STAGING_DIR)/ ; fi
	if [ -f $(PLUGIN_LIB_DIR)/rtf_parser.so ]; then cp $(PLUGIN_LIB_DIR)/rtf_parser.so $(TEST_PLUGINS_STAGING_DIR)/ ; fi
	if [ -f $(PLUGIN_LIB_DIR)/rtf_preprocessor.so ]; then cp $(PLUGIN_LIB_DIR)/rtf_preprocessor.so $(TEST_PLUGINS_STAGING_DIR)/ ; fi
	if [ -f $(PLUGIN_LIB_DIR)/dic_parser.so ]; then cp $(PLUGIN_LIB_DIR)/dic_parser.so $(TEST_PLUGINS_STAGING_DIR)/ ; fi
	if [ -f $(PLUGIN_LIB_DIR)/dic_preprocessor.so ]; then cp $(PLUGIN_LIB_DIR)/dic_preprocessor.so $(TEST_PLUGINS_STAGING_DIR)/ ; fi
	if [ -f $(PLUGIN_LIB_DIR)/sqlite_parser.so ]; then cp $(PLUGIN_LIB_DIR)/sqlite_parser.so $(TEST_PLUGINS_STAGING_DIR)/ ; fi
	if [ -f $(PLUGIN_LIB_DIR)/sqlite_preprocessor.so ]; then cp $(PLUGIN_LIB_DIR)/sqlite_preprocessor.so $(TEST_PLUGINS_STAGING_DIR)/ ; fi
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
	cp $(PLUGIN_LIB_DIR)/dummy_plugin.so $(TEST_PLUGINS_STAGING_DIR)/
	cp $(PLUGIN_LIB_DIR)/json_preprocessor.so $(TEST_PLUGINS_STAGING_DIR)/
	cp $(PLUGIN_LIB_DIR)/json_parser.so $(TEST_PLUGINS_STAGING_DIR)/
	cp $(PLUGIN_LIB_DIR)/elasticsearch_indexer.so $(TEST_PLUGINS_STAGING_DIR)/
	cp $(PLUGIN_LIB_DIR)/txt_preprocessor.so $(TEST_PLUGINS_STAGING_DIR)/
	cp $(PLUGIN_LIB_DIR)/txt_parser.so $(TEST_PLUGINS_STAGING_DIR)/
	cp $(PLUGIN_LIB_DIR)/csv_preprocessor.so $(TEST_PLUGINS_STAGING_DIR)/
	cp $(PLUGIN_LIB_DIR)/csv_parser.so $(TEST_PLUGINS_STAGING_DIR)/
	# cp $(PLUGIN_LIB_DIR)/zip_decompressor.so $(TEST_PLUGINS_STAGING_DIR)/ # DISABLED
	# cp $(PLUGIN_LIB_DIR)/gzip_decompressor.so $(TEST_PLUGINS_STAGING_DIR)/ # DISABLED
	# cp $(PLUGIN_LIB_DIR)/zstd_decompressor.so $(TEST_PLUGINS_STAGING_DIR)/ # DISABLED
	# cp $(PLUGIN_LIB_DIR)/avx512_phrase_search.so $(TEST_PLUGINS_STAGING_DIR)/ # DISABLED
	# cp $(PLUGIN_LIB_DIR)/ml_classifier.so $(TEST_PLUGINS_STAGING_DIR)/ # DISABLED
	cp $(PLUGIN_LIB_DIR)/content_analyzer.so $(TEST_PLUGINS_STAGING_DIR)/
	if [ -f $(PLUGIN_LIB_DIR)/doc_parser.so ]; then cp $(PLUGIN_LIB_DIR)/doc_parser.so $(TEST_PLUGINS_STAGING_DIR)/ ; fi
	if [ -f $(PLUGIN_LIB_DIR)/doc_preprocessor.so ]; then cp $(PLUGIN_LIB_DIR)/doc_preprocessor.so $(TEST_PLUGINS_STAGING_DIR)/ ; fi
	if [ -f $(PLUGIN_LIB_DIR)/xls_parser.so ]; then cp $(PLUGIN_LIB_DIR)/xls_parser.so $(TEST_PLUGINS_STAGING_DIR)/ ; fi
	if [ -f $(PLUGIN_LIB_DIR)/xls_preprocessor.so ]; then cp $(PLUGIN_LIB_DIR)/xls_preprocessor.so $(TEST_PLUGINS_STAGING_DIR)/ ; fi
	if [ -f $(PLUGIN_LIB_DIR)/pdf_parser.so ]; then cp $(PLUGIN_LIB_DIR)/pdf_parser.so $(TEST_PLUGINS_STAGING_DIR)/ ; fi
	if [ -f $(PLUGIN_LIB_DIR)/pdf_preprocessor.so ]; then cp $(PLUGIN_LIB_DIR)/pdf_preprocessor.so $(TEST_PLUGINS_STAGING_DIR)/ ; fi
	if [ -f $(PLUGIN_LIB_DIR)/rtf_parser.so ]; then cp $(PLUGIN_LIB_DIR)/rtf_parser.so $(TEST_PLUGINS_STAGING_DIR)/ ; fi
	if [ -f $(PLUGIN_LIB_DIR)/rtf_preprocessor.so ]; then cp $(PLUGIN_LIB_DIR)/rtf_preprocessor.so $(TEST_PLUGINS_STAGING_DIR)/ ; fi
	if [ -f $(PLUGIN_LIB_DIR)/dic_parser.so ]; then cp $(PLUGIN_LIB_DIR)/dic_parser.so $(TEST_PLUGINS_STAGING_DIR)/ ; fi
	if [ -f $(PLUGIN_LIB_DIR)/dic_preprocessor.so ]; then cp $(PLUGIN_LIB_DIR)/dic_preprocessor.so $(TEST_PLUGINS_STAGING_DIR)/ ; fi
	if [ -f $(PLUGIN_LIB_DIR)/sqlite_parser.so ]; then cp $(PLUGIN_LIB_DIR)/sqlite_parser.so $(TEST_PLUGINS_STAGING_DIR)/ ; fi
	if [ -f $(PLUGIN_LIB_DIR)/sqlite_preprocessor.so ]; then cp $(PLUGIN_LIB_DIR)/sqlite_preprocessor.so $(TEST_PLUGINS_STAGING_DIR)/ ; fi
	$(CC) $(CFLAGS) $(INCLUDE_DIRS) $(TEST_PIPELINE_SRC) \
		$(filter %.o,$^) -o $@ $(LDFLAGS)
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
         $(SQLITE_PARSER_PLUGIN_SO) $(SQLITE_PREPROCESSOR_PLUGIN_SO)
	@echo "All specified plugins built."

# Plugin Object File Rules
$(DUMMY_PLUGIN_OBJ): $(DUMMY_PLUGIN_SRC) $(INCLUDE_DIR)/plugin_interface.h $(INCLUDE_DIR)/data_types.h $(INCLUDE_DIR)/utils.h
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(INCLUDE_DIRS) -c $< -o $@
	@echo "Compiled plugin object: $@"
$(JSON_PREPROCESSOR_PLUGIN_OBJ): $(JSON_PREPROCESSOR_PLUGIN_SRC) $(INCLUDE_DIR)/plugin_interface.h $(INCLUDE_DIR)/data_types.h $(INCLUDE_DIR)/utils.h
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(INCLUDE_DIRS) -c $< -o $@
	@echo "Compiled plugin object: $@"
$(JSON_PARSER_PLUGIN_OBJ): $(JSON_PARSER_PLUGIN_SRC) $(INCLUDE_DIR)/plugin_interface.h $(INCLUDE_DIR)/data_types.h $(INCLUDE_DIR)/utils.h
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(INCLUDE_DIRS) -c $< -o $@
	@echo "Compiled plugin object: $@"
$(ELASTICSEARCH_INDEXER_PLUGIN_OBJ): $(ELASTICSEARCH_INDEXER_PLUGIN_SRC) $(INCLUDE_DIR)/plugin_interface.h $(INCLUDE_DIR)/data_types.h $(INCLUDE_DIR)/utils.h $(INCLUDE_DIR)/elasticsearch_client.h
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(INCLUDE_DIRS) -c $< -o $@
	@echo "Compiled plugin object: $@"
$(TXT_PREPROCESSOR_PLUGIN_OBJ): $(TXT_PREPROCESSOR_PLUGIN_SRC) $(INCLUDE_DIR)/plugin_interface.h $(INCLUDE_DIR)/data_types.h $(INCLUDE_DIR)/utils.h $(INCLUDE_DIR)/config.h
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(INCLUDE_DIRS) -c $< -o $@
	@echo "Compiled plugin object: $@"
$(TXT_PARSER_PLUGIN_OBJ): $(TXT_PARSER_PLUGIN_SRC) $(INCLUDE_DIR)/plugin_interface.h $(INCLUDE_DIR)/data_types.h $(INCLUDE_DIR)/utils.h
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(INCLUDE_DIRS) -c $< -o $@
	@echo "Compiled plugin object: $@"
$(CSV_PREPROCESSOR_PLUGIN_OBJ): $(CSV_PREPROCESSOR_PLUGIN_SRC) $(INCLUDE_DIR)/plugin_interface.h $(INCLUDE_DIR)/data_types.h $(INCLUDE_DIR)/utils.h $(INCLUDE_DIR)/config.h
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(INCLUDE_DIRS) -c $< -o $@
	@echo "Compiled plugin object: $@"
$(CSV_PARSER_PLUGIN_OBJ): $(CSV_PARSER_PLUGIN_SRC) $(INCLUDE_DIR)/plugin_interface.h $(INCLUDE_DIR)/data_types.h $(INCLUDE_DIR)/utils.h
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(INCLUDE_DIRS) -c $< -o $@
	@echo "Compiled plugin object: $@"
$(CONTENT_ANALYZER_PLUGIN_OBJ): $(CONTENT_ANALYZER_PLUGIN_SRC) $(INCLUDE_DIR)/plugin_interface.h $(INCLUDE_DIR)/data_types.h $(INCLUDE_DIR)/utils.h
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(INCLUDE_DIRS) -c $< -o $@
	@echo "Compiled plugin object: $@"
$(DOC_PARSER_PLUGIN_OBJ): $(DOC_PARSER_PLUGIN_SRC) $(INCLUDE_DIR)/plugin_interface.h $(INCLUDE_DIR)/data_types.h $(INCLUDE_DIR)/utils.h
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(INCLUDE_DIRS) -c $< -o $@
	@echo "Compiled plugin object: $@"
$(DOC_PREPROCESSOR_PLUGIN_OBJ): $(DOC_PREPROCESSOR_PLUGIN_SRC) $(INCLUDE_DIR)/plugin_interface.h $(INCLUDE_DIR)/data_types.h $(INCLUDE_DIR)/utils.h
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(INCLUDE_DIRS) -c $< -o $@
	@echo "Compiled plugin object: $@"
$(XLS_PARSER_PLUGIN_OBJ): $(XLS_PARSER_PLUGIN_SRC) $(INCLUDE_DIR)/plugin_interface.h $(INCLUDE_DIR)/data_types.h $(INCLUDE_DIR)/utils.h
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(INCLUDE_DIRS) -c $< -o $@
	@echo "Compiled plugin object: $@"
$(XLS_PREPROCESSOR_PLUGIN_OBJ): $(XLS_PREPROCESSOR_PLUGIN_SRC) $(INCLUDE_DIR)/plugin_interface.h $(INCLUDE_DIR)/data_types.h $(INCLUDE_DIR)/utils.h
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(INCLUDE_DIRS) -c $< -o $@
	@echo "Compiled plugin object: $@"
$(PDF_PARSER_PLUGIN_OBJ): $(PDF_PARSER_PLUGIN_SRC) $(INCLUDE_DIR)/plugin_interface.h $(INCLUDE_DIR)/data_types.h $(INCLUDE_DIR)/utils.h
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(INCLUDE_DIRS) -c $< -o $@
	@echo "Compiled plugin object: $@"
$(PDF_PREPROCESSOR_PLUGIN_OBJ): $(PDF_PREPROCESSOR_PLUGIN_SRC) $(INCLUDE_DIR)/plugin_interface.h $(INCLUDE_DIR)/data_types.h $(INCLUDE_DIR)/utils.h
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(INCLUDE_DIRS) -c $< -o $@
	@echo "Compiled plugin object: $@"
$(RTF_PARSER_PLUGIN_OBJ): $(RTF_PARSER_PLUGIN_SRC) $(INCLUDE_DIR)/plugin_interface.h $(INCLUDE_DIR)/data_types.h $(INCLUDE_DIR)/utils.h
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(INCLUDE_DIRS) -c $< -o $@
	@echo "Compiled plugin object: $@"
$(RTF_PREPROCESSOR_PLUGIN_OBJ): $(RTF_PREPROCESSOR_PLUGIN_SRC) $(INCLUDE_DIR)/plugin_interface.h $(INCLUDE_DIR)/data_types.h $(INCLUDE_DIR)/utils.h
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(INCLUDE_DIRS) -c $< -o $@
	@echo "Compiled plugin object: $@"
$(DIC_PARSER_PLUGIN_OBJ): $(DIC_PARSER_PLUGIN_SRC) $(INCLUDE_DIR)/plugin_interface.h $(INCLUDE_DIR)/data_types.h $(INCLUDE_DIR)/utils.h
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(INCLUDE_DIRS) -c $< -o $@
	@echo "Compiled plugin object: $@"
$(DIC_PREPROCESSOR_PLUGIN_OBJ): $(DIC_PREPROCESSOR_PLUGIN_SRC) $(INCLUDE_DIR)/plugin_interface.h $(INCLUDE_DIR)/data_types.h $(INCLUDE_DIR)/utils.h
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(INCLUDE_DIRS) -c $< -o $@
	@echo "Compiled plugin object: $@"
$(SQLITE_PARSER_PLUGIN_OBJ): $(SQLITE_PARSER_PLUGIN_SRC) $(INCLUDE_DIR)/plugin_interface.h $(INCLUDE_DIR)/data_types.h $(INCLUDE_DIR)/utils.h
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(INCLUDE_DIRS) -c $< -o $@
	@echo "Compiled plugin object: $@"
$(SQLITE_PREPROCESSOR_PLUGIN_OBJ): $(SQLITE_PREPROCESSOR_PLUGIN_SRC) $(INCLUDE_DIR)/plugin_interface.h $(INCLUDE_DIR)/data_types.h $(INCLUDE_DIR)/utils.h
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(INCLUDE_DIRS) -c $< -o $@
	@echo "Compiled plugin object: $@"

# Disabled Plugin Object File Rules
# $(ZIP_DECOMPRESSOR_PLUGIN_OBJ): $(ZIP_DECOMPRESSOR_PLUGIN_SRC) ...
# $(GZIP_DECOMPRESSOR_PLUGIN_OBJ): $(GZIP_DECOMPRESSOR_PLUGIN_SRC) ...
# $(ZSTD_DECOMPRESSOR_PLUGIN_OBJ): $(ZSTD_DECOMPRESSOR_PLUGIN_SRC) ...
# $(AVX512_PLUGIN_OBJ): $(AVX512_PLUGIN_SRC) ...
# $(ML_CLASSIFIER_PLUGIN_OBJ): $(ML_CLASSIFIER_PLUGIN_SRC) ...

# Plugin Shared Object (.so) Rules
$(DUMMY_PLUGIN_SO): $(DUMMY_PLUGIN_OBJ)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -shared $^ -o $@ $(LDFLAGS)
	@echo "Built plugin: $@"
$(JSON_PREPROCESSOR_PLUGIN_SO): $(JSON_PREPROCESSOR_PLUGIN_OBJ)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -shared $^ -o $@ $(LDFLAGS)
	@echo "Built plugin: $@"
$(JSON_PARSER_PLUGIN_SO): $(JSON_PARSER_PLUGIN_OBJ)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -shared $^ -o $@ $(LDFLAGS)
	@echo "Built plugin: $@"
$(ELASTICSEARCH_INDEXER_PLUGIN_SO): $(ELASTICSEARCH_INDEXER_PLUGIN_OBJ) $(BUILD_DIR)/core/elasticsearch_client.o
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -shared $^ -o $@ $(LDFLAGS)
	@echo "Built plugin: $@"
$(TXT_PREPROCESSOR_PLUGIN_SO): $(TXT_PREPROCESSOR_PLUGIN_OBJ)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -shared $^ -o $@ $(LDFLAGS)
	@echo "Built plugin: $@"
$(TXT_PARSER_PLUGIN_SO): $(TXT_PARSER_PLUGIN_OBJ)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -shared $^ -o $@ $(LDFLAGS)
	@echo "Built plugin: $@"
$(CSV_PREPROCESSOR_PLUGIN_SO): $(CSV_PREPROCESSOR_PLUGIN_OBJ)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -shared $^ -o $@ $(LDFLAGS)
	@echo "Built plugin: $@"
$(CSV_PARSER_PLUGIN_SO): $(CSV_PARSER_PLUGIN_OBJ)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -shared $^ -o $@ $(LDFLAGS)
	@echo "Built plugin: $@"
$(CONTENT_ANALYZER_PLUGIN_SO): $(CONTENT_ANALYZER_PLUGIN_OBJ)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -shared $^ -o $@ $(LDFLAGS)
	@echo "Built plugin: $@"
$(DOC_PARSER_PLUGIN_SO): $(DOC_PARSER_PLUGIN_OBJ)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -shared $^ -o $@ $(LDFLAGS)
	@echo "Built plugin: $@"
$(DOC_PREPROCESSOR_PLUGIN_SO): $(DOC_PREPROCESSOR_PLUGIN_OBJ)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -shared $^ -o $@ $(LDFLAGS)
	@echo "Built plugin: $@"
$(XLS_PARSER_PLUGIN_SO): $(XLS_PARSER_PLUGIN_OBJ)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -shared $^ -o $@ $(LDFLAGS)
	@echo "Built plugin: $@"
$(XLS_PREPROCESSOR_PLUGIN_SO): $(XLS_PREPROCESSOR_PLUGIN_OBJ)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -shared $^ -o $@ $(LDFLAGS)
	@echo "Built plugin: $@"
$(PDF_PARSER_PLUGIN_SO): $(PDF_PARSER_PLUGIN_OBJ)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -shared $^ -o $@ $(LDFLAGS)
	@echo "Built plugin: $@"
$(PDF_PREPROCESSOR_PLUGIN_SO): $(PDF_PREPROCESSOR_PLUGIN_OBJ)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -shared $^ -o $@ $(LDFLAGS)
	@echo "Built plugin: $@"
$(RTF_PARSER_PLUGIN_SO): $(RTF_PARSER_PLUGIN_OBJ)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -shared $^ -o $@ $(LDFLAGS)
	@echo "Built plugin: $@"
$(RTF_PREPROCESSOR_PLUGIN_SO): $(RTF_PREPROCESSOR_PLUGIN_OBJ)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -shared $^ -o $@ $(LDFLAGS)
	@echo "Built plugin: $@"
$(DIC_PARSER_PLUGIN_SO): $(DIC_PARSER_PLUGIN_OBJ)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -shared $^ -o $@ $(LDFLAGS)
	@echo "Built plugin: $@"
$(DIC_PREPROCESSOR_PLUGIN_SO): $(DIC_PREPROCESSOR_PLUGIN_OBJ)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -shared $^ -o $@ $(LDFLAGS)
	@echo "Built plugin: $@"
$(SQLITE_PARSER_PLUGIN_SO): $(SQLITE_PARSER_PLUGIN_OBJ)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -shared $^ -o $@ $(LDFLAGS)
	@echo "Built plugin: $@"
$(SQLITE_PREPROCESSOR_PLUGIN_SO): $(SQLITE_PREPROCESSOR_PLUGIN_OBJ)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -shared $^ -o $@ $(LDFLAGS)
	@echo "Built plugin: $@"

# Disabled Plugin .so Rules
# $(ZIP_DECOMPRESSOR_PLUGIN_SO): $(ZIP_DECOMPRESSOR_PLUGIN_OBJ) ...
# $(GZIP_DECOMPRESSOR_PLUGIN_SO): $(GZIP_DECOMPRESSOR_PLUGIN_OBJ) ...
# $(ZSTD_DECOMPRESSOR_PLUGIN_SO): $(ZSTD_DECOMPRESSOR_PLUGIN_OBJ) ...
# $(AVX512_PLUGIN_SO): $(AVX512_PLUGIN_OBJ) ...
# $(ML_CLASSIFIER_PLUGIN_SO): $(ML_CLASSIFIER_PLUGIN_OBJ) ...

clean:
	@echo "Cleaning build artifacts..."
	rm -rf $(BUILD_DIR)
	rm -f $(PLUGIN_DIR)/dummy_plugin/dummy_plugin.so
	rm -f $(PLUGIN_DIR)/preprocessors/json_preprocessor.so
	rm -f $(PLUGIN_DIR)/parsers/json_parser.so
	rm -f $(PLUGIN_DIR)/indexers/elasticsearch_indexer.so
	rm -f $(PLUGIN_DIR)/preprocessors/txt_preprocessor.so
	rm -f $(PLUGIN_DIR)/parsers/txt_parser.so
	rm -f $(PLUGIN_DIR)/preprocessors/csv_preprocessor.so
	rm -f $(PLUGIN_DIR)/parsers/csv_parser.so
	@echo "Clean complete."
