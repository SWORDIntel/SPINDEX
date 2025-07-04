# --- SPINDEX Plugins Aggregator CMakeLists.txt ---

# This file adds subdirectories for each plugin category.
# AEGIS modules are handled in core/CMakeLists.txt as they are statically linked.

# Define a custom target that other targets (like tests needing all plugins) can depend on.
# This target itself doesn't build anything but serves as a dependency hub.
add_custom_target(plugins_built DEPENDS spindex_plugins_list) # Depends on a list of all plugin targets

# Global list to hold all plugin library targets
set(SPINDEX_ALL_PLUGIN_TARGETS "" CACHE INTERNAL "List of all plugin targets")

# Helper function to add a plugin subdirectory
macro(add_plugin_subdirectory subdir_name)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${subdir_name}/CMakeLists.txt")
        add_subdirectory(${subdir_name})
        message(STATUS "Added plugin subdirectory: ${subdir_name}")
    elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${subdir_name}")
        message(WARNING "Plugin subdirectory ${subdir_name} exists, but its CMakeLists.txt was not found. Skipping.")
    else()
        message(STATUS "Plugin subdirectory ${subdir_name} not found. Skipping.")
    endif()
endmacro()

add_plugin_subdirectory(dummy_plugin)
add_plugin_subdirectory(preprocessors)
add_plugin_subdirectory(parsers)
add_plugin_subdirectory(indexers)
add_plugin_subdirectory(analyzers)
add_plugin_subdirectory(classifiers)
add_plugin_subdirectory(search_algorithms)
# add_plugin_subdirectory(decompressors) # Add when these are actively being built
# add_plugin_subdirectory(optimizations)
# add_plugin_subdirectory(security)

# Create an interface library that groups all actual plugin targets.
# Tests or other components can link against this interface library
# to ensure all plugins are built before them.
add_library(spindex_plugins_list INTERFACE)
if(SPINDEX_ALL_PLUGIN_TARGETS)
    add_dependencies(spindex_plugins_list ${SPINDEX_ALL_PLUGIN_TARGETS})
endif()


message(STATUS "Plugins aggregator CMakeLists.txt processing complete.")
# Make sure the plugins directory is added in the root CMakeLists.txt
# Ensure `add_subdirectory(plugins)` is present in `SPINDEX/CMakeLists.txt`
# (This will be done by editing the root CMakeLists.txt later if not already there)
