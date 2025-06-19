#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h> // For dynamic loading (dlopen, dlsym, dlclose)
#include <time.h> // For clock_gettime

// These paths are relative to the SPINDEX directory where 'make' is typically run,
// assuming the test harness is also run from there or can find them.
// For a test executable in SPINDEX/build/, the path to the plugin might be
// "./build/plugins/lib/openvino_classifier.so"
// For now, define a placeholder that might need adjustment in the Makefile or runtime.
// Corrected path assuming test executable is run from SPINDEX project root.
#define OPENVINO_PLUGIN_SO_PATH "./build/lib/openvino_classifier.so"

// We need the interface definitions.
// Path relative to SPINDEX/tests/ directory if test_openvino_classifier_hw.c is there.
#include "../include/ml_interface.h"      // For SpindexMLModelInterface, ModelHandle etc.
#include "../include/data_types.h"       // For ProcessedDataChunk
// We also need the declaration for openvino_set_target_device
// Assuming openvino_classifier.h is structured to be included by external code.
#include "../plugins/classifiers/openvino_classifier.h" // For openvino_set_target_device prototype

// Define function pointer types for the functions we'll get via dlsym
typedef int (*CREATE_OPENVINO_MODEL_INTERFACE_FUNC)(SpindexMLModelInterface*, const char*);
typedef void (*SET_OPENVINO_TARGET_DEVICE_FUNC)(const char*);

void print_usage(const char* prog_name) {
    fprintf(stderr, "Usage: %s <model_xml_path> <input_tensor_data_path> <device_name>\n", prog_name);
    fprintf(stderr, "  <model_xml_path>: Path to the OpenVINO model's .xml file.\n");
    fprintf(stderr, "  <input_tensor_data_path>: Path to a binary file containing the preprocessed input tensor data.\n");
    fprintf(stderr, "  <device_name>: Target device for OpenVINO (e.g., AUTO, CPU, GPU, NPU).\n");
}

// Simple helper to load a binary file into a ProcessedDataChunk
// This is a placeholder for how input tensor data might be loaded.
ProcessedDataChunk* load_tensor_data_from_file(const char* file_path) {
    FILE* f = fopen(file_path, "rb");
    if (!f) {
        perror("Error opening input tensor data file");
        return NULL;
    }

    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (file_size <= 0) {
        fprintf(stderr, "Error: Input tensor data file is empty or invalid size.\n");
        fclose(f);
        return NULL;
    }

    ProcessedDataChunk* chunk = (ProcessedDataChunk*)malloc(sizeof(ProcessedDataChunk));
    if (!chunk) {
        fprintf(stderr, "Error: Failed to allocate memory for ProcessedDataChunk.\n");
        fclose(f);
        return NULL;
    }
    memset(chunk, 0, sizeof(ProcessedDataChunk)); // Initialize

    chunk->data = malloc(file_size);
    if (!chunk->data) {
        fprintf(stderr, "Error: Failed to allocate memory for tensor data.\n");
        free(chunk);
        fclose(f);
        return NULL;
    }

    if (fread(chunk->data, 1, file_size, f) != (size_t)file_size) {
        fprintf(stderr, "Error: Failed to read full tensor data from file.\n");
        free(chunk->data);
        free(chunk);
        fclose(f);
        return NULL;
    }

    chunk->size = file_size; // Using 'size' as per ProcessedDataChunk for binary data
    chunk->length = file_size; // Also set length, though 'size' is more appropriate for binary
    chunk->data_type = CHUNK_TYPE_OPAQUE_DATA; // Or a more specific tensor type if defined
    chunk->source_id = strdup(file_path);

    fclose(f);
    printf("Loaded tensor data from '%s' (%ld bytes).\n", file_path, file_size);
    return chunk;
}

void free_processed_data_chunk(ProcessedDataChunk* chunk) {
    if (!chunk) return;
    if (chunk->source_id) free(chunk->source_id);
    if (chunk->data) free(chunk->data);
    free(chunk);
}


int main(int argc, char* argv[]) {
    if (argc != 4) {
        print_usage(argv[0]);
        return 1;
    }

    const char* model_xml_path = argv[1];
    const char* input_tensor_path = argv[2];
    const char* device_name_arg = argv[3];

    printf("Test Harness for OpenVINO Classifier\n");
    printf("-------------------------------------\n");
    printf("Plugin SO Path (expected): %s\n", OPENVINO_PLUGIN_SO_PATH);
    printf("Model XML Path: %s\n", model_xml_path);
    printf("Input Tensor Data Path: %s\n", input_tensor_path);
    printf("Target Device: %s\n", device_name_arg);

    void* plugin_handle = NULL;
    CREATE_OPENVINO_MODEL_INTERFACE_FUNC create_interface_func = NULL;
    SET_OPENVINO_TARGET_DEVICE_FUNC set_device_func = NULL;
    SpindexMLModelInterface model_interface;
    ModelHandle model_handle = NULL; // Stores model_interface.handle locally
    ProcessedDataChunk* input_chunk = NULL;
    ProcessedDataChunk* output_chunk = NULL;
    int ret_code = 1; // Default to error

    // Initialize model_interface members to NULL or default
    memset(&model_interface, 0, sizeof(SpindexMLModelInterface));

    // 1. Load the plugin library
    plugin_handle = dlopen(OPENVINO_PLUGIN_SO_PATH, RTLD_LAZY);
    if (!plugin_handle) {
        fprintf(stderr, "Error: Failed to load plugin shared library '%s': %s\n", OPENVINO_PLUGIN_SO_PATH, dlerror());
        goto cleanup;
    }
    printf("Plugin library '%s' loaded successfully.\n", OPENVINO_PLUGIN_SO_PATH);

    // 2. Get function pointers from the plugin
    create_interface_func = (CREATE_OPENVINO_MODEL_INTERFACE_FUNC)dlsym(plugin_handle, "create_openvino_model_interface");
    if (!create_interface_func) {
        fprintf(stderr, "Error: Failed to find 'create_openvino_model_interface' in plugin: %s\n", dlerror());
        goto cleanup;
    }
    printf("'create_openvino_model_interface' function found.\n");

    set_device_func = (SET_OPENVINO_TARGET_DEVICE_FUNC)dlsym(plugin_handle, "openvino_set_target_device");
    if (!set_device_func) {
        fprintf(stderr, "Error: Failed to find 'openvino_set_target_device' in plugin: %s\n", dlerror());
        goto cleanup;
    }
    printf("'openvino_set_target_device' function found.\n");

    // 3. Create the model interface
    if (create_interface_func(&model_interface, model_xml_path) != 0) {
        fprintf(stderr, "Error: create_openvino_model_interface failed.\n");
        goto cleanup;
    }
    printf("OpenVINO model interface created. Description: %s\n", model_interface.model_description);

    // 4. Set the target device
    set_device_func(device_name_arg);
    printf("Target device set to '%s' via plugin function.\n", device_name_arg);

    // 5. Load the model
    printf("Loading model: %s...\n", model_xml_path);
    if (!model_interface.load) {
        fprintf(stderr, "Error: model_interface.load function pointer is NULL.\n");
        goto cleanup;
    }
    if (model_interface.load(model_xml_path, &model_interface.handle) != 0) {
        fprintf(stderr, "Error: model_interface.load failed for model '%s'.\n", model_xml_path);
        goto cleanup;
    }
    model_handle = model_interface.handle;
    printf("Model '%s' loaded successfully. Handle: %p\n", model_xml_path, (void*)model_handle);

    // 6. Load input tensor data
    input_chunk = load_tensor_data_from_file(input_tensor_path);
    if (!input_chunk) {
        fprintf(stderr, "Error: Failed to load input tensor data from '%s'.\n", input_tensor_path);
        goto cleanup;
    }

    // 7. Perform inference (with timing and iterations)
    if (!model_interface.predict) {
        fprintf(stderr, "Error: model_interface.predict function pointer is NULL.\n");
        goto cleanup;
    }

    struct timespec start_time, end_time;
    long total_ns = 0;
    int num_iterations = 10; // Example, make this configurable later if needed
    printf("Performing %d inference iterations...\n", num_iterations);

    for (int i = 0; i < num_iterations; ++i) {
        if (output_chunk) { // Free output from previous iteration
            free_processed_data_chunk(output_chunk);
            output_chunk = NULL;
        }

        clock_gettime(CLOCK_MONOTONIC, &start_time);
        if (model_interface.predict(model_handle, input_chunk, &output_chunk) != 0) {
            fprintf(stderr, "Error: model_interface.predict failed during iteration %d.\n", i + 1);
            goto cleanup;
        }
        clock_gettime(CLOCK_MONOTONIC, &end_time);

        if (!output_chunk || !output_chunk->data) {
            fprintf(stderr, "Error: Prediction succeeded but output_chunk or its data is NULL during iteration %d.\n", i + 1);
            goto cleanup;
        }

        long iteration_ns = (end_time.tv_sec - start_time.tv_sec) * 1000000000L + (end_time.tv_nsec - start_time.tv_nsec);
        total_ns += iteration_ns;

        if (i == 0) { // Only print details for the first iteration's output
            printf("    Iteration %d: Output size: %zu bytes. Latency: %.3f ms\n",
                   i + 1, output_chunk->size, (double)iteration_ns / 1000000.0);
            printf("    Output tensor (first few bytes as hex from iter 0): ");
            for (size_t k = 0; k < output_chunk->size && k < 16; ++k) {
                printf("%02X ", ((unsigned char*)output_chunk->data)[k]);
            }
            printf("\n");
        }
    }

    if (num_iterations > 0) {
        double avg_latency_ms = (double)total_ns / num_iterations / 1000000.0;
        double throughput_ips = (double)num_iterations / ((double)total_ns / 1000000000.0);
        printf("-------------------------------------\n");
        printf("Performance Summary:\n");
        printf("  Total iterations: %d\n", num_iterations);
        printf("  Total time: %.3f ms\n", (double)total_ns / 1000000.0);
        printf("  Average latency: %.3f ms/inference\n", avg_latency_ms);
        printf("  Throughput: %.2f inferences/sec\n", throughput_ips);
        printf("-------------------------------------\n");
    }

    ret_code = 0;
    printf("Test harness execution completed successfully.\n");

cleanup:
    printf("Cleaning up resources...\n");
    if (output_chunk) {
        free_processed_data_chunk(output_chunk);
        output_chunk = NULL;
    }
    if (input_chunk) {
        free_processed_data_chunk(input_chunk);
        input_chunk = NULL;
    }
    // Use model_interface.handle for unload, as it's the one set by 'load'
    if (model_interface.handle && model_interface.unload) {
        printf("Unloading model...\n");
        model_interface.unload(model_interface.handle);
        model_interface.handle = NULL;
    } else if (model_interface.handle) { // Unload pointer is null but handle is not
        fprintf(stderr, "Warning: model_interface.unload function pointer is null, cannot unload model explicitly.\n");
    }

    if (plugin_handle) {
        dlclose(plugin_handle);
        printf("Plugin library closed.\n");
    }

    printf("Test harness finished with exit code %d.\n", ret_code);
    return ret_code;
}
