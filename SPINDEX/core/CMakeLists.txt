# --- SPINDEX Core CMakeLists.txt ---

# Identify Core Source Files (excluding test files)
file(GLOB CORE_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/*.c"
)
list(FILTER CORE_SOURCES EXCLUDE REGEX "_test\\.c$")
list(FILTER CORE_SOURCES EXCLUDE REGEX "test_.*\\.c$") # More robust test file exclusion
list(FILTER CORE_SOURCES EXCLUDE REGEX "main\\.c$") # Exclude main.c if it's separate for the executable

# Identify AEGIS Module Source Files
# These are treated as part of the core application build as per Makefile.legacy
file(GLOB AEGIS_MODULE_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/../plugins/aegis_modules/*.c"
)

# --- Create a static library for core components + AEGIS modules ---
add_library(spindex_core_lib STATIC
    ${CORE_SOURCES}
    ${AEGIS_MODULE_SOURCES}
)

# Include directories for spindex_core_lib
target_include_directories(spindex_core_lib PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/../include"  # Main include directory
    "${CMAKE_CURRENT_SOURCE_DIR}/../plugins/aegis_modules" # AEGIS headers
    "${CMAKE_CURRENT_SOURCE_DIR}" # For core internal headers if any
)

# Propagate usage requirements for include directories from dependencies
if(Jansson_FOUND)
    if(TARGET Jansson::Jansson)
        # Handled by target_link_libraries with Jansson::Jansson
    else()
        target_include_directories(spindex_core_lib INTERFACE ${JANSSON_INCLUDE_DIRS})
    endif()
endif()
if(GEOS_FOUND)
    if(TARGET GEOS::GEOS)
        # Handled by target_link_libraries with GEOS::GEOS
    else()
        # GEOS_INCLUDE_DIRS was set from geos-config in root CMakeLists.txt
        # This was added globally via include_directories() command.
        # If we want to be more specific:
        # target_include_directories(spindex_core_lib INTERFACE ${GEOS_INCLUDE_DIRS})
    endif()
endif()
if(OPENVINO_FOUND AND OpenVINO_C_INCLUDE_DIR)
    target_include_directories(spindex_core_lib INTERFACE ${OpenVINO_C_INCLUDE_DIR})
endif()

# Link external libraries to spindex_core_lib
target_link_libraries(spindex_core_lib PUBLIC
    Threads::Threads
    CURL::CURL
)
if(Jansson_FOUND)
    if(TARGET Jansson::Jansson)
        target_link_libraries(spindex_core_lib PUBLIC Jansson::Jansson)
    else()
        target_link_libraries(spindex_core_lib PUBLIC ${JANSSON_LIBRARIES})
    endif()
endif()
if(GEOS_FOUND)
    if(TARGET GEOS::GEOS)
        target_link_libraries(spindex_core_lib PUBLIC GEOS::GEOS)
    else()
        # GEOS_CONFIG_LIBRARIES is a string of flags e.g. "-L/path -lgeos_c"
        # This needs to be parsed or added to LINK_FLAGS.
        # For now, assuming it's just library names or paths that CMake can handle.
        # A better way is to parse it in root and provide a list of libraries.
        # Or, if FindGEOS.cmake sets GEOS_LIBRARIES:
        # target_link_libraries(spindex_core_lib PUBLIC ${GEOS_LIBRARIES})
        # For now, we rely on link_directories and direct lib names if GEOS_CONFIG_LIBRARIES is simple.
        # This is a common tricky part when moving from Makefiles.
        # Let's assume GEOS_CONFIG_LIBRARIES contains something like "geos_c" or full paths.
        # If it contains -L flags, those should be handled by link_directories in root.
        separate_arguments(GEOS_LINK_ARGS NATIVE_COMMAND ${GEOS_CONFIG_LIBRARIES})
        target_link_libraries(spindex_core_lib PUBLIC ${GEOS_LINK_ARGS})
    endif()
endif()

if(OPENVINO_FOUND AND OpenVINO_C_LIBRARIES)
    target_link_libraries(spindex_core_lib PUBLIC ${OpenVINO_C_LIBRARIES})
    if(OpenVINO_C_LIBRARY_DIR AND (CMAKE_SYSTEM_NAME STREQUAL "Linux" OR CMAKE_SYSTEM_NAME STREQUAL "Darwin"))
        # RPATH for OpenVINO libraries if they are in a non-standard location
        target_link_options(spindex_core_lib INTERFACE "-Wl,-rpath,${OpenVINO_C_LIBRARY_DIR}")
    endif()
endif()

# Set common compile flags for the core library
target_compile_options(spindex_core_lib INTERFACE ${CMAKE_C_FLAGS})


# --- Main Application: spindex_app ---
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/main.c")
    add_executable(spindex_app main.c)
    target_link_libraries(spindex_app PRIVATE spindex_core_lib ${CMAKE_DL_LIBS}) # For dlopen/dlsym
    target_compile_options(spindex_app PRIVATE ${CMAKE_C_FLAGS})

    if(OPENVINO_FOUND AND OpenVINO_C_LIBRARY_DIR AND (CMAKE_SYSTEM_NAME STREQUAL "Linux" OR CMAKE_SYSTEM_NAME STREQUAL "Darwin"))
         target_link_options(spindex_app PRIVATE "-Wl,-rpath,${OpenVINO_C_LIBRARY_DIR}")
    endif()
else()
    message(WARNING "main.c not found in core directory. spindex_app target not created.")
endif()


# --- Core Test Programs ---
macro(add_core_test test_exe_name test_src_file)
    add_executable(${test_exe_name} ${test_src_file})
    target_link_libraries(${test_exe_name} PRIVATE spindex_core_lib ${CMAKE_DL_LIBS})
    target_compile_options(${test_exe_name} PRIVATE ${CMAKE_C_FLAGS})
    add_test(NAME ${test_exe_name} COMMAND ${test_exe_name})

    if(OPENVINO_FOUND AND OpenVINO_C_LIBRARY_DIR AND (CMAKE_SYSTEM_NAME STREQUAL "Linux" OR CMAKE_SYSTEM_NAME STREQUAL "Darwin"))
        # Example: if test_ingestion_pipeline loads OpenVINO plugins indirectly
        if("${test_exe_name}" STREQUAL "test_ingestion_pipeline" OR "${test_exe_name}" STREQUAL "test_elasticsearch_indexing")
             target_link_options(${test_exe_name} PRIVATE "-Wl,-rpath,${OpenVINO_C_LIBRARY_DIR}")
        endif()
    endif()

    # For tests that load plugins
    if("${test_exe_name}" STREQUAL "test_ingestion_pipeline" OR \
       "${test_exe_name}" STREQUAL "test_elasticsearch_indexing" OR \
       "${test_exe_name}" STREQUAL "test_plugin_loader"
      )
        set_tests_properties(${test_exe_name} PROPERTIES
            ENVIRONMENT "SPINDEX_PLUGIN_DIR=${CMAKE_LIBRARY_OUTPUT_DIRECTORY}")
        # Ensure plugins are built before these tests run
        add_dependencies(${test_exe_name} spindex_plugins_list)
    endif()
endmacro()

# Define core tests
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test_config_manager.c")
    add_core_test(test_config_manager test_config_manager.c)
endif()
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test_plugin_loader.c")
    add_core_test(test_plugin_loader test_plugin_loader.c)
endif()
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test_logger.c")
    add_core_test(test_logger test_logger.c)
endif()
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test_hardware_detection.c")
    add_core_test(test_hardware_detection test_hardware_detection.c)
endif()
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test_ingestion_pipeline.c")
    add_core_test(test_ingestion_pipeline test_ingestion_pipeline.c)
endif()
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test_elasticsearch_client.c")
    add_core_test(test_elasticsearch_client test_elasticsearch_client.c)
endif()
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test_elasticsearch_indexing.c")
    add_core_test(test_elasticsearch_indexing test_elasticsearch_indexing.c)
endif()
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test_complex_geometry_handler.c")
    add_core_test(test_complex_geometry_handler test_complex_geometry_handler.c)
endif()
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test_nlp_ner_parser.c")
    add_core_test(test_nlp_ner_parser test_nlp_ner_parser.c)
endif()
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test_geometric_validator.c")
    add_core_test(test_geometric_validator test_geometric_validator.c)
endif()

message(STATUS "Core CMakeLists.txt processing complete.")
