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
# With Conan and CMakeDeps, linking the imported target (e.g., Jansson::Jansson)
# handles include directories automatically. So, manual target_include_directories for these is not needed.

# Link external libraries to spindex_core_lib
target_link_libraries(spindex_core_lib PUBLIC
    Threads::Threads # System library
    CURL::libcurl    # Conan imported target (typical name)
    Jansson::jansson # Conan imported target (typical name)
    GEOS::geos       # Conan imported target (typical name)
)

if(OPENVINO_FOUND)
    # Assuming OpenVINO Conan recipe provides an imported target like OpenVINO::OpenVINO or OpenVINO::runtime etc.
    # The exact target name depends on the Conan recipe for OpenVINO.
    # Common names could be OpenVINO::OpenVINO, OpenVINO::openvino, or specific components.
    # Let's assume OpenVINO::OpenVINO is a common pattern or an interface target.
    # If the recipe provides specific component targets (e.g., OpenVINO::inference_engine_c_api),
    # those might be linked instead.
    target_link_libraries(spindex_core_lib PUBLIC OpenVINO::OpenVINO) # Example target name
    # RPATH settings are usually handled by Conan's toolchain or the recipe itself for its artifacts.
    # Manual RPATH for OpenVINO might not be needed here if Conan sets it up.
    # If still needed:
    # get_target_property(OPENVINO_LIB_DIR OpenVINO::OpenVINO INTERFACE_LINK_LIBRARIES)
    # string(REGEX REPLACE "/lib[^/]*$" "" OPENVINO_INSTALL_PREFIX "${OPENVINO_LIB_DIR}")
    # if(OPENVINO_INSTALL_PREFIX AND (CMAKE_SYSTEM_NAME STREQUAL "Linux" OR CMAKE_SYSTEM_NAME STREQUAL "Darwin"))
    #    target_link_options(spindex_core_lib INTERFACE "-Wl,-rpath,${OPENVINO_INSTALL_PREFIX}/lib") # Adjust path as needed
    # endif()
endif()

# Set common compile flags for the core library
target_compile_options(spindex_core_lib INTERFACE ${CMAKE_C_FLAGS})


# --- Main Application: spindex_app ---
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/main.c")
    add_executable(spindex_app main.c)
    target_link_libraries(spindex_app PRIVATE spindex_core_lib ${CMAKE_DL_LIBS}) # For dlopen/dlsym
    target_compile_options(spindex_app PRIVATE ${CMAKE_C_FLAGS})

    # RPATH for OpenVINO on the final executable if linked dynamically through spindex_core_lib
    # and if not handled by Conan toolchain for the executable itself.
    # This might be redundant if spindex_core_lib's INTERFACE rpath is effective or Conan handles it.
    # if(OPENVINO_FOUND AND <condition_for_openvino_lib_path_known>)
    #      target_link_options(spindex_app PRIVATE "-Wl,-rpath,${OpenVINO_LIBRARY_DIR_FROM_CONAN_OR_TARGET}")
    # endif()
else()
    message(WARNING "main.c not found in core directory. spindex_app target not created.")
endif()


# --- Core Test Programs ---
macro(add_core_test test_exe_name test_src_file)
    add_executable(${test_exe_name} ${test_src_file})
    target_link_libraries(${test_exe_name} PRIVATE spindex_core_lib ${CMAKE_DL_LIBS})
    target_compile_options(${test_exe_name} PRIVATE ${CMAKE_C_FLAGS})
    add_test(NAME ${test_exe_name} COMMAND ${test_exe_name})

    # RPATH for OpenVINO on tests, if they link OpenVINO and it's not handled by Conan toolchain.
    # if(OPENVINO_FOUND AND <condition_for_openvino_lib_path_known>)
    #     if("${test_exe_name}" STREQUAL "test_ingestion_pipeline" OR "${test_exe_name}" STREQUAL "test_elasticsearch_indexing")
    #          target_link_options(${test_exe_name} PRIVATE "-Wl,-rpath,${OpenVINO_LIBRARY_DIR_FROM_CONAN_OR_TARGET}")
    #     endif()
    # endif()

    # For tests that load plugins
    if("${test_exe_name}" STREQUAL "test_ingestion_pipeline" OR
       "${test_exe_name}" STREQUAL "test_elasticsearch_indexing" OR
       "${test_exe_name}" STREQUAL "test_plugin_loader")
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
