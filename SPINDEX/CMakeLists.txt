cmake_minimum_required(VERSION 3.15)
project(SPINDEX C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -g -fPIC")

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin)

include_directories(include core)

find_package(PkgConfig REQUIRED)

find_package(Threads REQUIRED)
find_package(CURL REQUIRED)

pkg_check_modules(JANSSON REQUIRED jansson)
if(JANSSON_FOUND)
    message(STATUS "Found Jansson: YES (via pkg-config)")
    message(STATUS "  Jansson includes: ${JANSSON_INCLUDE_DIRS}")
    message(STATUS "  Jansson libraries: ${JANSSON_LIBRARIES}")
    # Create an interface library for Jansson to make it easy to use
    add_library(Jansson::Jansson INTERFACE IMPORTED)
    target_include_directories(Jansson::Jansson INTERFACE ${JANSSON_INCLUDE_DIRS})
    # For libraries found by pkg-config, JANSSON_LIBRARIES often contains full paths
    # or linker flags like -ljansson. target_link_libraries can usually handle these.
    target_link_libraries(Jansson::Jansson INTERFACE ${JANSSON_LIBRARIES})
else()
    message(FATAL_ERROR "Jansson not found via pkg-config.")
endif()

# Try to find GEOS C API using pkg-config
pkg_check_modules(GEOS_C REQUIRED geos-c) # 'geos-c' is common for the C API
if(GEOS_C_FOUND)
    message(STATUS "Found GEOS (C API): YES (via pkg-config)")
    message(STATUS "  GEOS includes: ${GEOS_C_INCLUDE_DIRS}")
    message(STATUS "  GEOS libraries: ${GEOS_C_LIBRARIES}")
    add_library(GEOS::geos_c INTERFACE IMPORTED) # Keep GEOS::geos_c as used before
    target_include_directories(GEOS::geos_c INTERFACE ${GEOS_C_INCLUDE_DIRS})
    target_link_libraries(GEOS::geos_c INTERFACE ${GEOS_C_LIBRARIES})
else()
    # Fallback if 'geos-c.pc' is not found, try 'geos.pc'
    pkg_check_modules(GEOS REQUIRED geos)
    if(GEOS_FOUND)
        message(STATUS "Found GEOS (generic): YES (via pkg-config)")
        message(STATUS "  GEOS includes: ${GEOS_INCLUDE_DIRS}")
        message(STATUS "  GEOS libraries: ${GEOS_LIBRARIES}")
        add_library(GEOS::geos_c INTERFACE IMPORTED) # Still use GEOS::geos_c for consistency
        target_include_directories(GEOS::geos_c INTERFACE ${GEOS_INCLUDE_DIRS})
        target_link_libraries(GEOS::geos_c INTERFACE ${GEOS_LIBRARIES})
    else()
        message(FATAL_ERROR "GEOS (neither geos-c nor geos) not found via pkg-config.")
    endif()
endif()

# Placeholder for OpenVINO detection
# find_package(OpenVINO REQUIRED)

add_subdirectory(core)
add_subdirectory(plugins/aegis_modules)

enable_testing()
