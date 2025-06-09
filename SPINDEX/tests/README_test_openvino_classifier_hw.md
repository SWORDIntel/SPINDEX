# README: OpenVINO Classifier Hardware Test Harness (`test_openvino_classifier_hw`)

## 1. Overview

This document provides instructions for compiling and running the `test_openvino_classifier_hw` test harness. This harness is designed to help execute hardware-specific testing and validation for the OpenVINO-based classifier plugin (`openvino_classifier.so`) on target hardware, specifically systems like Intel Meteor Lake with NPUs, as outlined in the test plan `ML_OV_005_TESTING_PLAN.md`.

The test harness allows for loading a specified OpenVINO model, running inference on provided preprocessed data, targeting specific compute devices (AUTO, CPU, GPU, NPU), and measuring basic performance.

**Important:** This test harness requires manual execution on a system with the appropriate hardware (e.g., Intel Meteor Lake 165h) and a correctly configured software environment (drivers, Intel OpenVINO Toolkit).

## 2. Prerequisites

*   **Hardware:** A system equipped with an Intel® Core™ Ultra processor with an integrated NPU (e.g., Intel Core Ultra 7 165h or similar Meteor Lake SKU) is required for NPU testing. CPU and iGPU testing can be done on compatible Intel hardware.
*   **Operating System:** Linux (e.g., Ubuntu 22.04+) with a kernel supporting the target hardware components.
*   **Intel OpenVINO Toolkit:** The toolkit must be installed, and its `setupvars.sh` script should be sourced in the shell session where you compile and run the test harness. This ensures all necessary environment variables, libraries, and paths are correctly set.
    ```bash
    source /path/to/openvino/setupvars.sh
    # (Adjust path to your OpenVINO installation)
    ```
*   **Drivers:** Up-to-date drivers for Intel NPU, Intel integrated graphics, and CPU.
*   **SPINDEX Project:** A local clone of the SPINDEX repository.
*   **ML Model:** An FP16-quantized text classification model in OpenVINO Intermediate Representation (IR) format (`.xml` and `.bin` files).
*   **Preprocessed Test Data:** Input data that has been preprocessed into the exact numerical tensor format expected by your ML model. The test harness expects this as a flat binary file.
    *   Sample placeholder data format is described in `SPINDEX/test_data/openvino_sample_input/README.md`. You can use these or (preferably) your own model-specific data.

## 3. Compiling the Test Harness

Navigate to the root directory of the SPINDEX project.
Ensure the OpenVINO environment is set up (source `setupvars.sh`).
Run the following make command:

```bash
make build/test_openvino_classifier_hw
```

This command will compile the test harness executable and place it at `build/test_openvino_classifier_hw`. It will also compile the `openvino_classifier.so` plugin if it hasn't been built already, as the harness loads it dynamically.

## 4. Running the Test Harness

Execute the compiled test harness from the SPINDEX root directory with the following command-line arguments:

```bash
./build/test_openvino_classifier_hw <model_xml_path> <input_tensor_data_path> <device_name>
```

**Arguments:**

*   `<model_xml_path>`: **Required.** Full or relative path to the OpenVINO model's `.xml` file. The corresponding `.bin` file must be in the same directory.
*   `<input_tensor_data_path>`: **Required.** Full or relative path to a binary file containing the preprocessed input tensor data.
*   `<device_name>`: **Required.** The target device for OpenVINO inference. Common values:
    *   `AUTO`: Lets OpenVINO automatically select the best device. Recommended for initial NPU testing on Meteor Lake.
    *   `CPU`: Targets the CPU.
    *   `GPU`: Targets the integrated Intel GPU.
    *   `NPU`: Explicitly targets the Neural Processing Unit.

**Example:**

```bash
# Ensure OpenVINO vars are sourced first!
source /opt/intel/openvino_2023/setupvars.sh

./build/test_openvino_classifier_hw \
    ./models/my_distilbert_fp16.xml \
    ./test_data/openvino_sample_input/sample1_int64_seq128.bin \
    AUTO
```

## 5. Interpreting Output

The test harness will print information to `stdout` and `stderr`:

*   **Setup Information:** Paths to plugin, model, data, and target device.
*   **Plugin Loading:** Success or failure messages for loading `openvino_classifier.so` and its functions.
*   **Device Setting:** Confirmation of the device target being set.
*   **Model Loading:** Success or failure for loading the specified model. It will indicate the device on which the model was compiled (which should match the target device or "AUTO"'s choice).
*   **Data Loading:** Confirmation of input tensor data loading.
*   **Inference Loop (10 iterations by default):**
    *   For the first iteration, it will print the output tensor size and the first few bytes of the output tensor in hexadecimal format. This helps verify that inference produced data.
*   **Performance Summary:**
    *   Total iterations performed.
    *   Total time taken for all inference iterations.
    *   Average inference latency (in milliseconds per inference).
    *   Throughput (inferences per second).
*   **Cleanup:** Messages related to unloading the model and closing the plugin.
*   **Exit Code:** The program will exit with `0` if the basic execution path completes, or `1` if an error occurs during setup or execution.

**Error Messages:** Any errors encountered during plugin loading, model operations, or inference will be printed to `stderr`. Pay close attention to these messages for troubleshooting. OpenVINO itself may also print diagnostic messages.

## 6. Testing Different Scenarios

To perform the tests outlined in `ML_OV_005_TESTING_PLAN.md`, run the harness multiple times, varying the `<device_name>` argument (AUTO, CPU, GPU, NPU) and potentially the `<model_xml_path>` or `<input_tensor_data_path>` if you have different models or datasets to test.

*   **NPU Utilization:** When testing with "NPU" or "AUTO" on Meteor Lake, use system monitoring tools (`intel_gpu_top` for GPU, specific NPU monitoring tools if provided by Intel, or observe OpenVINO verbose logs if enabled) to attempt to confirm NPU engagement. The test harness itself does not directly measure NPU-specific metrics beyond overall inference time.

This test harness provides a command-line tool to facilitate the execution of most tests described in `ML_OV_005_TESTING_PLAN.md`.
