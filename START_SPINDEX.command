#!/bin/bash
# SPINDEX One-Click Launcher for macOS
# Double-click this file to start SPINDEX

# Get the directory where this script is located
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
cd "$DIR"

# Colors for pretty output
RED='\033[0;31m'
GREEN='\033[0;32m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo ""
echo -e "${CYAN}╔══════════════════════════════════════════════════════════════╗${NC}"
echo -e "${CYAN}║                                                              ║${NC}"
echo -e "${CYAN}║                 🚀 SPINDEX LAUNCHER 🚀                       ║${NC}"
echo -e "${CYAN}║                                                              ║${NC}"
echo -e "${CYAN}║         One-Click Startup for Full-Stack System             ║${NC}"
echo -e "${CYAN}║                                                              ║${NC}"
echo -e "${CYAN}╚══════════════════════════════════════════════════════════════╝${NC}"
echo ""

# Check if Python 3 is installed
if command -v python3 &> /dev/null; then
    echo -e "${GREEN}✅ Python 3 found${NC}"
else
    echo -e "${RED}❌ Python 3 is not installed${NC}"
    echo ""
    echo "Please install Python 3.8 or higher from:"
    echo "https://www.python.org/downloads/"
    echo ""
    echo "Or install via Homebrew:"
    echo "  brew install python@3"
    echo ""
    read -p "Press Enter to exit..."
    exit 1
fi

# Check if Node.js is installed
if command -v node &> /dev/null; then
    echo -e "${GREEN}✅ Node.js found${NC}"
else
    echo -e "${RED}❌ Node.js is not installed${NC}"
    echo ""
    echo "Please install Node.js 16 or higher from:"
    echo "https://nodejs.org/"
    echo ""
    echo "Or install via Homebrew:"
    echo "  brew install node"
    echo ""
    read -p "Press Enter to exit..."
    exit 1
fi

echo ""
echo -e "${BLUE}Starting SPINDEX...${NC}"
echo ""

# Run the Python launcher
python3 start_spindex.py

if [ $? -ne 0 ]; then
    echo ""
    echo -e "${RED}❌ SPINDEX failed to start${NC}"
    echo ""
    read -p "Press Enter to exit..."
    exit 1
fi

echo ""
read -p "Press Enter to close this window..."
