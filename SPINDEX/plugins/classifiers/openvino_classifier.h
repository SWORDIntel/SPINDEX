#ifndef OPENVINO_CLASSIFIER_H
#define OPENVINO_CLASSIFIER_H

#include "include/ml_interface.h" // For SpindexMLModelInterface
#include <openvino/c/openvino.h>   // OpenVINO C API

// Function to create and initialize an OpenVINO-backed ML model interface
int create_openvino_model_interface(SpindexMLModelInterface* model_interface, const char* model_path);

// OpenVINO specific implementation of ml_model_load_func
int openvino_load_model(const char* model_path, ModelHandle* model_handle_ptr);

// OpenVINO specific implementation of ml_model_unload_func
void openvino_unload_model(ModelHandle model_handle);

// OpenVINO specific implementation of ml_model_predict_func
int openvino_predict(ModelHandle model_handle,
                     const ProcessedDataChunk* input_data,
                     ProcessedDataChunk** output_data_ptr);

#endif // OPENVINO_CLASSIFIER_H
