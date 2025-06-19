# SPINDEX/plugins/aegis_modules/CMakeLists.txt
cmake_minimum_required(VERSION 3.15) # Or inherit from parent

file(GLOB AEGIS_SOURCES "*.c")
# Exclude any test files if they exist in this directory in the future
# list(FILTER AEGIS_SOURCES EXCLUDE REGEX "test_.*\\.c")

add_library(spindex_aegis STATIC ${AEGIS_SOURCES})

target_include_directories(spindex_aegis PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR} # For headers within aegis_modules like complex_geometry_handler.h
    ${CMAKE_SOURCE_DIR}/include # Access to general include files like data_types.h
)

# Assuming Jansson and GEOS are found by the top-level CMakeLists.txt
target_link_libraries(spindex_aegis PUBLIC
    Jansson::jansson   # Conan-provided target for Jansson
    GEOS::geos         # Conan-provided target for GEOS
)
# Add other dependencies if AEGIS modules require them (e.g. CURL, Threads)
# Based on Makefile, these modules don't seem to directly require curl or pthreads beyond what core might provide.
