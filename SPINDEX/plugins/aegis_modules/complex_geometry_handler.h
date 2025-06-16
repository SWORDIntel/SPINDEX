#ifndef COMPLEX_GEOMETRY_HANDLER_H
#define COMPLEX_GEOMETRY_HANDLER_H

#include "config_manager.h" // For accessing configuration
#include "data_types.h"     // For AEGIS_PROCESSING_STATUS and potentially GeoJSON structures

// Forward declaration for cJSON or a similar library if used directly in the header
// For now, we'll assume operations are on file paths or abstract structures
// typedef struct cJSON cJSON;

typedef enum {
    STRATEGY_UNSET,
    STRATEGY_SPLIT,
    STRATEGY_FILTER,
    STRATEGY_REJECT
} complex_geometry_strategy_t;

// Assuming MAX_TYPE_NAME_LEN is defined in data_types.h or similar, e.g., 64
#define MAX_TYPE_NAME_LEN_LOCAL 64 // Local definition if not available elsewhere

typedef struct {
    complex_geometry_strategy_t strategy;
    char filter_type[MAX_TYPE_NAME_LEN_LOCAL]; // e.g., "Polygon", "Point"
} ComplexGeometryHandlerConfig;

/**
 * @brief Processes a GeoJSON file to handle complex geometries based on the configured strategy.
 *
 * @param input_geojson_path Path to the input GeoJSON file.
 * @param output_geojson_path Path where the processed GeoJSON file will be written.
 *                           This can be the same as input_geojson_path for in-place modification
 *                           or a new path if significant changes (like splitting) occur.
 * @param handler_config Configuration for the complex geometry handler.
 * @param temp_dir A temporary directory for intermediate files if needed.
 * @return AEGIS_PROCESSING_STATUS indicating success, failure, or rejection.
 */
AEGIS_PROCESSING_STATUS process_complex_geometries(
    const char* input_geojson_path,
    char* output_geojson_path, // May be modified to point to a new file
    const ComplexGeometryHandlerConfig* handler_config,
    const char* temp_dir
);

/**
 * @brief Loads the complex geometry handling configuration.
 *
 * @param config Pointer to the global Config struct from config_manager.
 * @param handler_config Pointer to ComplexGeometryHandlerConfig to be populated.
 * @return Returns 0 on success, -1 on failure.
 */
int load_complex_geometry_handler_config(const Config* config, ComplexGeometryHandlerConfig* handler_config);

#endif // COMPLEX_GEOMETRY_HANDLER_H
