# Test Plan: [ML-OV-005] OpenVINO FP16 Inference on Meteor Lake 165h

## 1. Overview

This document outlines the testing and validation plan for task `[ML-OV-005]`: Execute hardware-specific testing and validation for OpenVINO FP16 Inference on Intel Meteor Lake 165h processors. This plan focuses on verifying the `openvino_classifier.so` plugin's functionality, performance, and stability, particularly its ability to leverage FP16 precision models and potentially the NPU on the target hardware.

This plan assumes the `openvino_classifier.c` plugin is implemented as per its header and source files, and is intended to be used within the SPINDEX application for tasks like text classification.

## 2. Testing Objectives and Scope

*   **Functional Correctness:**
    *   Verify that the `openvino_classifier.so` plugin correctly loads pre-trained FP16 models in OpenVINO Intermediate Representation (IR) format (`.xml` and `.bin`).
    *   Confirm accurate inference results when processing sample text data using a suitable classification model.
    *   Ensure the plugin handles different device targets (AUTO, CPU, GPU, NPU if selectable) correctly.
*   **Performance:**
    *   Measure inference latency and throughput for FP16 models on the Meteor Lake 165h platform.
    *   If possible, compare performance against CPU-only execution or FP32 model inference (requires baseline data).
    *   Assess performance when targeting different devices (CPU, iGPU, NPU).
*   **NPU Offloading Verification (Best Effort):**
    *   Attempt to confirm if OpenVINO successfully utilizes the NPU on the Meteor Lake 165h when configured to "AUTO" or "NPU". This may involve using OpenVINO verbose logs, performance monitoring tools (e.g., `ov::benchmark_app`, system monitoring tools), or specific logging implemented within the plugin if available.
*   **Stability:**
    *   Ensure the plugin operates without crashes, memory leaks, or other issues during sustained operation and when processing diverse inputs.

**Out of Scope for this Test Plan (but relevant for overall plugin quality):**
*   Detailed accuracy testing of the ML model itself (model accuracy is assumed to be pre-validated).
*   Testing on hardware other than Intel Meteor Lake 165h.
*   Creation or fine-tuning of ML models (test plan assumes suitable FP16 IR models are provided).

## 3. Target Test Environment Setup

*   **Hardware:**
    *   System equipped with an Intel® Core™ Ultra processor with integrated NPU (e.g., Intel Core Ultra 7 165H or similar Meteor Lake SKU).
*   **Software:**
    *   **Operating System:** Linux (e.g., Ubuntu 22.04+) with a kernel version that includes stable drivers for the Intel NPU, iGPU, and CPU.
    *   **Intel OpenVINO Toolkit:** Latest stable version compatible with Meteor Lake NPUs and iGPUs, including all necessary runtime libraries and development tools.
    *   **Drivers:** Up-to-date drivers for Intel NPU, Intel integrated graphics (e.g., Xe Graphics), and CPU.
    *   **SPINDEX Application:** Compiled SPINDEX application, including the `openvino_classifier.so` plugin, linked against the installed OpenVINO libraries.
    *   **Build Tools:** GCC, Make, CMake as required for SPINDEX.
*   **ML Models:**
    *   **Primary Test Model:** An FP16-quantized text classification model in OpenVINO IR format (`.xml` and `.bin`). A DistilBERT-based model fine-tuned for a relevant task (e.g., document type classification, sentiment analysis) is recommended, as per `FP16.MD` (assumed content).
        *   The model's input and output tensor specifications (name, shape, data type, layout) must be known for preparing input data and interpreting results.
    *   **(Optional) Secondary Model:** A different FP16 model or an FP32 version of the primary model for comparison.
*   **Test Data:**
    *   A dataset of text snippets or small text files (e.g., 100-500 samples for functional tests, 1000+ for performance/stability).
    *   Data should be representative of the input the classification model expects.
    *   Include samples that cover different expected classes.
    *   Include some edge cases if applicable (e.g., very short text, text with unusual characters – though this tests model robustness more than plugin functionality).

## 4. Test Cases and Procedures

### Test Case 1: Basic Model Loading and Inference (Functional Correctness)

*   **Objective:** Verify the plugin can load an FP16 IR model and perform a basic inference.
*   **Procedure:**
    1.  Configure SPINDEX or a test harness to use `openvino_classifier.so`.
    2.  Provide the path to the FP16 test model's `.xml` file to the plugin (e.g., via SPINDEX configuration or test harness parameter).
    3.  Prepare a small set (5-10 samples) of input text data. This data needs to be preprocessed into the raw numerical format expected by the model's input tensor (e.g., token IDs, attention masks).
    4.  For each input sample:
        a.  Create a `ProcessedDataChunk` where `data` points to the preprocessed input tensor data, and `size` is the byte size of this tensor.
        b.  Call the plugin's `predict` function (`openvino_predict`).
        c.  Examine the returned output `ProcessedDataChunk`. The raw output tensor data will need to be interpreted based on the model's output specification (e.g., softmax logits).
    5.  **Verification:**
        *   Confirm that the plugin loads the model without errors (check `stderr` and application logs).
        *   Confirm that `predict` returns a non-null output chunk.
        *   Manually verify that the classification output (after any necessary post-processing) is reasonable for the given input samples and model. (Exact class matching is model testing, here we check for plausible output format/values).
        *   Check for any errors logged by OpenVINO or the plugin.

### Test Case 2: Device Targeting and NPU Utilization (Functional & Performance)

*   **Objective:** Verify inference on different devices (CPU, iGPU, NPU) and attempt to confirm NPU usage.
*   **Assumptions:** The `openvino_classifier.c` plugin allows device selection for compilation (e.g., by modifying the `device_name` variable in `openvino_load_model` or through a configuration parameter if implemented).
*   **Procedure:**
    1.  **Test with "AUTO" device selection:**
        a.  Ensure `device_name` in `openvino_load_model` is set to "AUTO".
        b.  Execute Test Case 1 procedures.
        c.  **Verification:** Observe OpenVINO logs (if verbosity can be increased) or use system monitoring tools (e.g., `intel_gpu_top` for iGPU, specific NPU monitoring tools if available from Intel) during inference to infer which device "AUTO" mode selected. Note performance.
    2.  **Test with "NPU" explicit device selection:**
        a.  Modify/configure the plugin to explicitly use "NPU" as the `device_name`. Recompile SPINDEX if code change was needed.
        b.  Execute Test Case 1 procedures.
        c.  **Verification:** Attempt to confirm NPU engagement using available monitoring tools. Compare performance (latency) against "AUTO" and "CPU".
    3.  **Test with "GPU" explicit device selection (for iGPU):**
        a.  Modify/configure for "GPU". Recompile if needed.
        b.  Execute Test Case 1 procedures.
        c.  **Verification:** Confirm iGPU usage (e.g., `intel_gpu_top`). Note performance.
    4.  **Test with "CPU" explicit device selection:**
        a.  Modify/configure for "CPU". Recompile if needed.
        b.  Execute Test Case 1 procedures.
        c.  **Verification:** Note performance as a baseline.
*   **Note:** Verifying NPU utilization directly can be challenging without specific vendor tools or detailed OpenVINO telemetry. Focus on whether the selection runs successfully and gather performance metrics.

### Test Case 3: Performance Benchmark (FP16)

*   **Objective:** Measure inference performance with the FP16 model.
*   **Procedure:**
    1.  Use the configuration deemed most optimal from Test Case 2 (e.g., "AUTO" or explicit "NPU" if it showed benefits and stability).
    2.  Prepare a larger dataset (e.g., >1000 samples) of preprocessed input tensors.
    3.  Iterate through the dataset, calling the `predict` function for each sample.
    4.  For each inference call, measure the time taken by the `predict` function.
    5.  Calculate:
        *   Average inference latency (ms per sample).
        *   Total throughput (samples per second).
    6.  **Verification:**
        *   Performance metrics are recorded.
        *   (Optional) Compare against any available FP32 or non-OpenVINO CPU baseline for the same model and task.

### Test Case 4: Stability and Error Handling Test

*   **Objective:** Ensure the plugin is stable under sustained load and handles potential errors gracefully.
*   **Procedure:**
    1.  **Sustained Load:** Run the inference process (Test Case 1 or 3 procedure) repeatedly for an extended duration (e.g., 30-60 minutes, or a very large number of files).
    2.  **Invalid Inputs (Conceptual - may require plugin modification to pass such data):**
        *   If possible, test with an incorrectly sized input `ProcessedDataChunk` (plugin should reject it as per current code).
        *   Test with a null `input_data->data`.
        *   Test by providing an invalid model path to `openvino_load_model`.
    3.  **Verification:**
        *   Monitor SPINDEX application and system for crashes, hangs, or excessive memory consumption.
        *   Check logs for any new errors or warnings.
        *   Ensure the plugin correctly reports errors for invalid inputs or model loading failures (e.g., returns non-zero, sets output chunk to NULL).

## 5. Test Harness/Scripts (Conceptual)

*   **Option A: SPINDEX Application (End-to-End Style)**
    *   Configure the SPINDEX ingestion pipeline to use the `openvino_classifier.so` plugin for a specific task.
    *   Prepare a directory of input files. The SPINDEX pipeline would need a pre-processing step before the OpenVINO plugin to convert raw text files into the numerical tensor format expected by the ML model. This pre-processing step is outside the scope of the `openvino_classifier.so` plugin itself but is crucial for end-to-end testing.
    *   Run SPINDEX ingestion on the directory.
    *   Pros: Tests integration with the larger system.
    *   Cons: More complex to set up; harder to isolate plugin-specific performance; pre-processing dependency.
*   **Option B: Dedicated C Test Harness (Unit/Component Style)**
    *   A C program that:
        1.  Dynamically loads `openvino_classifier.so` (or links against a static version if built that way for testing).
        2.  Uses the `SpindexMLModelInterface` functions:
            *   `create_openvino_model_interface(...)`
            *   `openvino_load_model(...)` with the test FP16 model path.
        3.  Reads pre-processed test data (numerical tensors) from files or generates them.
        4.  Populates `ProcessedDataChunk` structures for input.
        5.  Calls `openvino_predict(...)`.
        6.  Times the `predict` calls.
        7.  Retrieves output from the output `ProcessedDataChunk` and prints/validates it.
        8.  Calls `openvino_unload_model(...)`.
    *   Pros: Allows focused testing of the plugin; easier to control inputs and measure performance accurately.
    *   Cons: Requires writing a separate test program; doesn't test integration with SPINDEX core as deeply.

**Recommendation:** Start with a **Dedicated C Test Harness (Option B)** for focused functional and performance testing of the plugin. Then, proceed to **SPINDEX Application testing (Option A)** for integration and end-to-end validation.

## 6. Expected Results and Success Criteria

*   **Functional:** All test cases involving model loading and inference with valid inputs should complete without crashes or errors reported by the plugin or OpenVINO. Output data should be in the expected format.
*   **Performance:** Measurable performance metrics (latency, throughput) should be obtainable. If NPU or GPU is targeted, performance should ideally be better than CPU-only (though this depends heavily on the model, batch size, and specific hardware capabilities).
*   **NPU Utilization:** Logs or tools should ideally indicate NPU engagement when "NPU" or "AUTO" (and NPU is chosen by AUTO) is selected. If direct confirmation is hard, successful runs on "NPU" target with expected performance characteristics would be a partial validation.
*   **Stability:** No crashes or memory leaks during extended runs. Graceful error handling for invalid inputs or setup issues.

## 7. Reporting and Deliverables

*   This `ML_OV_005_TESTING_PLAN.md` document is the primary deliverable for the planning phase of `[ML-OV-005]`.
*   Upon actual execution (on target hardware):
    *   A test report summarizing the execution of these test cases.
    *   Recorded performance metrics (latency, throughput for different devices).
    *   Any logs or error messages encountered.
    *   Observations on NPU utilization.
    *   Any identified bugs, issues, or areas for improvement in the plugin or its documentation.

This plan provides a structured approach to validating the `openvino_classifier.so` plugin on Intel Meteor Lake 165h hardware.
