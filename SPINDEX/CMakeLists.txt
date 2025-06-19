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

# OpenVINO Detection
# Users might need to set OpenVINO_DIR environment variable or CMake variable
# for find_package to locate the OpenVINO installation.
# Example: cmake -DOpenVINO_DIR=/opt/intel/openvino_2023 ..
find_package(OpenVINO) # Try to find OpenVINO, but don't fail if not found initially.

if(OpenVINO_FOUND)
    message(STATUS "Found OpenVINO: YES")
    # FindOpenVINO.cmake should provide imported targets (e.g., OpenVINO::runtime, OpenVINO::Core).
    # The sub-projects (openvino_classifier plugin, test_openvino_classifier_hw)
    # will link against these. The placeholder 'OpenVINO::openvino' used earlier
    # should be replaced with actual targets like OpenVINO::runtime if that's what FindOpenVINO provides.
    # For now, this message indicates it's found. The actual linking targets are deferred
    # to where they are used, assuming FindOpenVINO sets them up.
    # If specific components are needed, they can be listed in find_package:
    # find_package(OpenVINO COMPONENTS ie_c_api runtime REQUIRED)
    # This would create targets like OpenVINO::ie_c_api and OpenVINO::runtime
    message(STATUS "  OpenVINO version: ${OpenVINO_VERSION}") # Example: Check version if available
    message(STATUS "  Relevant OpenVINO CMake variables might include:")
    message(STATUS "    OpenVINO_INCLUDE_DIRS: ${OpenVINO_INCLUDE_DIRS}")
    message(STATUS "    OpenVINO_LIBS: ${OpenVINO_LIBS}") # Older style, prefer imported targets
    message(STATUS "    OpenVINO_LIBRARIES: ${OpenVINO_LIBRARIES}") # Older style
else()
    message(WARNING "OpenVINO not found. The 'openvino_classifier' plugin and 'test_openvino_classifier_hw' test may not build or link correctly. Please ensure OpenVINO is installed and CMake can find it (e.g., set OpenVINO_DIR).")
    # To make OpenVINO a hard requirement for the project to configure,
    # change `find_package(OpenVINO)` to `find_package(OpenVINO REQUIRED)`.
endif()

message(STATUS "Adding subdirectories in specific order for dependency resolution.")
add_subdirectory(plugins/aegis_modules) # Defines spindex_aegis (STATIC LIB)
add_subdirectory(plugins)               # Defines all .so plugins and the 'stage_all_plugins_for_core_tests' custom target
add_subdirectory(core)                  # Defines spindex_core (STATIC LIB), spindex_app (EXE), and core test executables
add_subdirectory(tests)                 # Defines test_openvino_classifier_hw (EXE) and other standalone tests

enable_testing()
