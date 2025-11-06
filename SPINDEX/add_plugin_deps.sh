#!/bin/bash
# Add missing dependency functions to all plugins

PLUGIN_DIRS=(
    "plugins/preprocessors"
    "plugins/parsers"
    "plugins/indexers"
    "plugins/analyzers"
    "plugins/dummy_plugin"
)

DEPS_CODE='
// Plugin dependency management (stub - no dependencies)
SpindexPluginDependency* spindex_plugin_get_dependencies(int* count) {
    if (count) *count = 0;
    return NULL;  // This plugin has no dependencies
}

void spindex_plugin_free_dependencies_list(SpindexPluginDependency* dependencies, int count) {
    // Nothing to free - we returned NULL
    (void)dependencies;
    (void)count;
}
'

for dir in "${PLUGIN_DIRS[@]}"; do
    if [ -d "$dir" ]; then
        for file in "$dir"/*.c; do
            if [ -f "$file" ] && ! grep -q "spindex_plugin_get_dependencies" "$file"; then
                echo "$DEPS_CODE" >> "$file"
                echo "✓ Added to $(basename $file)"
            fi
        done
    fi
done

echo ""
echo "✓ All plugins updated!"
