file(GLOB CORE_SOURCES "*.c")
list(FILTER CORE_SOURCES EXCLUDE REGEX "main\\.c")
list(FILTER CORE_SOURCES EXCLUDE REGEX "test_.*\\.c")

add_library(spindex_core STATIC ${CORE_SOURCES})

target_link_libraries(spindex_core PUBLIC
    Threads::Threads
    libcurl::libcurl    # Conan-provided target for libcurl
    Jansson::jansson    # Conan-provided target for Jansson
    GEOS::geos          # Conan-provided target for GEOS
)

add_executable(spindex_app main.c)
target_link_libraries(spindex_app PRIVATE spindex_core spindex_aegis)

# Add tests
set(CORE_TEST_SOURCES
    test_config_manager.c
    test_plugin_loader.c
    test_logger.c
    test_hardware_detection.c
    test_elasticsearch_client.c
)

foreach(test_source ${CORE_TEST_SOURCES})
    get_filename_component(test_name ${test_source} NAME_WE)
    add_executable(${test_name} ${test_source})
    target_link_libraries(${test_name} PRIVATE spindex_core)
    # Enable testing for each test executable
    add_test(NAME ${test_name} COMMAND ${test_name})
endforeach()

# Test ingestion pipeline
add_executable(test_ingestion_pipeline test_ingestion_pipeline.c)
target_link_libraries(test_ingestion_pipeline PRIVATE spindex_core spindex_aegis)
add_dependencies(test_ingestion_pipeline stage_all_plugins_for_core_tests)
add_test(NAME test_ingestion_pipeline COMMAND test_ingestion_pipeline WORKING_DIRECTORY ${PROJECT_BINARY_DIR})

# Test Elasticsearch indexing
add_executable(test_elasticsearch_indexing test_elasticsearch_indexing.c)
target_link_libraries(test_elasticsearch_indexing PRIVATE spindex_core spindex_aegis)
add_dependencies(test_elasticsearch_indexing stage_all_plugins_for_core_tests)
add_test(NAME test_elasticsearch_indexing COMMAND test_elasticsearch_indexing WORKING_DIRECTORY ${PROJECT_BINARY_DIR})

# --- AEGIS Module Tests ---
# These tests use source files from core/ (e.g., test_complex_geometry_handler.c)
# but test functionalities from plugins/aegis_modules/ (e.g., complex_geometry_handler.c).
# The aegis_modules are compiled into the spindex_aegis static library.
# These tests also depend on utilities from spindex_core.

# Test for complex_geometry_handler
# Source: core/test_complex_geometry_handler.c
add_executable(test_complex_geometry_handler test_complex_geometry_handler.c)
target_link_libraries(test_complex_geometry_handler PRIVATE spindex_core spindex_aegis)
add_test(NAME test_complex_geometry_handler COMMAND test_complex_geometry_handler)

# Test for nlp_ner_parser
# Source: core/test_nlp_ner_parser.c
add_executable(test_nlp_ner_parser test_nlp_ner_parser.c)
# spindex_aegis PUBLICly links Jansson::jansson, so this test should inherit it.
# Adding Jansson::jansson explicitly if test code directly uses Jansson API.
target_link_libraries(test_nlp_ner_parser PRIVATE spindex_core spindex_aegis Jansson::jansson)
add_test(NAME test_nlp_ner_parser COMMAND test_nlp_ner_parser)

# Test for geometric_validator
# Source: core/test_geometric_validator.c
add_executable(test_geometric_validator test_geometric_validator.c)
# spindex_aegis PUBLICly links GEOS::geos, so this test should inherit it.
# Adding GEOS::geos explicitly if test code directly uses GEOS API.
target_link_libraries(test_geometric_validator PRIVATE spindex_core spindex_aegis GEOS::geos)
add_test(NAME test_geometric_validator COMMAND test_geometric_validator)