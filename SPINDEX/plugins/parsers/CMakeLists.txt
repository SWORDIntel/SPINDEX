# --- Parser Plugins CMakeLists.txt ---

macro(add_parser_plugin plugin_id source_file)
    set(plugin_target_name "${plugin_id}_parser_so")

    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${source_file}")
        add_library(${plugin_target_name} MODULE ${source_file})
        set_target_properties(${plugin_target_name} PROPERTIES PREFIX "")
        set_target_properties(${plugin_target_name} PROPERTIES OUTPUT_NAME "${plugin_id}_parser")

        target_include_directories(${plugin_target_name} PUBLIC
            "${CMAKE_SOURCE_DIR}/include"
        )
        target_compile_options(${plugin_target_name} PRIVATE ${CMAKE_C_FLAGS})

        if("${plugin_id}" STREQUAL "json") # json_parser needs Jansson
            # Jansson_FOUND is checked in root CMakeLists.txt when using find_package(Jansson CONFIG REQUIRED)
            # We assume Jansson::jansson imported target is available from Conan
            target_link_libraries(${plugin_target_name} PRIVATE Jansson::jansson)
        endif()

        list(APPEND SPINDEX_ALL_PLUGIN_TARGETS ${plugin_target_name})
        set(SPINDEX_ALL_PLUGIN_TARGETS ${SPINDEX_ALL_PLUGIN_TARGETS} CACHE INTERNAL "List of all plugin targets" FORCE)
        message(STATUS "Configured parser plugin: ${plugin_target_name} (${plugin_id}_parser.so)")
    else()
        message(WARNING "Source file ${source_file} not found for ${plugin_id}_parser.")
    endif()
endmacro()

add_parser_plugin(json json_parser.c)
add_parser_plugin(txt txt_parser.c)
add_parser_plugin(csv csv_parser.c)
add_parser_plugin(doc doc_parser.c)
add_parser_plugin(xls xls_parser.c)
add_parser_plugin(pdf pdf_parser.c)
add_parser_plugin(rtf rtf_parser.c)
add_parser_plugin(dic dic_parser.c)
add_parser_plugin(sqlite sqlite_parser.c)

# --- Parser Plugin Tests ---
macro(add_parser_test test_id plugin_id test_src_file)
    set(test_target_name "test_${test_id}_parser")
    set(plugin_so_target_name "${plugin_id}_parser_so")

    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${test_src_file}")
        if(TARGET ${plugin_so_target_name})
            add_executable(${test_target_name} ${test_src_file})
            target_compile_options(${test_target_name} PRIVATE ${CMAKE_C_FLAGS})
            target_include_directories(${test_target_name} PRIVATE "${CMAKE_SOURCE_DIR}/include")
            target_link_libraries(${test_target_name} PRIVATE spindex_core_lib ${plugin_so_target_name} ${CMAKE_DL_LIBS})
            add_test(NAME ${test_target_name} COMMAND ${test_target_name})
            message(STATUS "Added parser test: ${test_target_name} for ${plugin_so_target_name}")
        else()
            message(WARNING "Plugin target ${plugin_so_target_name} not found for test ${test_target_name}.")
        endif()
    else()
        message(STATUS "Test source file ${test_src_file} not found for ${test_id}_parser test. Skipping.")
    endif()
endmacro()

add_parser_test(doc doc test_doc_parser.c)
add_parser_test(xls xls test_xls_parser.c)
add_parser_test(pdf pdf test_pdf_parser.c)
add_parser_test(rtf rtf test_rtf_parser.c)
add_parser_test(dic dic test_dic_parser.c)
add_parser_test(sqlite sqlite test_sqlite_parser.c)

message(STATUS "Parser plugins CMakeLists.txt processing complete.")
