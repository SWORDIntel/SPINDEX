# --- Search Algorithm Plugins CMakeLists.txt ---

macro(add_search_algorithm_plugin plugin_id source_file avx_flags_list)
    set(plugin_target_name "${plugin_id}_so")
    set(output_name_base "${plugin_id}")

    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${source_file}")
        add_library(${plugin_target_name} MODULE ${source_file})
        set_target_properties(${plugin_target_name} PROPERTIES PREFIX "")
        set_target_properties(${plugin_target_name} PROPERTIES OUTPUT_NAME "${output_name_base}")

        target_include_directories(${plugin_target_name} PUBLIC
            "${CMAKE_SOURCE_DIR}/include"
        )

        target_compile_options(${plugin_target_name} PRIVATE ${CMAKE_C_FLAGS})

        if(avx_flags_list)
            include(CheckCCompilerFlag)
            foreach(flag IN LISTS avx_flags_list) # Iterate over list of flags
                string(REGEX REPLACE "^-" "" flag_var_name ${flag}) # Create a valid variable name part from flag
                string(REPLACE "=" "_" flag_var_name ${flag_var_name}) # e.g. -mavx -> mavx
                string(REPLACE "." "_" flag_var_name ${flag_var_name})

                check_c_compiler_flag("${flag}" COMPILER_SUPPORTS_${flag_var_name})
                if(COMPILER_SUPPORTS_${flag_var_name})
                    target_compile_options(${plugin_target_name} PRIVATE ${flag})
                else()
                    message(WARNING "C compiler does not support flag: ${flag}. Plugin ${plugin_target_name} might not build optimally or correctly.")
                endif()
            endforeach()
        endif()

        target_link_libraries(${plugin_target_name} PRIVATE spindex_core_lib) # For hardware_detection.o and utils.o

        list(APPEND SPINDEX_ALL_PLUGIN_TARGETS ${plugin_target_name})
        set(SPINDEX_ALL_PLUGIN_TARGETS ${SPINDEX_ALL_PLUGIN_TARGETS} CACHE INTERNAL "List of all plugin targets" FORCE)
        message(STATUS "Configured search algorithm plugin: ${plugin_target_name} (${output_name_base}.so)")
    else()
        message(WARNING "Source file ${source_file} not found for ${plugin_id}.")
    endif()
endmacro()

set(AVX512_PLUGIN_ID "avx512_phrase_search")
set(AVX512_PLUGIN_SRC "avx512_phrase_search.c")
set(AVX512_COMPILE_FLAGS_LIST -mavx512f -mavx512bw) # CMake list (semicolon or space separated)

option(BUILD_AVX512_PLUGIN "Build the AVX-512 search plugin" ON)

if(BUILD_AVX512_PLUGIN)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${AVX512_PLUGIN_SRC}")
        add_search_algorithm_plugin(${AVX512_PLUGIN_ID} ${AVX512_PLUGIN_SRC} "${AVX512_COMPILE_FLAGS_LIST}")
    else()
        message(STATUS "${AVX512_PLUGIN_SRC} not found, AVX-512 plugin will be skipped even if BUILD_AVX512_PLUGIN=ON.")
        set(BUILD_AVX512_PLUGIN OFF) # Ensure it's off if source is missing
    endif()
else()
    message(STATUS "AVX-512 plugin build is disabled by CMake option BUILD_AVX512_PLUGIN.")
endif()

# --- Search Algorithm Plugin Tests ---
macro(add_search_algorithm_test test_id plugin_id_base test_src_file avx_flags_list_for_test)
    set(test_target_name "test_${test_id}")
    set(plugin_so_target_name "${plugin_id_base}_so")

    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${test_src_file}")
        if(TARGET ${plugin_so_target_name}) # Check if the plugin target itself was created
            add_executable(${test_target_name} ${test_src_file})
            target_compile_options(${test_target_name} PRIVATE ${CMAKE_C_FLAGS})

            if(avx_flags_list_for_test)
                include(CheckCCompilerFlag) # Ensure it's included
                foreach(flag IN LISTS avx_flags_list_for_test)
                    string(REGEX REPLACE "^-" "" flag_var_name ${flag})
                    string(REPLACE "=" "_" flag_var_name ${flag_var_name})
                    string(REPLACE "." "_" flag_var_name ${flag_var_name})
                    # Use the already checked variable from plugin if flags are identical or re-check
                    if(DEFINED COMPILER_SUPPORTS_${flag_var_name} AND COMPILER_SUPPORTS_${flag_var_name})
                         target_compile_options(${test_target_name} PRIVATE ${flag})
                    else() # Re-check if not defined or for clarity
                        check_c_compiler_flag("${flag}" TEST_COMPILER_SUPPORTS_${flag_var_name})
                        if(TEST_COMPILER_SUPPORTS_${flag_var_name})
                            target_compile_options(${test_target_name} PRIVATE ${flag})
                        endif()
                    endif()
                endforeach()
            endif()

            target_include_directories(${test_target_name} PRIVATE "${CMAKE_SOURCE_DIR}/include")
            target_link_libraries(${test_target_name} PRIVATE spindex_core_lib ${plugin_so_target_name} ${CMAKE_DL_LIBS} m) # Link libm
            add_test(NAME ${test_target_name} COMMAND ${test_target_name})
            message(STATUS "Added search algorithm test: ${test_target_name} for ${plugin_so_target_name}")
        else()
            message(WARNING "Plugin target ${plugin_so_target_name} not found (or AVX plugin disabled by option/missing source). Test ${test_target_name} will be skipped.")
        endif()
    else()
        message(STATUS "Test source file ${test_src_file} not found for ${test_id} test. Skipping.")
    endif()
endmacro()

if(BUILD_AVX512_PLUGIN) # Only add test if plugin is being built
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test_avx512_phrase_search.c")
        add_search_algorithm_test(avx512_phrase_search ${AVX512_PLUGIN_ID} test_avx512_phrase_search.c "${AVX512_COMPILE_FLAGS_LIST}")
    endif()
endif()

message(STATUS "Search algorithm plugins CMakeLists.txt processing complete.")
