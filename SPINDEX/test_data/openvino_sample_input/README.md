# Sample Preprocessed Input Data for OpenVINO Test Harness

This directory contains placeholder binary files intended for use with `SPINDEX/tests/test_openvino_classifier_hw.c`. These files simulate preprocessed input tensors for a hypothetical text classification model (e.g., a DistilBERT-like model).

## File Format

*   **`sample*_int64_seq128.bin`**:
    *   Each file represents a single input tensor for one inference call.
    *   The data consists of 128 consecutive `int64_t` (64-bit integer) values.
    *   The total size of each file is `128 * sizeof(int64_t) = 128 * 8 = 1024` bytes.
    *   The `int64_t` values are stored in **little-endian** byte order.
    *   These values would typically correspond to token IDs after text preprocessing (tokenization, padding/truncation to a fixed sequence length of 128).

## Content Details

*   **`sample1_int64_seq128.bin`**: Contains the sequence of `int64_t` values: `0, 1, 2, ..., 127`.
*   **`sample2_int64_seq128.bin`**: Contains the sequence of `int64_t` values: `127, 126, 125, ..., 0`.

## Usage

The `test_openvino_classifier_hw` harness can be pointed to one of these files using its `<input_tensor_data_path>` command-line argument. For example:

```bash
# Assuming test harness is run from SPINDEX root
./build/test_openvino_classifier_hw path/to/model.xml ./test_data/openvino_sample_input/sample1_int64_seq128.bin AUTO
```

**Note:** This data is purely for placeholder/testing purposes to allow the test harness to function. Actual models will require specific preprocessing tailored to their architecture and training data. The `openvino_classifier.c` plugin (as of its current version) expects this preprocessed tensor data directly.
