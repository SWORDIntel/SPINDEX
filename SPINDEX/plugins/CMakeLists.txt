# SPINDEX/plugins/CMakeLists.txt

# Add subdirectories for different categories of plugins
add_subdirectory(dummy_plugin)
add_subdirectory(preprocessors)
add_subdirectory(parsers)
add_subdirectory(indexers)
add_subdirectory(analyzers)
add_subdirectory(classifiers)
add_subdirectory(search_algorithms)
add_subdirectory(decompressors)

# Note: AEGIS modules are in a separate directory (plugins/aegis_modules)
# and already handled by the top-level SPINDEX/CMakeLists.txt.
# Other plugin types can be added here if/when they are migrated.

# --- Plugin Staging for Core Tests ---
set(CORE_TEST_PLUGIN_STAGING_DIR ${PROJECT_BINARY_DIR}/plugins_for_test)

# List all plugin library targets that should be staged.
# These names must match the add_library() names for each plugin.
set(ALL_PLUGIN_LIBRARY_TARGETS
    dummy_plugin
    json_preprocessor
    txt_preprocessor
    csv_preprocessor
    doc_preprocessor
    xls_preprocessor
    pdf_preprocessor
    rtf_preprocessor
    dic_preprocessor
    sqlite_preprocessor
    json_parser
    txt_parser
    csv_parser
    doc_parser
    xls_parser
    pdf_parser
    rtf_parser
    dic_parser
    sqlite_parser
    elasticsearch_indexer
    content_analyzer
    openvino_classifier
    avx512_phrase_search
    # Add new decompressor plugins
    gzip_decompressor
    zip_decompressor
    zstd_decompressor
)

# Create a custom target that will be responsible for staging these plugins.
# Core tests that need these plugins will depend on this target.
add_custom_target(stage_all_plugins_for_core_tests
    COMMENT "Staging all relevant plugins to ${CORE_TEST_PLUGIN_STAGING_DIR}"
)

# Command to create the staging directory.
# This command is part of the 'stage_all_plugins_for_core_tests' target's execution.
add_custom_command(
    TARGET stage_all_plugins_for_core_tests
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CORE_TEST_PLUGIN_STAGING_DIR}
    COMMENT "Creating plugin staging directory: ${CORE_TEST_PLUGIN_STAGING_DIR}"
    VERBATIM
)

# For each plugin library, make 'stage_all_plugins_for_core_tests' depend on it,
# and add a command to copy the plugin to the staging directory.
foreach(plugin_lib_target ${ALL_PLUGIN_LIBRARY_TARGETS})
    # Ensure the plugin library is built before attempting to stage it.
    add_dependencies(stage_all_plugins_for_core_tests ${plugin_lib_target})

    # Add a command to copy the built plugin library to the staging directory.
    # This command is also part of the 'stage_all_plugins_for_core_tests' target's execution.
    add_custom_command(
        TARGET stage_all_plugins_for_core_tests
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_FILE:${plugin_lib_target}>
                ${CORE_TEST_PLUGIN_STAGING_DIR}/$<TARGET_FILE_NAME:${plugin_lib_target}>
        COMMENT "Staging $<TARGET_FILE_NAME:${plugin_lib_target}> to ${CORE_TEST_PLUGIN_STAGING_DIR}"
        VERBATIM
    )
endforeach()
