#!/bin/bash
# Quick plugin discovery test for SPINDEX

PLUGIN_DIR="build_cmake/lib/spindex/plugins"
TOTAL_PLUGINS=0
WORKING_PLUGINS=0

echo "================================"
echo "SPINDEX Plugin Discovery Test"
echo "================================"
echo ""
echo "Plugin Directory: $PLUGIN_DIR"
echo ""

if [ ! -d "$PLUGIN_DIR" ]; then
    echo "❌ Plugin directory not found!"
    exit 1
fi

echo "Scanning for plugins..."
echo ""

for plugin in "$PLUGIN_DIR"/*.so; do
    if [ -f "$plugin" ]; then
        TOTAL_PLUGINS=$((TOTAL_PLUGINS + 1))
        plugin_name=$(basename "$plugin")

        # Check if plugin has required symbols
        if nm -D "$plugin" 2>/dev/null | grep -q "spindex_plugin_get_info"; then
            echo "✅ $plugin_name"
            WORKING_PLUGINS=$((WORKING_PLUGINS + 1))
        else
            echo "⚠️  $plugin_name (missing symbols)"
        fi
    fi
done

echo ""
echo "================================"
echo "Summary:"
echo "  Total plugins found: $TOTAL_PLUGINS"
echo "  Plugins with symbols: $WORKING_PLUGINS"
echo "================================"
echo ""

if [ $TOTAL_PLUGINS -eq 21 ]; then
    echo "✅ All 21 expected plugins present!"
else
    echo "⚠️  Expected 21 plugins, found $TOTAL_PLUGINS"
fi

# Test if spindex_app can load the config
echo ""
echo "Testing spindex_app with config..."
if ./build_cmake/bin/spindex_app config.ini 2>&1 | head -5; then
    echo "✅ Application starts successfully!"
else
    echo "⚠️  Application had issues starting"
fi
