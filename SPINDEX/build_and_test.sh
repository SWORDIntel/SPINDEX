#!/bin/bash

# Exit immediately if a command exits with a non-zero status.
set -e

# --- Configuration Variables ---
VENV_DIR="keyplug_venv"
PYTHON_EXEC="python3"
PIP_EXEC="pip3"
REQUIREMENTS_FILE="requirements.txt"
TEST_SCRIPT="test_jsteg.py"
TEST_IMAGE_DIR="test_images_jpeg"

# --- Helper Functions ---
print_header() {
    echo ""
    echo "======================================================================"
    echo "  $1"
    echo "======================================================================"
}

# --- 1. Initial Diagnostic Checks ---
# Why it helps: Ensures the environment is in a known state before starting.
print_header "Running Initial Diagnostic Checks"

echo "-> System Information:"
uname -a

echo "-> Git Repository Status:"
if [ -d ".git" ]; then
    git status -uall
    git remote -v
else
    echo "Not a git repository. Skipping git checks."
fi

echo "-> Checking for required tools (python3, pip3, gcc)..."
command -v $PYTHON_EXEC >/dev/null 2>&1 || { echo >&2 "Error: python3 is not installed or not in PATH. Aborting."; exit 1; }
command -v $PIP_EXEC >/dev/null 2>&1 || { echo >&2 "Error: pip3 is not installed or not in PATH. Aborting."; exit 1; }
command -v gcc >/dev/null 2>&1 || { echo >&2 "Warning: gcc (C compiler) not found. C compilation steps will be skipped."; }

echo "-> Tool Versions:"
$PYTHON_EXEC --version
$PIP_EXEC --version
gcc --version || echo "gcc not found."

echo "-> Checking for required files and directories..."
[ -f "$TEST_SCRIPT" ] || { echo "Error: Test script '$TEST_SCRIPT' not found."; exit 1; }
[ -f "$REQUIREMENTS_FILE" ] || { echo "Error: Requirements file '$REQUIREMENTS_FILE' not found."; exit 1; }
[ -d "$TEST_IMAGE_DIR" ] || { echo "Error: Test image directory '$TEST_IMAGE_DIR' not found."; exit 1; }
[ -f "$TEST_IMAGE_DIR/jpeg_grayscale.jpg" ] || { echo "Error: Test image 'jpeg_grayscale.jpg' not found."; exit 1; }
[ -f "$TEST_IMAGE_DIR/jpeg_low_quality_small.jpg" ] || { echo "Error: Test image 'jpeg_low_quality_small.jpg' not found."; exit 1; }

echo "-> Diagnostics Complete. Environment looks sane."

# --- 2. C Component Build ---
# Why it helps: Reduces ambiguity about how C executables are built.
print_header "Building C Components (if any)"
if [ -f "Makefile" ]; then
    echo "-> Found Makefile. Running 'make'..."
    make
elif [ -f "makefile" ]; then
    echo "-> Found makefile. Running 'make'..."
    make
else
    echo "-> No Makefile found. Skipping C compilation."
fi

# --- 3. Python Environment Setup ---
# Why it helps: Manages dependencies and ensures a consistent Python environment.
print_header "Setting up Python Virtual Environment"

if [ ! -d "$VENV_DIR" ]; then
    echo "-> Creating Python virtual environment in '$VENV_DIR'..."
    $PYTHON_EXEC -m venv $VENV_DIR
else
    echo "-> Virtual environment '$VENV_DIR' already exists."
fi

echo "-> Activating virtual environment..."
source "$VENV_DIR/bin/activate"

echo "-> Installing/Updating Python dependencies from $REQUIREMENTS_FILE..."
$PIP_EXEC install -r $REQUIREMENTS_FILE

echo "-> Verifying Python dependencies..."
pip check

# --- 4. Running Tests ---
# Why it helps: Automates the validation of the project's functionality.
print_header "Running Python Unit Tests"

# Set PYTHONPATH to include the current directory to help with module resolution.
export PYTHONPATH=.
echo "-> PYTHONPATH set to: $PYTHONPATH"
echo "-> Current working directory: $(pwd)"
echo "-> Files in CWD: $(ls -F)"

echo "-> Executing $TEST_SCRIPT..."
$PYTHON_EXEC $TEST_SCRIPT

# --- Completion ---
print_header "Build and Test Script Finished Successfully"
deactivate
echo "-> Virtual environment deactivated."
