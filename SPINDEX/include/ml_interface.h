#ifndef ML_INTERFACE_H
#define ML_INTERFACE_H

#include "data_types.h" // For ProcessedDataChunk

// Forward declaration for the model handle, allowing it to be an opaque pointer.
// The actual ML library will define what this points to.
struct SpindexOpaqueModelHandle;
typedef struct SpindexOpaqueModelHandle* ModelHandle;


/**
 * @brief Function pointer type for loading an ML model.
 *
 * @param model_path Path to the model file or directory.
 * @param model_handle_ptr Pointer to a ModelHandle that will be set by this function.
 * @return 0 on success, non-zero on failure.
 */
typedef int (*ml_model_load_func)(const char* model_path, ModelHandle* model_handle_ptr);

/**
 * @brief Function pointer type for unloading/freeing an ML model.
 *
 * @param model_handle Handle to the model to be unloaded.
 */
typedef void (*ml_model_unload_func)(ModelHandle model_handle);

/**
 * @brief Function pointer type for performing inference with an ML model.
 *
 * The input_data chunk contains the data to be fed to the model.
 * The function is responsible for allocating and populating output_data_ptr
 * with a new ProcessedDataChunk containing the model's predictions or features.
 * The caller is responsible for freeing the *output_data_ptr chunk.
 *
 * @param model_handle Handle to the loaded model.
 * @param input_data Pointer to a ProcessedDataChunk containing input for the model.
 * @param output_data_ptr Pointer to a ProcessedDataChunk pointer that will be set
 *                        by this function to point to the output.
 * @return 0 on success, non-zero on failure. output_data_ptr should be NULL on failure.
 */
typedef int (*ml_model_predict_func)(ModelHandle model_handle,
                                     const ProcessedDataChunk* input_data,
                                     ProcessedDataChunk** output_data_ptr);

/**
 * @brief Represents a loaded ML model and its interface functions.
 *
 * This structure allows SPINDEX components (like an ML plugin) to interact
 * with an underlying ML model in a generic way.
 */
typedef struct {
    ModelHandle handle;                     // Opaque handle to the loaded model instance
    ml_model_load_func load;                // Function to load the model
    ml_model_unload_func unload;            // Function to unload the model
    ml_model_predict_func predict;          // Function to perform inference

    // Optional: Metadata about the model or interface implementation
    char interface_name[128];             // e.g., "ONNXRuntime_Interface_v1"
    char model_description[256];          // e.g., "Text Classifier for Sentiment"
} SpindexMLModelInterface;

#endif // ML_INTERFACE_H
