# SPINDEX/plugins/indexers/CMakeLists.txt
cmake_minimum_required(VERSION 3.15)

set(PLUGIN_NAME "elasticsearch_indexer")
set(PLUGIN_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/${PLUGIN_NAME}.c")

add_library(${PLUGIN_NAME} SHARED ${PLUGIN_SOURCES})

# elasticsearch_indexer plugin depends on functions from elasticsearch_client and utils,
# which are part of spindex_core.
target_link_libraries(${PLUGIN_NAME} PRIVATE spindex_core)

# Include directories are expected to be handled by the top-level CMakeLists.txt
# or explicitly added here if needed:
# target_include_directories(${PLUGIN_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/include)

# Output directory for .so files is handled by CMAKE_LIBRARY_OUTPUT_DIRECTORY.
