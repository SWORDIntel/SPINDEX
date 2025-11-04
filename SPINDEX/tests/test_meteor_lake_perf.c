#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>
#include <assert.h>

#include "../../include/plugin_interface.h"
#include "../../include/data_types.h"
#include "../../plugins/classifiers/openvino_classifier.h"

// --- Test Utilities ---
#define ASSERT_NOT_NULL(ptr) assert((ptr) != NULL)
#define ASSERT_EQ(val1, val2) assert((val1) == (val2))

// --- Test Case ---

void test_meteor_lake_performance() {
    printf("--- Testing Meteor Lake FP16 Performance ---\n");

    // 1. Create OpenVINO model interface
    SpindexMLModelInterface model_interface;
    int result = create_openvino_model_interface(&model_interface, "model.xml");
    ASSERT_EQ(result, 0);

    // 2. Load the model
    ModelHandle model_handle;
    result = model_interface.load("model.xml", &model_handle);
    ASSERT_EQ(result, 0);

    // 3. Prepare input data
    ProcessedData input_data;
    input_data.count = 1;
    input_data.chunks = (DataChunk*)malloc(sizeof(DataChunk));
    input_data.chunks[0].length = 1 * 3 * 224 * 224 * 2; // FP16
    input_data.chunks[0].data = malloc(input_data.chunks[0].length);
    // Fill with dummy data
    memset(input_data.chunks[0].data, 0, input_data.chunks[0].length);

    // 4. Run inference on different devices
    const char* devices[] = {"AUTO", "CPU", "NPU", "GNA", "MYRIAD"};
    int num_devices = sizeof(devices) / sizeof(devices[0]);

    printf("| Device | Avg. Inference Time (ms) |\n");
    printf("|--------|--------------------------|\n");

    for (int i = 0; i < num_devices; i++) {
        openvino_set_target_device(devices[i]);

        // Reload the model for the new device
        model_interface.unload(model_handle);
        result = model_interface.load("model.xml", &model_handle);
        if (result != 0) {
            printf("| %-6s | Error loading model      |\n", devices[i]);
            continue;
        }

        // Warm-up run
        ProcessedData* output_data;
        model_interface.predict(model_handle, &input_data, &output_data);
        if (output_data) {
            for (size_t j = 0; j < output_data->count; j++) {
                free(output_data->chunks[j].data);
                if(output_data->chunks[j].metadata) free(output_data->chunks[j].metadata);
            }
            free(output_data->chunks);
            free(output_data);
        }

        // Performance test
        int num_runs = 100;
        struct timespec start, end;
        clock_gettime(CLOCK_MONOTONIC, &start);
        for (int j = 0; j < num_runs; j++) {
            model_interface.predict(model_handle, &input_data, &output_data);
            if (output_data) {
                for (size_t k = 0; k < output_data->count; k++) {
                    free(output_data->chunks[k].data);
                    if(output_data->chunks[k].metadata) free(output_data->chunks[k].metadata);
                }
                free(output_data->chunks);
                free(output_data);
            }
        }
        clock_gettime(CLOCK_MONOTONIC, &end);
        double time_spent = (end.tv_sec - start.tv_sec) * 1000.0 + (end.tv_nsec - start.tv_nsec) / 1000000.0;
        double avg_time_ms = time_spent / num_runs;
        printf("| %-6s | %-24.2f |\n", devices[i], avg_time_ms);
    }

    // 5. Cleanup
    model_interface.unload(model_handle);
    free(input_data.chunks[0].data);
    free(input_data.chunks);
}

int main() {
    test_meteor_lake_performance();
    return 0;
}
