# SPINDEX/plugins/CMakeLists.txt

# Add subdirectories for different categories of plugins
add_subdirectory(dummy_plugin)
add_subdirectory(preprocessors)
add_subdirectory(parsers)
add_subdirectory(indexers)
add_subdirectory(analyzers)
add_subdirectory(classifiers)
add_subdirectory(search_algorithms)

# Note: AEGIS modules are in a separate directory (plugins/aegis_modules)
# and already handled by the top-level SPINDEX/CMakeLists.txt.
# Decompressors and other plugin types can be added here if/when they are migrated.
