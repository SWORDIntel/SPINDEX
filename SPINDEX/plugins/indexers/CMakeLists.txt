# --- Indexer Plugins CMakeLists.txt ---

macro(add_indexer_plugin plugin_id source_file)
    set(plugin_target_name "${plugin_id}_indexer_so")

    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${source_file}")
        add_library(${plugin_target_name} MODULE ${source_file})
        set_target_properties(${plugin_target_name} PROPERTIES PREFIX "")
        set_target_properties(${plugin_target_name} PROPERTIES OUTPUT_NAME "${plugin_id}_indexer")

        target_include_directories(${plugin_target_name} PUBLIC
            "${CMAKE_SOURCE_DIR}/include"
        )

        if("${plugin_id}" STREQUAL "elasticsearch")
            # elasticsearch_indexer links against elasticsearch_client.o and utils.o from spindex_core_lib
            target_link_libraries(${plugin_target_name} PRIVATE spindex_core_lib)
            # CURL is already linked to spindex_core_lib, so it should be available.
            # If spindex_core_lib was an INTERFACE library, explicit linking here would be needed.
        endif()

        list(APPEND SPINDEX_ALL_PLUGIN_TARGETS ${plugin_target_name})
        set(SPINDEX_ALL_PLUGIN_TARGETS ${SPINDEX_ALL_PLUGIN_TARGETS} CACHE INTERNAL "List of all plugin targets" FORCE)
        message(STATUS "Configured indexer plugin: ${plugin_target_name} (${plugin_id}_indexer.so)")
    else()
        message(WARNING "Source file ${source_file} not found for ${plugin_id}_indexer.")
    endif()
endmacro()

add_indexer_plugin(elasticsearch elasticsearch_indexer.c)

message(STATUS "Indexer plugins CMakeLists.txt processing complete.")
