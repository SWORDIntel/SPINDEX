# --- Preprocessor Plugins CMakeLists.txt ---

macro(add_preprocessor_plugin plugin_id source_file)
    set(plugin_target_name "${plugin_id}_preprocessor_so")

    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${source_file}")
        add_library(${plugin_target_name} MODULE ${source_file})
        set_target_properties(${plugin_target_name} PROPERTIES PREFIX "")
        set_target_properties(${plugin_target_name} PROPERTIES OUTPUT_NAME "${plugin_id}_preprocessor")

        target_include_directories(${plugin_target_name} PUBLIC
            "${CMAKE_SOURCE_DIR}/include"
        )
        target_compile_options(${plugin_target_name} PRIVATE ${CMAKE_C_FLAGS})

        if("${plugin_id}" STREQUAL "json") # json_preprocessor needs Jansson
            if(Jansson_FOUND)
                if(TARGET Jansson::Jansson)
                    target_link_libraries(${plugin_target_name} PRIVATE Jansson::Jansson)
                else()
                    target_link_libraries(${plugin_target_name} PRIVATE ${JANSSON_LIBRARIES})
                endif()
            else()
                message(WARNING "Jansson not found, ${plugin_target_name} might fail to link or run.")
            endif()
        endif()

        list(APPEND SPINDEX_ALL_PLUGIN_TARGETS ${plugin_target_name})
        set(SPINDEX_ALL_PLUGIN_TARGETS ${SPINDEX_ALL_PLUGIN_TARGETS} CACHE INTERNAL "List of all plugin targets" FORCE)
        message(STATUS "Configured preprocessor plugin: ${plugin_target_name} (${plugin_id}_preprocessor.so)")
    else()
        message(WARNING "Source file ${source_file} not found for ${plugin_id}_preprocessor.")
    endif()
endmacro()

add_preprocessor_plugin(json json_preprocessor.c)
add_preprocessor_plugin(txt txt_preprocessor.c)
add_preprocessor_plugin(csv csv_preprocessor.c)
add_preprocessor_plugin(doc doc_preprocessor.c)
add_preprocessor_plugin(xls xls_preprocessor.c)
add_preprocessor_plugin(pdf pdf_preprocessor.c)
add_preprocessor_plugin(rtf rtf_preprocessor.c)
add_preprocessor_plugin(dic dic_preprocessor.c)
add_preprocessor_plugin(sqlite sqlite_preprocessor.c)

# --- Preprocessor Plugin Tests ---
macro(add_preprocessor_test test_id plugin_id test_src_file)
    set(test_target_name "test_${test_id}_preprocessor")
    set(plugin_so_target_name "${plugin_id}_preprocessor_so")

    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${test_src_file}")
        if(TARGET ${plugin_so_target_name})
            add_executable(${test_target_name} ${test_src_file})
            target_compile_options(${test_target_name} PRIVATE ${CMAKE_C_FLAGS})
            target_include_directories(${test_target_name} PRIVATE "${CMAKE_SOURCE_DIR}/include")
            target_link_libraries(${test_target_name} PRIVATE spindex_core_lib ${plugin_so_target_name} ${CMAKE_DL_LIBS})
            add_test(NAME ${test_target_name} COMMAND ${test_target_name})
            message(STATUS "Added preprocessor test: ${test_target_name} for ${plugin_so_target_name}")
        else()
            message(WARNING "Plugin target ${plugin_so_target_name} not found for test ${test_target_name}.")
        endif()
    else()
        message(STATUS "Test source file ${test_src_file} not found for ${test_id}_preprocessor test. Skipping.")
    endif()
endmacro()

add_preprocessor_test(doc doc test_doc_preprocessor.c)
add_preprocessor_test(xls xls test_xls_preprocessor.c)
add_preprocessor_test(pdf pdf test_pdf_preprocessor.c)
add_preprocessor_test(rtf rtf test_rtf_preprocessor.c)
add_preprocessor_test(dic dic test_dic_preprocessor.c)
add_preprocessor_test(sqlite sqlite test_sqlite_preprocessor.c)

message(STATUS "Preprocessor plugins CMakeLists.txt processing complete.")
