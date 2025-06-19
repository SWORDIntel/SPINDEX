#!/bin/bash
set -e # Exit on error

echo "SPINDEX OpenVINO Hardware Test Runner"
echo "======================================"
echo "IMPORTANT: This script assumes:"
echo "1. You are on a Linux system with compatible Intel hardware (CPU/GPU/NPU)."
echo "2. You have installed the Intel OpenVINO Toolkit."
echo "3. You have the necessary drivers for your hardware."
echo "4. You have an FP16 OpenVINO model (.xml & .bin) and preprocessed input tensor data (.bin)."
echo "--------------------------------------"

# --- User Configuration ---
OPENVINO_SETUP_VARS_PATH="/opt/intel/openvino_2023/setupvars.sh" # ADJUST THIS PATH
MODEL_XML_PATH="models/your_model_fp16.xml"                     # ADJUST THIS PATH
INPUT_TENSOR_PATH="test_data/your_input_tensor.bin"           # ADJUST THIS PATH
TARGET_DEVICE="AUTO" # Options: AUTO, CPU, GPU, NPU

# --- Script Body ---
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" &> /dev/null && pwd)"
PROJECT_ROOT="$SCRIPT_DIR" # Assumes script is in SPINDEX root

echo "Project Root: $PROJECT_ROOT"
echo "OpenVINO Setup Script: $OPENVINO_SETUP_VARS_PATH"
echo "Model XML: $MODEL_XML_PATH"
echo "Input Tensor: $INPUT_TENSOR_PATH"
echo "Target Device: $TARGET_DEVICE"
echo "--------------------------------------"

if [ ! -f "$OPENVINO_SETUP_VARS_PATH" ]; then
    echo "ERROR: OpenVINO setupvars.sh not found at $OPENVINO_SETUP_VARS_PATH"
    echo "Please adjust the OPENVINO_SETUP_VARS_PATH variable in this script."
    exit 1
fi
if [ ! -f "$MODEL_XML_PATH" ]; then
    echo "ERROR: Model XML file not found at $MODEL_XML_PATH"
    echo "Please adjust the MODEL_XML_PATH variable and ensure model files are present."
    exit 1
fi
if [ ! -f "$INPUT_TENSOR_PATH" ]; then
    echo "ERROR: Input tensor data not found at $INPUT_TENSOR_PATH"
    echo "Please adjust the INPUT_TENSOR_PATH variable and ensure data is present."
    exit 1
fi

echo "Sourcing OpenVINO environment..."
source "$OPENVINO_SETUP_VARS_PATH"

echo "Building SPINDEX project with CMake..."
cd "$PROJECT_ROOT"
mkdir -p build
cd build
cmake ..
cmake --build . -j$(nproc || echo 2) # Use nproc if available, otherwise default to 2 cores
cd "$PROJECT_ROOT" # Return to project root

TEST_EXECUTABLE="./build/bin/test_openvino_classifier_hw"

if [ ! -f "$TEST_EXECUTABLE" ]; then
    echo "ERROR: Test executable not found at $TEST_EXECUTABLE after build."
    exit 1
fi
if [ ! -f "./build/lib/openvino_classifier.so" ]; then # Check for plugin too
    echo "ERROR: OpenVINO plugin not found at ./build/lib/openvino_classifier.so after build."
    exit 1
fi


echo "Running OpenVINO Hardware Test Harness..."
echo "Command: $TEST_EXECUTABLE $MODEL_XML_PATH $INPUT_TENSOR_PATH $TARGET_DEVICE"
"$TEST_EXECUTABLE" "$MODEL_XML_PATH" "$INPUT_TENSOR_PATH" "$TARGET_DEVICE"

echo "--------------------------------------"
echo "Test run finished."
echo "Please check the output above for results and potential errors."
