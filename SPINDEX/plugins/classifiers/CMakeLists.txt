# SPINDEX/plugins/classifiers/CMakeLists.txt
cmake_minimum_required(VERSION 3.15)

set(PLUGIN_NAME "openvino_classifier")
set(PLUGIN_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/${PLUGIN_NAME}.c")

add_library(${PLUGIN_NAME} SHARED ${PLUGIN_SOURCES})

# openvino_classifier plugin depends on utils from spindex_core
# and OpenVINO libraries.
# Assumes OpenVINO has been found by find_package(OpenVINO) in a higher-level CMakeLists.txt
# and provides imported targets.
# Link against spindex_core for utils.o
target_link_libraries(${PLUGIN_NAME} PRIVATE
    spindex_core
    # This is a placeholder for the actual OpenVINO imported target.
    # Common targets could be OpenVINO::runtime, OpenVINO::Core, etc.
    # This will be resolved when FindOpenVINO.cmake is fully integrated.
    OpenVINO::openvino
)

# Include directories from OpenVINO should be handled by target_link_libraries
# if FindOpenVINO.cmake sets up the imported targets correctly with INTERFACE_INCLUDE_DIRECTORIES.
# e.g., target_include_directories(${PLUGIN_NAME} PRIVATE ${OpenVINO_INCLUDE_DIRS}) might not be needed.

# General include directories for plugin_interface.h are expected to be handled globally.
# Output directory for .so files is handled by CMAKE_LIBRARY_OUTPUT_DIRECTORY.
