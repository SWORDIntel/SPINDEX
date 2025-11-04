#include "openvino_classifier.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h> // Ensure string.h is included for strncpy/snprintf if not already

// OpenVINO headers are now in openvino_classifier.h

static char current_target_device[32] = "AUTO"; // Default device

// Struct to hold OpenVINO runtime objects
typedef struct {
    ov_core_t* core;
    ov_model_t* model;
    ov_compiled_model_t* compiled_model;
    ov_infer_request_t* infer_request;
    char* model_path_copy; // In case model path is needed later
    char compiled_on_device[32]; // Store the device name it was compiled on
} OpenVINOModel;


int create_openvino_model_interface(SpindexMLModelInterface* model_interface, const char* model_path) {
    if (!model_interface) {
        fprintf(stderr, "Error: create_openvino_model_interface received null interface pointer\n");
        return 1;
    }

    model_interface->load = openvino_load_model;
    model_interface->unload = openvino_unload_model;
    model_interface->predict = openvino_predict;
    model_interface->handle = NULL; // Will be set by openvino_load_model

    strncpy(model_interface->interface_name, "OpenVINO_Classifier_v1", sizeof(model_interface->interface_name) - 1);
    model_interface->interface_name[sizeof(model_interface->interface_name) - 1] = '\0';

    // model_path is passed to openvino_load_model. The model isn't loaded here.
    // strcpy(model_interface->model_description, model_path ? model_path : "No model path specified yet");
    snprintf(model_interface->model_description, sizeof(model_interface->model_description),
             "OpenVINO model. Path: %s. Target device (set via openvino_set_target_device): %s",
             model_path ? model_path : "NULL", current_target_device);


    fprintf(stdout, "OpenVINO model interface configured. Call 'load' to load model: %s\n", model_path ? model_path : "NULL");
    return 0;
}

int openvino_load_model(const char* model_path, ModelHandle* model_handle_ptr) {
    if (!model_path || !model_handle_ptr) {
        fprintf(stderr, "Error: openvino_load_model received null model_path or model_handle_ptr\n");
        return 1;
    }
    fprintf(stdout, "Loading OpenVINO model from: %s\n", model_path);

    OpenVINOModel* ov_model = (OpenVINOModel*)malloc(sizeof(OpenVINOModel));
    if (!ov_model) {
        fprintf(stderr, "Error: Failed to allocate memory for OpenVINOModel struct\n");
        return 1;
    }
    ov_model->core = NULL;
    ov_model->model = NULL;
    ov_model->compiled_model = NULL;
    ov_model->infer_request = NULL;
    ov_model->model_path_copy = NULL;
    ov_model->compiled_on_device[0] = '\0'; // Initialize compiled_on_device


    ov_model->model_path_copy = strdup(model_path);
    if (!ov_model->model_path_copy) {
        fprintf(stderr, "Error: Failed to duplicate model_path string\n");
        free(ov_model);
        return 1;
    }

    ov_status_e status;

    // 1. Initialize OpenVINO core
    status = ov_core_create(&ov_model->core);
    if (status != OV_STATUS_SUCCESS) {
        fprintf(stderr, "Error: Failed to create OpenVINO Core. Status: %d\n", status);
        free(ov_model->model_path_copy);
        free(ov_model);
        return 1;
    }

    // 2. Read the model.
    //    The model_path should point to an OpenVINO IR model (.xml file).
    //    The corresponding .bin file should be in the same directory.
    //    For optimal performance as discussed in FP16.MD, this model should ideally be
    //    pre-converted to FP16 precision using the OpenVINO Model Optimizer.
    fprintf(stdout, "Reading OpenVINO model (expected FP16 IR format): %s\n", model_path);
    status = ov_core_read_model_from_file(ov_model->core, model_path, NULL, &ov_model->model);
    if (status != OV_STATUS_SUCCESS) {
        fprintf(stderr, "Error: Failed to read OpenVINO model from file: %s. Status: %d\n", model_path, status);
        ov_core_free(ov_model->core);
        free(ov_model->model_path_copy);
        free(ov_model);
        return 1;
    }

    // 3. Compile the model for the target device.
    //    "AUTO" allows OpenVINO to select the best available device.
    //    For Intel Meteor Lake 165h, this could be the NPU.
    //    To explicitly target the NPU, "NPU" can be used.
    //    Device selection could be made configurable in a future enhancement.
    // const char* device_name = "AUTO"; // Old line
    fprintf(stdout, "Compiling OpenVINO model for device: %s (from current_target_device)\n", current_target_device);
    status = ov_core_compile_model(ov_model->core, ov_model->model, current_target_device, 0, &ov_model->compiled_model);
    if (status != OV_STATUS_SUCCESS) {
        fprintf(stderr, "Error: Failed to compile OpenVINO model for device %s. Status: %d\n", current_target_device, status);
        ov_model_free(ov_model->model);
        ov_core_free(ov_model->core);
        free(ov_model->model_path_copy);
        free(ov_model);
        return 1;
    }

    // 3.1. Optional: Set performance hints or device-specific properties.
    //      For Meteor Lake NPU optimization, this could involve setting properties
    //      like performance mode (e.g., OV_HINT_PERFORMANCE_MODE_LATENCY or OV_HINT_PERFORMANCE_MODE_THROUGHPUT)
    //      using ov_compiled_model_set_property.
    //      Example (conceptual, actual property names/values may vary):
    //      ov_property_value_t perf_mode_latency = {OV_HINT_PERFORMANCE_MODE_LATENCY, {0}};
    //      status = ov_compiled_model_set_property(ov_model->compiled_model, OV_HINT_PERFORMANCE_MODE_KEY, perf_mode_latency);
    //      if (status != OV_STATUS_SUCCESS) {
    //          fprintf(stderr, "Warning: Failed to set OpenVINO performance hint. Status: %d\n", status);
    //      }


    // 4. Create an inference request
    status = ov_compiled_model_create_infer_request(ov_model->compiled_model, &ov_model->infer_request);
    if (status != OV_STATUS_SUCCESS) {
        fprintf(stderr, "Error: Failed to create OpenVINO inference request. Status: %d\n", status);
        ov_compiled_model_free(ov_model->compiled_model);
        ov_model_free(ov_model->model);
        ov_core_free(ov_model->core);
        free(ov_model->model_path_copy);
        free(ov_model);
        return 1;
    }

    // After successfully creating infer_request and before returning 0
    strncpy(ov_model->compiled_on_device, current_target_device, sizeof(ov_model->compiled_on_device) - 1);
    ov_model->compiled_on_device[sizeof(ov_model->compiled_on_device) - 1] = '\0';

    *model_handle_ptr = (ModelHandle)ov_model;
    fprintf(stdout, "OpenVINO model loaded and compiled successfully on device '%s'. Handle: %p\n", ov_model->compiled_on_device, *model_handle_ptr);
    return 0; // Success
}

void openvino_set_target_device(const char* device) {
    if (device && strlen(device) < sizeof(current_target_device)) {
        strncpy(current_target_device, device, sizeof(current_target_device) - 1);
        current_target_device[sizeof(current_target_device) - 1] = '\0';
        fprintf(stdout, "OpenVINO target device set to: %s\n", current_target_device);
    } else if (device) {
        fprintf(stderr, "Error: Provided device name '%s' is too long (max %zu chars).\n", device, sizeof(current_target_device) -1);
    } else {
        fprintf(stderr, "Error: Provided device name is NULL. Current target device '%s' unchanged.\n", current_target_device);
    }
}

void openvino_unload_model(ModelHandle model_handle) {
    if (!model_handle) {
        fprintf(stderr, "Warning: openvino_unload_model received null model_handle\n");
        return;
    }
    fprintf(stdout, "Unloading OpenVINO model. Handle: %p\n", model_handle);

    OpenVINOModel* ov_model = (OpenVINOModel*)model_handle;

    if (ov_model->infer_request) {
        ov_infer_request_free(ov_model->infer_request);
    }
    if (ov_model->compiled_model) {
        ov_compiled_model_free(ov_model->compiled_model);
    }
    if (ov_model->model) {
        ov_model_free(ov_model->model);
    }
    if (ov_model->core) {
        ov_core_free(ov_model->core);
    }
    if (ov_model->model_path_copy) {
        free(ov_model->model_path_copy);
    }
    free(ov_model);

    fprintf(stdout, "OpenVINO model unloaded successfully.\n");
}

int openvino_predict(ModelHandle model_handle,
                     const ProcessedData* input_data,
                     ProcessedData** output_data_ptr) {
    if (!model_handle || !input_data || !input_data->chunks || !output_data_ptr) {
        fprintf(stderr, "Error: openvino_predict received null arguments or empty input data\n");
        if (output_data_ptr) *output_data_ptr = NULL;
        return 1;
    }
    *output_data_ptr = NULL;

    OpenVINOModel* ov_model = (OpenVINOModel*)model_handle;
    if (!ov_model->infer_request) {
        fprintf(stderr, "Error: OpenVINO infer_request is null in predict.\n");
        return 1;
    }

    ov_model_t* actual_model;
    ov_compiled_model_get_runtime_model(ov_model->compiled_model, &actual_model);

    size_t num_inputs = 0;
    ov_model_inputs_size(actual_model, &num_inputs);
    if (input_data->count != num_inputs) {
        fprintf(stderr, "Error: Mismatched number of inputs. Model expects %zu, but %zu were provided.\n", num_inputs, input_data->count);
        ov_model_free(actual_model);
        return 1;
    }

    for (size_t i = 0; i < num_inputs; ++i) {
        ov_tensor_t* input_tensor = NULL;
        ov_infer_request_get_input_tensor(ov_model->infer_request, i, &input_tensor);

        size_t input_tensor_byte_size;
        ov_tensor_get_byte_size(input_tensor, &input_tensor_byte_size);

        if (input_data->chunks[i].length != input_tensor_byte_size) {
            fprintf(stderr, "Error: Input data size mismatch for input %zu.\n", i);
            ov_tensor_free(input_tensor);
            ov_model_free(actual_model);
            return 1;
        }

        void* input_tensor_data;
        ov_tensor_get_data(input_tensor, &input_tensor_data);
        memcpy(input_tensor_data, input_data->chunks[i].data, input_tensor_byte_size);

        ov_tensor_free(input_tensor);
    }

    ov_infer_request_infer(ov_model->infer_request);

    size_t num_outputs = 0;
    ov_model_outputs_size(actual_model, &num_outputs);

    *output_data_ptr = (ProcessedData*)malloc(sizeof(ProcessedData));
    (*output_data_ptr)->chunks = (DataChunk*)malloc(num_outputs * sizeof(DataChunk));
    (*output_data_ptr)->count = num_outputs;

    for (size_t i = 0; i < num_outputs; ++i) {
        ov_tensor_t* output_tensor = NULL;
        ov_infer_request_get_output_tensor(ov_model->infer_request, i, &output_tensor);

        size_t output_tensor_byte_size;
        ov_tensor_get_byte_size(output_tensor, &output_tensor_byte_size);

        void* output_tensor_data;
        ov_tensor_get_data(output_tensor, &output_tensor_data);

        (*output_data_ptr)->chunks[i].data = malloc(output_tensor_byte_size);
        memcpy((*output_data_ptr)->chunks[i].data, output_tensor_data, output_tensor_byte_size);
        (*output_data_ptr)->chunks[i].length = output_tensor_byte_size;

        ov_tensor_free(output_tensor);
    }

    for (size_t i = 0; i < num_outputs; ++i) {
        // This is a placeholder for the actual post-processing logic.
        // In a real-world scenario, you would parse the output tensor and
        // generate a meaningful JSON string with the classification results.
        char metadata[256];
        snprintf(metadata, sizeof(metadata), "{\"class_label\": \"dummy\", \"class_id\": 0, \"score\": 0.99}");
        (*output_data_ptr)->chunks[i].metadata = strdup(metadata);
    }

    ov_model_free(actual_model);
    return 0;
}

// --- Plugin Registration ---

#define SPINDEX_PLUGIN_API __attribute__((visibility("default")))

SPINDEX_PLUGIN_API void spindex_plugin_initialize(void (*register_plugin)(SpindexPlugin)) {
    SpindexPlugin plugin;
    plugin.info.type = SPINDEX_PLUGIN_TYPE_ML_CLASSIFIER;
    strncpy(plugin.info.name, "OpenVINO Classifier", sizeof(plugin.info.name) - 1);
    plugin.info.name[sizeof(plugin.info.name) - 1] = '\0';
    // ... set other info fields ...
    plugin.ml_model_interface.create = create_openvino_model_interface;

    register_plugin(plugin);
}
