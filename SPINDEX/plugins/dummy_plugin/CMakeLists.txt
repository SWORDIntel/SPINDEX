# SPINDEX/plugins/dummy_plugin/CMakeLists.txt
cmake_minimum_required(VERSION 3.15)

set(PLUGIN_NAME "dummy_plugin")
set(PLUGIN_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/${PLUGIN_NAME}.c")

add_library(${PLUGIN_NAME} SHARED ${PLUGIN_SOURCES})

# Include directories are expected to be handled by the top-level CMakeLists.txt
# (e.g., via include_directories(${CMAKE_SOURCE_DIR}/include))
# or explicitly added here if needed:
# target_include_directories(${PLUGIN_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/include)

# No specific link libraries for dummy_plugin.
# Output directory for .so files is handled by CMAKE_LIBRARY_OUTPUT_DIRECTORY
# set in the top-level SPINDEX/CMakeLists.txt.
