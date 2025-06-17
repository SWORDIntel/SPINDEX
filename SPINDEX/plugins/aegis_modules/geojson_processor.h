#ifndef GEOJSON_PROCESSOR_H
#define GEOJSON_PROCESSOR_H

#include "data_types.h" // For ProcessedDataChunk, AEGIS_PROCESSING_STATUS
#include "config_manager.h" // For Config
#include "complex_geometry_handler.h" // For ComplexGeometryHandlerConfig
#include "geometric_validator.h"  // For GeometricValidatorConfig
#include <jansson.h>

// Main processing function
AEGIS_PROCESSING_STATUS process_geojson_chunk(
    ProcessedDataChunk** chunk_ptr, // Pointer to a pointer to allow modification of the chunk itself
    const SpindexConfig* main_app_config, // For context if sub-modules need it directly
    const ComplexGeometryHandlerConfig* cg_config,
    const GeometricValidatorConfig* gv_config,
    const char* temp_dir_path // For intermediate files used by complex_geometry_handler
);

#endif // GEOJSON_PROCESSOR_H
