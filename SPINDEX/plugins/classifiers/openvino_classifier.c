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

void openvino_set_target_device(const char* device);

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
                     const ProcessedDataChunk* input_data,
                     ProcessedDataChunk** output_data_ptr) {
    if (!model_handle || !input_data || !input_data->data || !output_data_ptr) {
        fprintf(stderr, "Error: openvino_predict received null arguments or empty input data\n");
        if (output_data_ptr) *output_data_ptr = NULL;
        return 1;
    }
    *output_data_ptr = NULL; // Initialize as per contract

    OpenVINOModel* ov_model = (OpenVINOModel*)model_handle;
    if (!ov_model->infer_request) {
        fprintf(stderr, "Error: OpenVINO infer_request is null in predict.\n");
        return 1;
    }

    fprintf(stdout, "Performing OpenVINO prediction. Handle: %p\n", model_handle);
    ov_status_e status;
    ov_tensor_t* input_tensor = NULL;
    ov_tensor_t* output_tensor = NULL;

    // 1. Get input tensor(s)
    ov_model_t* actual_model;
    status = ov_compiled_model_get_runtime_model(ov_model->compiled_model, &actual_model);
    if (status != OV_STATUS_SUCCESS) {
        fprintf(stderr, "Error: Failed to get runtime model to check inputs. Status: %d\n", status);
        return 1;
    }
    size_t num_inputs = 0;
    status = ov_model_inputs_size(actual_model, &num_inputs);
    if (status != OV_STATUS_SUCCESS) {
        fprintf(stderr, "Error: Failed to get model input count. Status: %d\n", status);
        ov_model_free(actual_model);
        return 1;
    }
    if (num_inputs > 1) {
        fprintf(stderr, "Error: Model has %zu inputs. This classifier implementation currently supports only single-input models.\n", num_inputs);
        // TODO: To handle models with multiple inputs, ProcessedDataChunk might need to be adapted or a new input data structure provided that can represent multiple input tensors. The loop for getting input tensors and preprocessing would also need to be updated.
        ov_model_free(actual_model);
        return 1; // Indicate error due to unsupported multi-input model
    }
    if (num_inputs == 0) {
        fprintf(stderr, "Error: Model has no inputs.\n");
        ov_model_free(actual_model);
        return 1;
    }
    ov_model_free(actual_model); // Free the runtime model handle

    status = ov_infer_request_get_input_tensor(ov_model->infer_request, 0, &input_tensor);
    if (status != OV_STATUS_SUCCESS) {
        fprintf(stderr, "Error: Failed to get input tensor (index 0). Status: %d\n", status);
        return 1;
    }

    // 2. Prepare input data and fill the tensor.
    //    This is a CRITICAL section that is highly model-dependent.
    //    It requires knowing the model's expected input shape, data type, layout (NCHW/NHWC),
    //    and any preprocessing steps like normalization, resizing, type conversion.

    //    For this placeholder, we'll make a simplifying assumption:
    //    - The input_data->data is already in the correct format and size.
    //    - We'll check if input_data->size matches the tensor's byte size.

    ov_shape_t input_shape;
    status = ov_tensor_get_shape(input_tensor, &input_shape);
    if (status != OV_STATUS_SUCCESS) {
        fprintf(stderr, "Error: Failed to get input tensor shape. Status: %d\n", status);
        ov_tensor_free(input_tensor);
        return 1;
    }

    ov_element_type_e input_element_type;
    status = ov_tensor_get_element_type(input_tensor, &input_element_type);
    if (status != OV_STATUS_SUCCESS) {
        fprintf(stderr, "Error: Failed to get input tensor element type. Status: %d\n", status);
        ov_shape_free(&input_shape);
        ov_tensor_free(input_tensor);
        return 1;
    }

    char shape_str[256] = {0};
    snprintf(shape_str, sizeof(shape_str)-1, "[");
    for (size_t i = 0; i < input_shape.rank; ++i) {
        char dim_str[32];
        snprintf(dim_str, sizeof(dim_str)-1, "%lld", (long long int)input_shape.dims[i]); // Use lld for int64_t
        strncat(shape_str, dim_str, sizeof(shape_str) - strlen(shape_str) -1);
        if (i < input_shape.rank - 1) {
            strncat(shape_str, ",", sizeof(shape_str) - strlen(shape_str) -1);
        }
    }
    strncat(shape_str, "]", sizeof(shape_str) - strlen(shape_str) -1);

    const char* element_type_str = ov_element_type_get_string(input_element_type);

    size_t input_tensor_byte_size;
    status = ov_tensor_get_byte_size(input_tensor, &input_tensor_byte_size);
    if (status != OV_STATUS_SUCCESS) {
        fprintf(stderr, "Error: Failed to get input tensor byte size. Status: %d\n", status);
        ov_shape_free(&input_shape);
        ov_tensor_free(input_tensor);
        return 1;
    }

    fprintf(stdout, "Model expects input tensor with shape: %s, element type: %s, total byte size: %zu\n",
            shape_str, element_type_str ? element_type_str : "UNKNOWN", input_tensor_byte_size);
    ov_shape_free(&input_shape); // Free the shape object

    if (input_data->size != input_tensor_byte_size) {
        fprintf(stderr, "Error: Provided input data size (%zu) does not match model's expected input tensor byte size (%zu).\n",
                input_data->size, input_tensor_byte_size);
        fprintf(stderr, "Critical: Proper preprocessing (e.g., resizing, normalization, type conversion, layout changes like NCHW/NHWC) is required to match model expectations. This is currently NOT IMPLEMENTED in this plugin.\n");
        fprintf(stderr, "Recommendation: Use OpenVINO Model Optimizer to embed preprocessing steps into the model (.xml) itself, or ensure input data is perfectly preprocessed before calling this plugin.\n");
        // TODO: For robust general-purpose classification, this plugin would need to incorporate or allow registration of model-specific preprocessing routines (e.g., image resizing, normalization, tokenization). Consider using OpenVINO's G-API or Preprocessing API if embedding into the model is not feasible.
        ov_tensor_free(input_tensor);
        return 1;
    }

    // Copy data to the input tensor
    // The tensor data buffer is allocated by OpenVINO. We just copy our data into it.
    void* input_tensor_data;
    status = ov_tensor_get_data(input_tensor, &input_tensor_data);
    if (status != OV_STATUS_SUCCESS) {
        fprintf(stderr, "Error: Failed to get input tensor data pointer. Status: %d\n", status);
        ov_tensor_free(input_tensor);
        return 1;
    }
    memcpy(input_tensor_data, input_data->data, input_tensor_byte_size);
    // ov_tensor_set_data is not what we want here if we're copying into existing buffer.
    // If input_data->data was *exactly* what OV expects and we wanted to avoid a copy,
    // we might try to use ov_tensor_set_data, but that's more complex with ownership.

    // 3. Perform inference
    status = ov_infer_request_infer(ov_model->infer_request);
    if (status != OV_STATUS_SUCCESS) {
        fprintf(stderr, "Error: Failed to perform inference. Status: %d\n", status);
        ov_tensor_free(input_tensor);
        return 1;
    }

    // 4. Get output tensor(s)
    // Re-fetch runtime model to check output count (actual_model was freed earlier)
    ov_model_t* runtime_model_for_output; // Use a new variable name to avoid confusion if actual_model was somehow still in scope
    status = ov_compiled_model_get_runtime_model(ov_model->compiled_model, &runtime_model_for_output);
    if (status != OV_STATUS_SUCCESS) {
        fprintf(stderr, "Error: Failed to get runtime model to check outputs. Status: %d\n", status);
        ov_tensor_free(input_tensor);
        return 1;
    }
    size_t num_outputs = 0;
    status = ov_model_outputs_size(runtime_model_for_output, &num_outputs);
    if (status != OV_STATUS_SUCCESS) {
        fprintf(stderr, "Error: Failed to get model output count. Status: %d\n", status);
        ov_model_free(runtime_model_for_output);
        ov_tensor_free(input_tensor);
        return 1;
    }
    if (num_outputs > 1) {
        fprintf(stderr, "Error: Model has %zu outputs. This classifier implementation currently supports only single-output models.\n", num_outputs);
        // TODO: To handle models with multiple outputs, ProcessedDataChunk might need to be adapted or a new output data structure designed to hold multiple output tensors. The loop for getting output tensors and postprocessing would also need to be updated.
        ov_model_free(runtime_model_for_output);
        ov_tensor_free(input_tensor); // Free the input tensor allocated earlier
        return 1; // Indicate error due to unsupported multi-output model
    }
    if (num_outputs == 0) {
        fprintf(stderr, "Error: Model has no outputs.\n");
        ov_model_free(runtime_model_for_output);
        ov_tensor_free(input_tensor);
        return 1;
    }
    ov_model_free(runtime_model_for_output); // Free the runtime model handle

    status = ov_infer_request_get_output_tensor(ov_model->infer_request, 0, &output_tensor);
    if (status != OV_STATUS_SUCCESS) {
        fprintf(stderr, "Error: Failed to get output tensor (index 0). Status: %d\n", status);
        ov_tensor_free(input_tensor);
        return 1;
    }

    // 5. Process output tensor and create ProcessedDataChunk
    //    This is also model-dependent (output shape, data type, post-processing).

    size_t output_tensor_byte_size;
    status = ov_tensor_get_byte_size(output_tensor, &output_tensor_byte_size);
    if (status != OV_STATUS_SUCCESS) {
        fprintf(stderr, "Error: Failed to get output tensor byte size. Status: %d\n", status);
        ov_tensor_free(input_tensor);
        ov_tensor_free(output_tensor);
        return 1;
    }

    void* output_tensor_data;
    status = ov_tensor_get_data(output_tensor, &output_tensor_data);
    if (status != OV_STATUS_SUCCESS) {
        fprintf(stderr, "Error: Failed to get output tensor data pointer. Status: %d\n", status);
        ov_tensor_free(input_tensor);
        ov_tensor_free(output_tensor);
        return 1;
    }

    // Allocate ProcessedDataChunk for the output
    *output_data_ptr = (ProcessedDataChunk*)malloc(sizeof(ProcessedDataChunk));
    if (!*output_data_ptr) {
        fprintf(stderr, "Error: Failed to allocate memory for output ProcessedDataChunk\n");
        ov_tensor_free(input_tensor);
        ov_tensor_free(output_tensor);
        return 1;
    }
    // Initialize to prevent use of uninitialized memory
    (*output_data_ptr)->data = NULL;
    (*output_data_ptr)->size = 0;
    (*output_data_ptr)->metadata = NULL;

    // Allocate memory for the data within the ProcessedDataChunk
    (*output_data_ptr)->data = malloc(output_tensor_byte_size);
    if (!(*output_data_ptr)->data) {
        fprintf(stderr, "Error: Failed to allocate memory for output data within ProcessedDataChunk\n");
        free(*output_data_ptr);
        *output_data_ptr = NULL;
        ov_tensor_free(input_tensor);
        ov_tensor_free(output_tensor);
        return 1;
    }

    // Copy data from the output tensor to the ProcessedDataChunk
    memcpy((*output_data_ptr)->data, output_tensor_data, output_tensor_byte_size);
    (*output_data_ptr)->size = output_tensor_byte_size;
    (*output_data_ptr)->metadata = NULL; // Explicitly NULL for now

    // TODO: Populate (*output_data_ptr)->metadata with model-specific post-processing results.
    // This typically involves:
    // 1. Interpreting the output_tensor_data based on the model's architecture (e.g., logits, probabilities).
    // 2. Applying any necessary post-processing (e.g., softmax, finding max score index).
    // 3. Mapping numerical results to human-readable labels (requires a label mapping file/mechanism).
    // 4. Formatting the results as a JSON string.
    // Example for a hypothetical classification model outputting a float array of scores:
    /*
    if (output_tensor_byte_size > 0 && (*output_data_ptr)->data) {
        // Assuming output is float scores and we have a labels array char* class_labels[]
        // float* scores = (float*)(*output_data_ptr)->data;
        // size_t num_classes = output_tensor_byte_size / sizeof(float);
        // int max_idx = 0;
        // float max_score = scores[0];
        // for (size_t i = 1; i < num_classes; ++i) {
        //     if (scores[i] > max_score) {
        //         max_score = scores[i];
        //         max_idx = i;
        //     }
        // }
        // char temp_metadata[256];
        // snprintf(temp_metadata, sizeof(temp_metadata),
        //          "{\"class_label\": \"%s\", \"class_id\": %d, \"score\": %.4f, \"device_used\": \"%s\"}",
        //          (max_idx < NUM_LABELS ? class_labels[max_idx] : "Unknown"), max_idx, max_score, ov_model->compiled_on_device);
        // (*output_data_ptr)->metadata = strdup(temp_metadata);
    }
    */
    fprintf(stdout, "Placeholder: Output metadata not populated. Requires model-specific post-processing and label mapping.\n");

    fprintf(stdout, "OpenVINO prediction complete. Output size: %zu bytes. Device used: %s\n", output_tensor_byte_size, ov_model->compiled_on_device);

    // Free OpenVINO tensors
    ov_tensor_free(input_tensor);
    ov_tensor_free(output_tensor);

    return 0; // Success
}

// TODO: Future Enhancements & Considerations:
// - Further improve error handling and consider integrating with a core logging utility if available.
// - Detailed FP16 handling: Ensure data type consistency and explore FP16-specific optimizations during inference if applicable beyond model conversion.
// - Advanced hardware optimizations: Explore setting more OpenVINO performance hints or device-specific properties for optimal performance on target hardware (e.g., NPUs).
// - Thread Safety: If a single ModelHandle is to be used concurrently by multiple threads, ensure openvino_predict is thread-safe. This typically means each thread needs its own ov_infer_request_t. The current implementation is not inherently thread-safe for concurrent predict calls on the same handle.

// Assuming plugin_interface.h or a similar header would define
// how plugins register themselves. For now, this is a conceptual include.
// #include "plugin_interface.h" // For SpindexPluginCollection, register_ml_model_interface etc.

// Define a simplified placeholder for plugin registration.
// In a real system, this would interact with plugin_loader.c.
typedef void (*register_model_func_t)(const char* name, SpindexMLModelInterface* interface);

// Dummy registration function - in a real scenario, this would be provided by the core.
void dummy_register_ml_model(const char* name, SpindexMLModelInterface* interface) {
    if (name && interface) {
        fprintf(stdout, "Successfully registered ML Model Interface: %s using %s\n", name, interface->interface_name);
        // Here, the plugin loader would store this interface.
    } else {
        fprintf(stderr, "Failed to register ML Model Interface: null name or interface\n");
    }
}
// End dummy registration


void openvino_set_target_device(const char* device) {
    if (device) {
        strncpy(current_target_device, device, sizeof(current_target_device) - 1);
        current_target_device[sizeof(current_target_device) - 1] = '\0';
    }
}

// This is a common function name that a plugin loader might look for using dlsym.
// The actual name and signature depend on the plugin system's design.
// We'll make it simple for now.
#define SPINDEX_PLUGIN_API __attribute__((visibility("default")))

SPINDEX_PLUGIN_API void spindex_plugin_initialize(register_model_func_t register_func) {
    fprintf(stdout, "Attempting to initialize and register OpenVINO Classifier Plugin...\n");

    if (!register_func) {
        fprintf(stderr, "OpenVINO Plugin Error: Registration function not provided.\n");
        return;
    }

    SpindexMLModelInterface ov_interface;

    // Initialize the OpenVINO model interface structure.
    // The actual model path would typically come from SPINDEX configuration
    // and be passed to the plugin, or the plugin would fetch it.
    // For this example, we pass NULL, assuming the 'load' function will receive the actual path.
    // Or, a specific model configuration could be hardcoded here if the plugin is for one model.
    const char* default_model_config_path = "path/to/your/openvino_model.xml"; // Example path

    int result = create_openvino_model_interface(&ov_interface, default_model_config_path);

    if (result == 0) {
        // Register this interface with the SPINDEX core/plugin manager.
        // The name "openvino_default_classifier" is an example.
        register_func("openvino_default_classifier", &ov_interface);
        fprintf(stdout, "OpenVINO Classifier Plugin initialized and registration requested.\n");
    } else {
        fprintf(stderr, "Failed to create OpenVINO model interface during plugin initialization.\n");
    }
}

// Optional: A function to deinitialize/cleanup the plugin if the system supports it.
SPINDEX_PLUGIN_API void spindex_plugin_finalize() {
    fprintf(stdout, "OpenVINO Classifier Plugin finalizing (if any global cleanup needed).\n");
    // If there were any global resources allocated by the plugin itself (not specific model instances),
    // they could be cleaned up here. For this OpenVINO plugin, cleanup is per model instance via 'unload'.
}
