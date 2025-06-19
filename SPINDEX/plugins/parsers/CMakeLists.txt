# SPINDEX/plugins/parsers/CMakeLists.txt
cmake_minimum_required(VERSION 3.15)

set(PARSER_PLUGIN_SOURCES
    json_parser.c
    txt_parser.c
    csv_parser.c
    doc_parser.c
    xls_parser.c
    pdf_parser.c
    rtf_parser.c
    dic_parser.c
    sqlite_parser.c
)

foreach(plugin_source_file ${PARSER_PLUGIN_SOURCES})
    get_filename_component(plugin_name ${plugin_source_file} NAME_WE)
    add_library(${plugin_name} SHARED ${CMAKE_CURRENT_SOURCE_DIR}/${plugin_source_file})

    # Include directories are expected to be handled by the top-level CMakeLists.txt
    # (e.g., via include_directories(${CMAKE_SOURCE_DIR}/include))
    # or explicitly added here if needed for specific plugins:
    # target_include_directories(${plugin_name} PRIVATE ${CMAKE_SOURCE_DIR}/include)

    # No specific link libraries for these parser plugins.
    # Output directory for .so files is handled by CMAKE_LIBRARY_OUTPUT_DIRECTORY.
endforeach()

# Add tests for specific parsers
set(PARSER_TEST_SOURCES
    test_doc_parser.c
    test_xls_parser.c
    test_pdf_parser.c
    test_rtf_parser.c
    test_dic_parser.c
    test_sqlite_parser.c
)

foreach(test_source ${PARSER_TEST_SOURCES})
    get_filename_component(test_name ${test_source} NAME_WE) # e.g., test_doc_parser
    # Derive the plugin library name from the test source file name
    # e.g., test_doc_parser.c tests doc_parser.so
    string(REPLACE "test_" "" plugin_name_ref ${test_name})

    add_executable(${test_name} ${CMAKE_CURRENT_SOURCE_DIR}/${test_source})
    # Link against the corresponding parser plugin and spindex_core (for utils, etc.)
    target_link_libraries(${test_name} PRIVATE ${plugin_name_ref} spindex_core)
    add_test(NAME ${test_name} COMMAND ${test_name})
endforeach()
