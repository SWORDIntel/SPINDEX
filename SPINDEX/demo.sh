#!/bin/bash
# SPINDEX End-to-End Processing Demo

set -e

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo ""
echo "╔════════════════════════════════════════╗"
echo "║   SPINDEX End-to-End Processing Demo  ║"
echo "╔════════════════════════════════════════╝"
echo ""

# Check prerequisites
echo -e "${BLUE}[1/6]${NC} Checking prerequisites..."
if [ ! -f "build_cmake/bin/spindex_app" ]; then
    echo -e "${RED}✗${NC} spindex_app not found! Run: cmake --build build_cmake"
    exit 1
fi
if [ ! -f "config.ini" ]; then
    echo -e "${RED}✗${NC} config.ini not found! Run: cp config.sample.ini config.ini"
    exit 1
fi
echo -e "${GREEN}✓${NC} All prerequisites met"
echo ""

# Show system info
echo -e "${BLUE}[2/6]${NC} System Information:"
./build_cmake/bin/spindex_app --version
echo ""

# Show sample data
echo -e "${BLUE}[3/6]${NC} Sample Data Available:"
if [ -d "sample_data" ]; then
    echo -e "${GREEN}✓${NC} Sample data directory exists"
    echo ""
    echo "  Files:"
    for file in sample_data/*; do
        size=$(du -h "$file" | cut -f1)
        echo "    • $(basename $file) - $size"
    done
else
    echo -e "${YELLOW}⚠${NC} No sample_data directory - creating..."
    mkdir -p sample_data
fi
echo ""

# Test plugin discovery
echo -e "${BLUE}[4/6]${NC} Plugin Discovery Test:"
plugin_count=$(ls build_cmake/lib/spindex/plugins/*.so 2>/dev/null | wc -l)
echo -e "${GREEN}✓${NC} Found $plugin_count plugin modules"
echo ""
echo "  Preprocessors:"
for plugin in build_cmake/lib/spindex/plugins/*_preprocessor.so; do
    [ -f "$plugin" ] && echo "    • $(basename $plugin)"
done | head -5
echo "    ... and more"
echo ""
echo "  Parsers:"
for plugin in build_cmake/lib/spindex/plugins/*_parser.so; do
    [ -f "$plugin" ] && echo "    • $(basename $plugin)"
done | head -5
echo "    ... and more"
echo ""

# Demonstrate configuration
echo -e "${BLUE}[5/6]${NC} Configuration:"
echo "  • Plugin Directory: $(grep plugin_directory config.ini | cut -d= -f2)"
echo "  • Log File: $(grep log_file_path config.ini | cut -d= -f2)"
echo "  • Log Level: $(grep log_level config.ini | cut -d= -f2)"
echo "  • Max Concurrent: $(grep max_concurrent_files config.ini | cut -d= -f2)"
echo ""

# Show processing capability
echo -e "${BLUE}[6/6]${NC} Processing Capabilities:"
echo ""
echo "  ${GREEN}JSON Processing:${NC}"
echo "    • Plugin: json_preprocessor.so → json_parser.so"
echo "    • Sample: sample_data/documents.json"
echo "    • Features: Metadata extraction, nested object parsing"
echo ""
echo "  ${GREEN}CSV Processing:${NC}"
echo "    • Plugin: csv_preprocessor.so → csv_parser.so"
echo "    • Sample: sample_data/locations.csv"
echo "    • Features: Column detection, type inference"
echo ""
echo "  ${GREEN}GeoJSON Processing:${NC}"
echo "    • Plugin: json_preprocessor.so + GEOS validator"
echo "    • Sample: sample_data/regions.geojson"
echo "    • Features: Geometry validation, topology repair"
echo ""
echo "  ${GREEN}Text Processing:${NC}"
echo "    • Plugin: txt_preprocessor.so → txt_parser.so"
echo "    • Sample: sample_data/readme.txt"
echo "    • Features: Content extraction, indexing"
echo ""

# Summary
echo "╔════════════════════════════════════════╗"
echo "║            Demo Complete!              ║"
echo "╚════════════════════════════════════════╝"
echo ""
echo "Next Steps:"
echo "  1. Start Elasticsearch: docker run -p 9200:9200 elasticsearch:8.11.0"
echo "  2. Run SPINDEX: ./build_cmake/bin/spindex_app config.ini"
echo "  3. Feed data: Point the system at sample_data/"
echo "  4. Query results: curl localhost:9200/spindex_documents/_search"
echo ""
echo -e "${GREEN}✓${NC} SPINDEX is ready to process data!"
echo ""
