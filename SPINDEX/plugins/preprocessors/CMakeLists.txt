# SPINDEX/plugins/preprocessors/CMakeLists.txt
cmake_minimum_required(VERSION 3.15)

set(PREPROCESSOR_PLUGIN_SOURCES
    json_preprocessor.c
    txt_preprocessor.c
    csv_preprocessor.c
    doc_preprocessor.c
    xls_preprocessor.c
    pdf_preprocessor.c
    rtf_preprocessor.c
    dic_preprocessor.c
    sqlite_preprocessor.c
)

foreach(plugin_source_file ${PREPROCESSOR_PLUGIN_SOURCES})
    get_filename_component(plugin_name ${plugin_source_file} NAME_WE)
    add_library(${plugin_name} SHARED ${CMAKE_CURRENT_SOURCE_DIR}/${plugin_source_file})

    # Include directories are expected to be handled by the top-level CMakeLists.txt
    # (e.g., via include_directories(${CMAKE_SOURCE_DIR}/include))
    # or explicitly added here if needed for specific plugins:
    # target_include_directories(${plugin_name} PRIVATE ${CMAKE_SOURCE_DIR}/include)

    # No specific link libraries for these preprocessor plugins.
    # Output directory for .so files is handled by CMAKE_LIBRARY_OUTPUT_DIRECTORY.
endforeach()

# Add tests for specific preprocessors
set(PREPROCESSOR_TEST_SOURCES
    test_doc_preprocessor.c
    test_xls_preprocessor.c
    test_pdf_preprocessor.c
    test_rtf_preprocessor.c
    test_dic_preprocessor.c
    test_sqlite_preprocessor.c
)

foreach(test_source ${PREPROCESSOR_TEST_SOURCES})
    get_filename_component(test_name ${test_source} NAME_WE) # e.g., test_doc_preprocessor
    # Derive the plugin library name from the test source file name
    string(REPLACE "test_" "" plugin_name_ref ${test_name}) # e.g., doc_preprocessor

    add_executable(${test_name} ${CMAKE_CURRENT_SOURCE_DIR}/${test_source})
    # Link against the corresponding preprocessor plugin and spindex_core (for utils, etc.)
    target_link_libraries(${test_name} PRIVATE ${plugin_name_ref} spindex_core)
    add_test(NAME ${test_name} COMMAND ${test_name})
endforeach()
