# SPINDEX/tests/CMakeLists.txt
cmake_minimum_required(VERSION 3.15)

# Test Harness for OpenVINO Hardware-Specific Validation
set(TEST_NAME "test_openvino_classifier_hw")
set(TEST_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/${TEST_NAME}.c")

add_executable(${TEST_NAME} ${TEST_SOURCE})

# This test dynamically loads the openvino_classifier plugin.
# It needs to link against libraries for dlopen (provided by Threads::Threads)
# and OpenVINO runtime libraries.
# Assumes OpenVINO has been found by find_package(OpenVINO) in a higher-level CMakeLists.txt.
target_link_libraries(${TEST_NAME} PRIVATE
    Threads::Threads # For dlopen, dlsym functionality
    # This is a placeholder for the actual OpenVINO imported target.
    OpenVINO::openvino
)

# Add general SPINDEX include directory, in case it uses any core headers directly.
# (e.g. for config.h, plugin_interface.h if it doesn't get them via OpenVINO includes)
target_include_directories(${TEST_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)
# OpenVINO's own include directories should come from the OpenVINO::openvino target's interface properties.

add_test(
    NAME ${TEST_NAME}
    COMMAND ${TEST_NAME}
    # This test might require the compiled openvino_classifier.so to be in a path
    # where dlopen can find it. This could be handled by setting working directory
    # or environment variables if CTest were to run it automatically.
    # For now, it's marked as a manual test in the roadmap.
)

# Note: The Makefile mentions this test is for manual execution on target hardware.
# The openvino_classifier.so plugin (built in SPINDEX/plugins/classifiers) must be discoverable at runtime.
