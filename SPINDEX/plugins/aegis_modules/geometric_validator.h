#ifndef GEOMETRIC_VALIDATOR_H
#define GEOMETRIC_VALIDATOR_H

#include "config_manager.h" // For Config struct
#include "data_types.h"     // For AEGIS_PROCESSING_STATUS
#include <jansson.h>        // For json_t input/output

typedef enum {
    GEOMETRY_STATUS_VALID,
    GEOMETRY_STATUS_INVALID,
    GEOMETRY_STATUS_REPAIRED_VALID,
    GEOMETRY_STATUS_REPAIR_FAILED,
    GEOMETRY_STATUS_VALIDATION_DISABLED,
    GEOMETRY_STATUS_ERROR_INTERNAL,      // Internal error in validator (e.g., GEOS context, mem alloc)
    GEOMETRY_STATUS_ERROR_INPUT_FORMAT   // Malformed GeoJSON input
} GeometryValidationResultStatus;

typedef struct {
    int enabled;
    int enable_repair;
    // char on_error_action[32]; // Future: "quarantine", "log_continue"
} GeometricValidatorConfig;

/**
 * @brief Initializes the geometric validator module (e.g., GEOS context).
 * Must be called once before any validation functions are used.
 * @return 0 on success, -1 on failure.
 */
int geometric_validator_init();

/**
 * @brief Shuts down the geometric validator module (e.g., GEOS context).
 * Must be called once when the application is shutting down.
 */
void geometric_validator_shutdown();

/**
 * @brief Loads geometric validator configuration from the global config.
 *
 * @param global_config Pointer to the global Config struct from config_manager.
 * @param val_config Pointer to GeometricValidatorConfig to be populated.
 * @return 0 on success, -1 on failure (e.g., required config keys missing).
 */
int load_geometric_validator_config(const Config* global_config, GeometricValidatorConfig* val_config);

/**
 * @brief Validates and optionally repairs a single GeoJSON geometry represented by a json_t object.
 *
 * The input json_t object (input_geometry_json) is not modified unless it is the same object
 * returned via output_geometry_json_ptr (i.e. no repair or repair failed but same object returned).
 * If a repair occurs and is successful, a *new* json_t object is created for the repaired geometry,
 * and output_geometry_json_ptr will point to this new object. The caller is responsible for
 * managing the reference count of the json_t object pointed to by output_geometry_json_ptr
 * (e.g., by calling json_decref when it's no longer needed).
 * The input_geometry_json's reference count is not changed by this function.
 *
 * @param input_geometry_json A json_t object representing a single GeoJSON geometry.
 *                            This function does not take ownership (does not decref).
 * @param val_config Configuration for the validator.
 * @param output_geometry_json_ptr Pointer to a json_t* which will be set to point to the
 *                                 resulting geometry. If repair occurs and is successful, this
 *                                 will be a new json_t object (caller must decref). Otherwise,
 *                                 it will typically point to input_geometry_json (caller should not
 *                                 decref input_geometry_json based on this pointer unless it's the
 *                                 sole owner).
 * @return GeometryValidationResultStatus indicating the outcome.
 */
GeometryValidationResultStatus validate_and_repair_geometry(
    json_t* input_geometry_json,
    const GeometricValidatorConfig* val_config,
    json_t** output_geometry_json_ptr
);

#endif // GEOMETRIC_VALIDATOR_H
