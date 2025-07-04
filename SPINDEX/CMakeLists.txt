cmake_minimum_required(VERSION 3.15)
project(SPINDEX VERSION 1.0 LANGUAGES C)

# --- Global Settings ---
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Default build type if not specified by user (e.g., -DCMAKE_BUILD_TYPE=Release)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug)
endif()

# Common compile flags
set(CMAKE_C_FLAGS_INIT "") # Initialize
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS_INIT} -Wall -Wextra")
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g")
endif()
# -fPIC is handled by CMAKE_POSITION_INDEPENDENT_CODE ON

# Output directories
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib/spindex/plugins) # For plugins
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# --- Include Directories ---
# Add top-level include directory globally.
# Target-specific include directories are preferred but this is a starting point.
include_directories(
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/core" # core might have internal headers
    "${CMAKE_CURRENT_SOURCE_DIR}/plugins/aegis_modules" # AEGIS module headers
)

# --- External Dependencies ---
# Threads
find_package(Threads REQUIRED)

# CURL
find_package(CURL REQUIRED)

# Jansson (JSON library)
find_package(Jansson)
if(NOT Jansson_FOUND)
  message(STATUS "Default FindJansson.cmake did not find Jansson. Trying pkg-config.")
  find_package(PkgConfig QUIET)
  if(PKG_CONFIG_FOUND)
    pkg_check_modules(PC_JANSSON QUIET jansson)
    if(PC_JANSSON_FOUND)
      message(STATUS "Found Jansson using pkg-config")
      set(JANSSON_INCLUDE_DIRS ${PC_JANSSON_INCLUDE_DIRS})
      set(JANSSON_LIBRARIES ${PC_JANSSON_LIBRARIES}) # This will be list of full library paths or -ljansson
      set(Jansson_FOUND TRUE)
      include_directories(${JANSSON_INCLUDE_DIRS})
    else()
      message(WARNING "pkg-config could not find Jansson. Please ensure libjansson-dev (or equivalent) is installed.")
    endif()
  else()
     message(WARNING "pkg-config not found. Cannot search for Jansson via pkg-config.")
  endif()

  if(NOT Jansson_FOUND)
    message(FATAL_ERROR "Jansson library not found. Please install libjansson-dev or ensure it's in your CMAKE_PREFIX_PATH.")
  endif()
else()
  message(STATUS "Found Jansson using FindJansson.cmake module.")
  # Assuming Jansson_INCLUDE_DIRS and Jansson_LIBRARIES are set by FindJansson.cmake
  # Or if it provides an imported target Jansson::Jansson
  if(TARGET Jansson::Jansson)
    # Link against imported target later
  else()
    include_directories(${Jansson_INCLUDE_DIRS})
    # JANSSON_LIBRARIES will be used in target_link_libraries
  endif()
endif()

# GEOS (Geometry library)
find_package(GEOS)
if(NOT GEOS_FOUND)
    message(STATUS "Default FindGEOS.cmake did not find GEOS. Trying geos-config.")
    find_program(GEOS_CONFIG_EXECUTABLE geos-config)
    if(GEOS_CONFIG_EXECUTABLE)
        message(STATUS "Found geos-config: ${GEOS_CONFIG_EXECUTABLE}")
        execute_process(COMMAND ${GEOS_CONFIG_EXECUTABLE} --cflags OUTPUT_VARIABLE GEOS_CFLAGS_STR OUTPUT_STRIP_TRAILING_WHITESPACE)
        execute_process(COMMAND ${GEOS_CONFIG_EXECUTABLE} --libs OUTPUT_VARIABLE GEOS_LIBS_STR OUTPUT_STRIP_TRAILING_WHITESPACE)

        string(REGEX MATCHALL "-I[^ ]+" GEOS_INCLUDE_DIRS_MATCHES "${GEOS_CFLAGS_STR}")
        set(GEOS_TEMP_INCLUDE_DIRS "")
        foreach(match ${GEOS_INCLUDE_DIRS_MATCHES})
            string(SUBSTRING "${match}" 2 -1 current_include_dir)
            list(APPEND GEOS_TEMP_INCLUDE_DIRS "${current_include_dir}")
        endforeach()
        include_directories(${GEOS_TEMP_INCLUDE_DIRS})
        set(GEOS_CONFIG_LIBRARIES ${GEOS_LIBS_STR}) # This is a string of linker flags e.g. "-L/path -lgeos_c"
        set(GEOS_FOUND TRUE)
        message(STATUS "GEOS Includes (from geos-config): ${GEOS_TEMP_INCLUDE_DIRS}")
        message(STATUS "GEOS Libs (from geos-config): ${GEOS_CONFIG_LIBRARIES}")
    else()
        message(WARNING "geos-config not found. Please ensure libgeos-dev (or equivalent) is installed and geos-config is in PATH.")
    endif()

    if(NOT GEOS_FOUND)
        message(FATAL_ERROR "GEOS library not found. Please install libgeos-dev or ensure geos-config is available.")
    endif()
else()
    message(STATUS "Found GEOS using FindGEOS.cmake module.")
    if(TARGET GEOS::GEOS)
        # GEOS::GEOS target will be used for linking
    else()
      include_directories(${GEOS_INCLUDE_DIRS})
      # GEOS_LIBRARIES will be used for linking
    endif()
endif()


# OpenVINO
option(USE_OPENVINO "Enable OpenVINO support" ON)
set(OPENVINO_DIR "" CACHE PATH "Path to OpenVINO installation (e.g., /opt/intel/openvino_2023)")

if(USE_OPENVINO)
    if(NOT OPENVINO_DIR AND DEFINED ENV{INTEL_OPENVINO_DIR})
        set(OPENVINO_DIR "$ENV{INTEL_OPENVINO_DIR}")
    endif()

    if(OPENVINO_DIR AND EXISTS "${OPENVINO_DIR}")
        message(STATUS "Configuring OpenVINO from: ${OPENVINO_DIR}")
        set(OPENVINO_INCLUDE_PATH_RUNTIME "${OPENVINO_DIR}/runtime/include")
        set(OPENVINO_INCLUDE_PATH_DEV "${OPENVINO_DIR}/development_tools/inference_engine/include") # Older versions

        if(EXISTS "${OPENVINO_INCLUDE_PATH_RUNTIME}/openvino/c/openvino.h")
            set(OPENVINO_PRIMARY_INCLUDE_DIR "${OPENVINO_INCLUDE_PATH_RUNTIME}")
        elseif(EXISTS "${OPENVINO_INCLUDE_PATH_DEV}/openvino/c/openvino.h")
            set(OPENVINO_PRIMARY_INCLUDE_DIR "${OPENVINO_INCLUDE_PATH_DEV}")
        elseif(EXISTS "${OPENVINO_INCLUDE_PATH_DEV}/inference_engine.hpp") # C++ header as proxy
            set(OPENVINO_PRIMARY_INCLUDE_DIR "${OPENVINO_INCLUDE_PATH_DEV}")
        else()
            message(WARNING "OpenVINO C API headers not found in expected locations under ${OPENVINO_DIR}.")
        endif()

        if(OPENVINO_PRIMARY_INCLUDE_DIR)
            include_directories(${OPENVINO_PRIMARY_INCLUDE_DIR})
            set(OpenVINO_C_INCLUDE_DIR ${OPENVINO_PRIMARY_INCLUDE_DIR}) # For core/CMakeLists.txt
            message(STATUS "OpenVINO include directory set to: ${OPENVINO_PRIMARY_INCLUDE_DIR}")
        endif()

        set(OPENVINO_LIB_PATH_RUNTIME "${OPENVINO_DIR}/runtime/lib/intel64") # Assuming intel64
        set(OPENVINO_LIB_PATH_DEV "${OPENVINO_DIR}/deployment_tools/inference_engine/lib/intel64")

        if(EXISTS "${OPENVINO_LIB_PATH_RUNTIME}/libopenvino_c.so")
            set(OPENVINO_PRIMARY_LIB_DIR "${OPENVINO_LIB_PATH_RUNTIME}")
        elseif(EXISTS "${OPENVINO_LIB_PATH_DEV}/libopenvino_c.so")
            set(OPENVINO_PRIMARY_LIB_DIR "${OPENVINO_LIB_PATH_DEV}")
        else()
            message(WARNING "OpenVINO C library (libopenvino_c.so) not found in expected locations under ${OPENVINO_DIR}.")
        endif()

        if(OPENVINO_PRIMARY_LIB_DIR)
            link_directories(${OPENVINO_PRIMARY_LIB_DIR})
            set(OpenVINO_C_LIBRARIES openvino_c openvino) # Library names
            set(OpenVINO_C_LIBRARY_DIR ${OPENVINO_PRIMARY_LIB_DIR}) # For core/CMakeLists.txt
            # For RPATH, this can be set per target:
            # set_target_properties(target PROPERTIES INSTALL_RPATH "${OPENVINO_PRIMARY_LIB_DIR}")
            # Or globally for executables:
            # set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-rpath,${OPENVINO_PRIMARY_LIB_DIR}")
            message(STATUS "OpenVINO library directory set to: ${OPENVINO_PRIMARY_LIB_DIR}")
            set(OPENVINO_FOUND TRUE) # Mark as found for conditional compilation elsewhere
        endif()

        if(NOT OPENVINO_FOUND)
             message(WARNING "OpenVINO was enabled but not fully configured. Check paths and library names.")
        endif()

    elseif(USE_OPENVINO)
        message(WARNING "USE_OPENVINO is ON but OPENVINO_DIR is not set or does not exist.")
        set(OPENVINO_FOUND FALSE)
    endif()
else()
    message(STATUS "OpenVINO support is disabled.")
    set(OPENVINO_FOUND FALSE)
endif()


# --- Add Subdirectories ---
add_subdirectory(core)
add_subdirectory(plugins)
add_subdirectory(tests)

# --- Enable Testing ---
enable_testing()

message(STATUS "Root CMakeLists.txt processing complete.")

# --- Helper function for adding tests that might need plugins ---
# (This function might be more useful if defined before add_subdirectory(core) if core tests use it)
# Or, it can be defined in core/CMakeLists.txt if only core tests need it.
# For now, keeping it here as a general utility.
function(add_spindex_test test_target_name test_source_files)
    add_executable(${test_target_name} ${test_source_files})
    # Link common libraries for tests, or make this part of the function arguments
    # target_link_libraries(${test_target_name} PRIVATE spindex_core_lib Threads::Threads ${CMAKE_DL_LIBS})
    add_test(NAME ${test_target_name} COMMAND ${test_target_name})
    # Example of setting plugin directory for tests that need it:
    # get_target_property(plugin_dir_for_test openvino_classifier_plugin LIBRARY_OUTPUT_DIRECTORY)
    # set_tests_properties(${test_target_name} PROPERTIES ENVIRONMENT "SPINDEX_PLUGIN_DIR=${plugin_dir_for_test}")
endfunction()

# Example of how to pass RPATH to a specific target if needed (e.g. OpenVINO plugins/tests)
# This is more of a note for when those targets are created.
# if(OPENVINO_FOUND AND OPENVINO_PRIMARY_LIB_DIR)
#   set_property(TARGET some_openvino_target APPEND PROPERTY LINK_FLAGS "-Wl,-rpath,${OPENVINO_PRIMARY_LIB_DIR}")
# endif()
