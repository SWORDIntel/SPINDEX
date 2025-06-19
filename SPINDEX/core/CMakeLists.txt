file(GLOB CORE_SOURCES "*.c")
list(FILTER CORE_SOURCES EXCLUDE REGEX "main\\.c")
list(FILTER CORE_SOURCES EXCLUDE REGEX "test_.*\\.c")

add_library(spindex_core STATIC ${CORE_SOURCES})

target_link_libraries(spindex_core PUBLIC
    Threads::Threads
    CURL::libcurl
    Jansson::Jansson
    GEOS::geos_c # Assuming geos_c, might need adjustment
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
add_test(NAME test_ingestion_pipeline COMMAND test_ingestion_pipeline)

# Test Elasticsearch indexing
add_executable(test_elasticsearch_indexing test_elasticsearch_indexing.c)
target_link_libraries(test_elasticsearch_indexing PRIVATE spindex_core spindex_aegis)
add_test(NAME test_elasticsearch_indexing COMMAND test_elasticsearch_indexing)
