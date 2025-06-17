cmake_minimum_required(VERSION 3.10)
project(SPINDEX C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -g -fPIC")

# Include directory for the project
include_directories(include)

# External Libraries
# PkgConfig
find_package(PkgConfig REQUIRED)

# Jansson
pkg_check_modules(JANSSON REQUIRED jansson)
include_directories(${JANSSON_INCLUDE_DIRS}) # Make Jansson headers available globally

# GEOS
pkg_check_modules(GEOS REQUIRED geos) # Using pkg-config for GEOS (trying 'geos' instead of 'geos-c')
include_directories(${GEOS_INCLUDE_DIRS}) # Make GEOS headers available globally

# CURL
find_package(CURL REQUIRED)
# PThreads
find_package(Threads REQUIRED)

# Libdl - usually found as a system library, will link with ${CMAKE_DL_LIBS}

add_subdirectory(core)
add_subdirectory(plugins/aegis_modules)

# Main executable (defined in core/CMakeLists.txt but spindex_app depends on aegis_modules)
# Ensure spindex_app target from core/CMakeLists.txt links with aegis_modules library
# This will be handled by target_link_libraries in core/CMakeLists.txt
