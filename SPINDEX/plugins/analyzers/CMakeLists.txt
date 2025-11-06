# --- Analyzer Plugins CMakeLists.txt ---

macro(add_analyzer_plugin plugin_id source_file)
    set(plugin_target_name "${plugin_id}_analyzer_so")

    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${source_file}")
        add_library(${plugin_target_name} MODULE ${source_file})
        set_target_properties(${plugin_target_name} PROPERTIES PREFIX "")
        set_target_properties(${plugin_target_name} PROPERTIES OUTPUT_NAME "${plugin_id}_analyzer")

        target_include_directories(${plugin_target_name} PUBLIC
            "${CMAKE_SOURCE_DIR}/include"
        )

        list(APPEND SPINDEX_ALL_PLUGIN_TARGETS ${plugin_target_name})
        set(SPINDEX_ALL_PLUGIN_TARGETS ${SPINDEX_ALL_PLUGIN_TARGETS} CACHE INTERNAL "List of all plugin targets" FORCE)
        message(STATUS "Configured analyzer plugin: ${plugin_target_name} (${plugin_id}_analyzer.so)")
    else()
        message(WARNING "Source file ${source_file} not found for ${plugin_id}_analyzer.")
    endif()
endmacro()

add_analyzer_plugin(content content_analyzer.c)

message(STATUS "Analyzer plugins CMakeLists.txt processing complete.")
