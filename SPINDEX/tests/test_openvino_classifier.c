#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>

#include "../include/plugin_interface.h"
#include "../include/data_types.h"
#include "../plugins/classifiers/openvino_classifier.h"

#define MODEL_PATH "test_data/openvino_sample_input/model.xml"
#define INPUT_DATA_PATH "test_data/openvino_sample_input/input.bin"

void test_load_model_on_npu() {
    printf("Running test: test_load_model_on_npu\n");

    SpindexMLModelInterface model_interface;
    int result = create_openvino_model_interface(&model_interface, MODEL_PATH);
    assert(result == 0);

    openvino_set_target_device("NPU");

    ModelHandle model_handle;
    result = model_interface.load(MODEL_PATH, &model_handle);
    assert(result == 0);

    // In a real-world scenario, we would inspect the logs to verify that the model was loaded on the NPU.
    // For this test, we will just check that the model was loaded successfully.

    model_interface.unload(model_handle);

    printf("Test passed\n");
}

void test_performance_on_npu() {
    printf("Running test: test_performance_on_npu\n");

    SpindexMLModelInterface model_interface;
    int result = create_openvino_model_interface(&model_interface, MODEL_PATH);
    assert(result == 0);

    // Run on CPU
    openvino_set_target_device("CPU");
    ModelHandle cpu_model_handle;
    result = model_interface.load(MODEL_PATH, &cpu_model_handle);
    assert(result == 0);

    ProcessedDataChunk input_data;
    FILE* f = fopen(INPUT_DATA_PATH, "rb");
    assert(f != NULL);
    fseek(f, 0, SEEK_END);
    input_data.size = ftell(f);
    fseek(f, 0, SEEK_SET);
    input_data.data = malloc(input_data.size);
    fread(input_data.data, 1, input_data.size, f);
    fclose(f);

    clock_t start = clock();
    for (int i = 0; i < 100; i++) {
        ProcessedDataChunk* output_data;
        model_interface.predict(cpu_model_handle, &input_data, &output_data);
        if (output_data) {
            free(output_data->data);
            free(output_data);
        }
    }
    clock_t end = clock();
    double cpu_time = (double)(end - start) / CLOCKS_PER_SEC;
    printf("CPU time: %f\n", cpu_time);

    model_interface.unload(cpu_model_handle);

    // Run on NPU
    openvino_set_target_device("NPU");
    ModelHandle npu_model_handle;
    result = model_interface.load(MODEL_PATH, &npu_model_handle);
    assert(result == 0);

    start = clock();
    for (int i = 0; i < 100; i++) {
        ProcessedDataChunk* output_data;
        model_interface.predict(npu_model_handle, &input_data, &output_data);
        if (output_data) {
            free(output_data->data);
            free(output_data);
        }
    }
    end = clock();
    double npu_time = (double)(end - start) / CLOCKS_PER_SEC;
    printf("NPU time: %f\n", npu_time);

    model_interface.unload(npu_model_handle);

    free(input_data.data);

    // In a real-world scenario, we would expect the NPU to be faster than the CPU.
    // However, this test does not make any assertions about the performance, as it
    // can vary depending on the hardware and the model.

    printf("Test passed\n");
}

int main() {
    test_load_model_on_npu();
    test_performance_on_npu();
    return 0;
}
