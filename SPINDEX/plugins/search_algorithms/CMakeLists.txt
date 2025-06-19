# SPINDEX/plugins/search_algorithms/CMakeLists.txt
cmake_minimum_required(VERSION 3.15)

set(PLUGIN_NAME "avx512_phrase_search")
set(PLUGIN_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/${PLUGIN_NAME}.c")

add_library(${PLUGIN_NAME} SHARED ${PLUGIN_SOURCES})

# AVX-512 plugin depends on hardware_detection and utils from spindex_core.
target_link_libraries(${PLUGIN_NAME} PRIVATE spindex_core)

# Add AVX-specific compiler flags.
# For a production system, you might want to check if the compiler supports these flags,
# e.g., using CheckCCompilerFlag from CMake's CheckCompilerFlags module.
target_compile_options(${PLUGIN_NAME} PRIVATE -mavx512f -mavx512bw)

# Include directories (e.g., for plugin_interface.h) are expected to be handled
# by the top-level CMakeLists.txt or explicitly added here if necessary.
# Output directory for .so files is handled by CMAKE_LIBRARY_OUTPUT_DIRECTORY
# set in the top-level SPINDEX/CMakeLists.txt.

# Add test for avx512_phrase_search
set(TEST_NAME "test_avx512_phrase_search")
set(TEST_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/${TEST_NAME}.c")
# This should match the PLUGIN_NAME variable used for add_library
set(PLUGIN_LIB_TARGET "avx512_phrase_search")

add_executable(${TEST_NAME} ${TEST_SOURCE})
# Link against the plugin library itself and spindex_core (for utils, hardware_detection, etc.)
target_link_libraries(${TEST_NAME} PRIVATE ${PLUGIN_LIB_TARGET} spindex_core)
add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})

# Note: The AVX compile options (-mavx512f -mavx512bw) are applied to the
# avx512_phrase_search library target. The test executable links against this library.
# It's generally not necessary to apply these compile options directly to the
# test_avx512_phrase_search.c compilation unless the test code itself uses AVX intrinsics
# independently of the plugin.
