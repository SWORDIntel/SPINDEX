# --- Dummy Plugin CMakeLists.txt ---
set(plugin_name "dummy_plugin_so") # Changed name to avoid conflict if a static lib is also named dummy_plugin
set(plugin_source "dummy_plugin.c")

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${plugin_source}")
    add_library(${plugin_name} MODULE ${plugin_source})
    set_target_properties(${plugin_name} PROPERTIES PREFIX "")
    set_target_properties(${plugin_name} PROPERTIES OUTPUT_NAME "dummy_plugin") # Actual filename dummy_plugin.so

    target_include_directories(${plugin_name} PUBLIC
        "${CMAKE_SOURCE_DIR}/include"
    )
    target_compile_options(${plugin_name} PRIVATE ${CMAKE_C_FLAGS})
    # No specific link libraries for dummy plugin beyond what might be default (e.g. C runtime)

    list(APPEND SPINDEX_ALL_PLUGIN_TARGETS ${plugin_name})
    set(SPINDEX_ALL_PLUGIN_TARGETS ${SPINDEX_ALL_PLUGIN_TARGETS} CACHE INTERNAL "List of all plugin targets" FORCE)

    message(STATUS "Configured plugin: ${plugin_name} (dummy_plugin.so)")
else()
    message(WARNING "Source file ${plugin_source} not found for dummy_plugin.")
endif()
