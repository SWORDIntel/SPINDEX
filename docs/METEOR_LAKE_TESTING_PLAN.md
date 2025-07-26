# Meteor Lake Hardware Testing Plan

## 1. Overview

This document outlines the test plan for verifying the support for Intel Meteor Lake hardware in the SPINDEX project. The primary focus of this testing is to ensure that the OpenVINO classifier can take advantage of the Neural Processing Unit (NPU) present in Meteor Lake processors.

## 2. Hardware Detection

### 2.1. Meteor Lake Processor Identification

**Objective**: Verify that the hardware detection module can correctly identify Meteor Lake processors.

**Procedure**:
1.  Modify the `core/hardware_detection.c` file to parse the CPU model name from `/proc/cpuinfo`.
2.  Add a new field `is_meteor_lake` to the `CpuFeatures` struct.
3.  In the `detect_cpu_features` function, check if the CPU model name contains "Meteor Lake".
4.  Write a new test case in `core/test_hardware_detection.c` to verify that the `is_meteor_lake` field is set correctly when running on a Meteor Lake processor.

### 2.2. NPU Identification

**Objective**: Verify that the hardware detection module can correctly identify the presence of an NPU.

**Procedure**:
1.  Research the appropriate method for detecting the NPU on Linux. This may involve checking for a specific device file (e.g., `/dev/ion`) or a specific entry in `/sys/class`.
2.  Add a new field `has_npu` to the `CpuFeatures` struct.
3.  In the `detect_cpu_features` function, implement the logic to detect the NPU.
4.  Write a new test case in `core/test_hardware_detection.c` to verify that the `has_npu` field is set correctly when running on a Meteor Lake processor with an NPU.

## 3. OpenVINO Classifier

### 3.1. NPU Configuration

**Objective**: Verify that the OpenVINO classifier can be configured to run on the NPU.

**Procedure**:
1.  Modify the `plugins/classifiers/openvino_classifier.c` file to allow the user to specify the device to run on. This will be done by adding a new configuration option `device` to the `plugins.cfg` file.
2.  In the `openvino_load_model` function, use the `device` configuration option to specify the device to run on when calling `ov_core_compile_model`.
3.  Write a new test case in `plugins/classifiers/test_openvino_classifier.c` to verify that the classifier can be successfully loaded with the `device` option set to "NPU".

### 3.2. NPU Execution Verification

**Objective**: Verify that the OpenVINO classifier is actually running on the NPU.

**Procedure**:
1.  Enable verbose logging in the OpenVINO runtime.
2.  Run the OpenVINO classifier with the `device` option set to "NPU".
3.  Inspect the logs for messages indicating that the inference is being performed on the NPU.

### 3.3. Performance Measurement

**Objective**: Measure the performance of the OpenVINO classifier on the NPU.

**Procedure**:
1.  Create a benchmark test that runs a large number of inferences with the OpenVINO classifier.
2.  Run the benchmark test with the `device` option set to "NPU", "CPU", and "GPU".
3.  Measure the average inference time for each device.
4.  Compare the results and document the performance improvement of using the NPU.
