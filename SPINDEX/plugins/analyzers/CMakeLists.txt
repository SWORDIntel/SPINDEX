# SPINDEX/plugins/analyzers/CMakeLists.txt
cmake_minimum_required(VERSION 3.15)

set(PLUGIN_NAME "content_analyzer")
set(PLUGIN_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/${PLUGIN_NAME}.c")

add_library(${PLUGIN_NAME} SHARED ${PLUGIN_SOURCES})

# Include directories are expected to be handled by the top-level CMakeLists.txt
# or explicitly added here if needed:
# target_include_directories(${PLUGIN_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/include)

# No specific link libraries for content_analyzer.
# Output directory for .so files is handled by CMAKE_LIBRARY_OUTPUT_DIRECTORY.
