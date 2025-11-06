// SPINDEX/plugins/aegis_modules/geojson_processor.c
#include "geojson_processor.h"
#include "utils.h" // Logging
#include <jansson.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h> // For mkdir
#include <unistd.h>   // For rmdir, unlink
#include <errno.h>    // For errno with mkdir

// Forward declaration
static json_t* traverse_and_validate_node(json_t* node, const GeometricValidatorConfig* gv_config, int* modified_overall, AEGIS_PROCESSING_STATUS* overall_op_status);


static json_t* process_geometry_node(json_t* geom_node, const GeometricValidatorConfig* gv_config, int* modified_overall, AEGIS_PROCESSING_STATUS* overall_op_status) {
    if (!json_is_object(geom_node)) return geom_node;

    json_t* output_geom_json = geom_node;
    // output_geom_json initially "borrows" the reference from geom_node.
    // If validate_and_repair_geometry creates a new object, output_geom_json will point to this new, ref-counted object.

    GeometryValidationResultStatus val_status = validate_and_repair_geometry(geom_node, gv_config, &output_geom_json);

    if (val_status == GEOMETRY_STATUS_REPAIRED_VALID) {
        LOG_INFO("Geometry repaired and valid.");
        if (output_geom_json != geom_node) { // New object was created by validator
            *modified_overall = 1;
            // The caller of process_geometry_node is responsible for replacing the old geom_node
            // with output_geom_json in the parent structure.
            // This function returns the new reference (output_geom_json).
            // The original geom_node reference is still held by its parent until replaced.
            return output_geom_json;
        }
        // If output_geom_json == geom_node, it means repair happened in-place (unlikely for json_t)
        // or validation decided the original was fine after trying to repair.
        // No change in reference, so return the original (borrowed) geom_node.
        return geom_node;
    } else if (val_status == GEOMETRY_STATUS_INVALID || val_status == GEOMETRY_STATUS_REPAIR_FAILED) {
        LOG_WARN("Geometry is invalid (status: %d).", val_status);
        if (*overall_op_status == AEGIS_PROCESSING_SUCCESS) *overall_op_status = AEGIS_PROCESSING_WARNING_GEOMETRY_INVALID;
        return geom_node; // Return original (borrowed) geometry
    } else if (val_status == GEOMETRY_STATUS_ERROR_INTERNAL || val_status == GEOMETRY_STATUS_ERROR_INPUT_FORMAT) {
        LOG_ERROR("Error during geometry validation (status: %d).", val_status);
         if (*overall_op_status == AEGIS_PROCESSING_SUCCESS) *overall_op_status = AEGIS_PROCESSING_ERROR_GEOMETRY_VALIDATION;
        return geom_node; // Return original (borrowed) geometry
    } else if (val_status == GEOMETRY_STATUS_VALID || val_status == GEOMETRY_STATUS_VALIDATION_DISABLED) {
        // Valid or disabled, no change.
        return geom_node; // Return original (borrowed) geometry
    }

    // Default: return original (borrowed) geometry if no specific action taken
    return geom_node;
}


static json_t* traverse_and_validate_node(json_t* node, const GeometricValidatorConfig* gv_config, int* modified_overall, AEGIS_PROCESSING_STATUS* overall_op_status) {
    if (!node) return NULL;

    if (json_is_object(node)) {
        json_t* type_val = json_object_get(node, "type");
        if (type_val && json_is_string(type_val)) {
            const char* type_str = json_string_value(type_val);

            if (strcmp(type_str, "Point") == 0 || strcmp(type_str, "LineString") == 0 ||
                strcmp(type_str, "Polygon") == 0 || strcmp(type_str, "MultiPoint") == 0 ||
                strcmp(type_str, "MultiLineString") == 0 || strcmp(type_str, "MultiPolygon") == 0 ||
                strcmp(type_str, "GeometryCollection") == 0) {
                // This node itself is a geometry object.
                // process_geometry_node will return either the original node (borrowed ref)
                // or a new node (new ref) if repaired.
                return process_geometry_node(node, gv_config, modified_overall, overall_op_status);
            }

            if (strcmp(type_str, "Feature") == 0) {
                json_t* geometry = json_object_get(node, "geometry");
                if (geometry && json_is_object(geometry)) { // Geometry can be null or not an object
                    json_t* processed_geometry = traverse_and_validate_node(geometry, gv_config, modified_overall, overall_op_status);
                    if (processed_geometry != geometry) { // If geometry was replaced by a new object
                        json_object_set_new(node, "geometry", processed_geometry); // Consumes processed_geometry's new reference
                        // The old 'geometry' is automatically decref'd by json_object_set_new
                        *modified_overall = 1; // Mark that the overall structure changed
                    }
                }
                return node; // Return the (potentially modified) feature (borrowed ref from caller)
            }

            if (strcmp(type_str, "FeatureCollection") == 0) {
                json_t* features = json_object_get(node, "features");
                if (json_is_array(features)) {
                    size_t i;
                    json_t *value;
                    json_array_foreach(features, i, value) {
                        json_t* processed_feature = traverse_and_validate_node(value, gv_config, modified_overall, overall_op_status);
                        if (processed_feature != value) { // If feature (or its geometry) was replaced
                             json_array_set_new(features, i, processed_feature); // Consumes processed_feature's new reference
                             // Old 'value' at this index is decref'd by json_array_set_new
                             *modified_overall = 1; // Ensure this is set if any feature is replaced
                        }
                    }
                }
                return node; // Return the (potentially modified) feature collection (borrowed ref)
            }
        }
         return node; // Not a GeoJSON type we recurse into, or not a type at all (borrowed ref)
    } else if (json_is_array(node)) {
        size_t i;
        json_t *value;
        json_array_foreach(node, i, value) {
            json_t* processed_element = traverse_and_validate_node(value, gv_config, modified_overall, overall_op_status);
            if (processed_element != value) { // Element was replaced
                json_array_set_new(node, i, processed_element); // Consumes processed_element's new reference
                *modified_overall = 1;
            }
        }
        return node; // Return the (potentially modified) array (borrowed ref)
    }
    return node; // Not an object or array (borrowed ref)
}


AEGIS_PROCESSING_STATUS process_geojson_chunk(
    ProcessedDataChunk** chunk_ptr,
    const SpindexConfig* main_app_config,
    const ComplexGeometryHandlerConfig* cg_config,
    const GeometricValidatorConfig* gv_config,
    const char* temp_dir_base_path) {

    if (!chunk_ptr || !(*chunk_ptr) || !(*chunk_ptr)->data || !main_app_config || !cg_config || !gv_config || !temp_dir_base_path) {
        LOG_ERROR("GeoJSONProc: Null arguments.");
        return AEGIS_PROCESSING_ERROR_ARGS;
    }

    ProcessedDataChunk* current_chunk = *chunk_ptr;
    char temp_input_path[MAX_PATH_LEN];
    char temp_output_path_cg[MAX_PATH_LEN];

    char instance_temp_dir[MAX_PATH_LEN];
    // Using a simpler temp file name generation for now. For multi-threaded, ensure uniqueness.
    // Using source_id (filename) could help, but needs sanitization.
    // For now, just a generic name inside the base temp path.
    snprintf(instance_temp_dir, MAX_PATH_LEN, "%s", temp_dir_base_path); // Use the base path directly or a sub-folder
    // Example: mkdir(instance_temp_dir, 0700); // Ensure base temp dir exists (caller's responsibility for now)


    // Create unique filenames using part of the source_id if available, or just random/pid based
    // For simplicity, using fixed names within the general temp_dir_base_path.
    // This means concurrent processing of different files into the same temp_dir_base_path could clash.
    // A robust solution would use mkstemp or ensure unique names.
    snprintf(temp_input_path, MAX_PATH_LEN, "%s/spindex_geojson_input_%lu.json", instance_temp_dir, (unsigned long)pthread_self());
    snprintf(temp_output_path_cg, MAX_PATH_LEN, "%s/spindex_geojson_output_cg_%lu.json", instance_temp_dir, (unsigned long)pthread_self());


    FILE* fp = fopen(temp_input_path, "wb");
    if (!fp) {
        LOG_ERROR("GeoJSONProc: Failed to open temp input file: %s (errno %d)", temp_input_path, errno);
        return AEGIS_PROCESSING_ERROR_IO;
    }
    fwrite(current_chunk->data, 1, current_chunk->length, fp);
    fclose(fp);

    AEGIS_PROCESSING_STATUS overall_status = AEGIS_PROCESSING_SUCCESS;

    // 1. Process with Complex Geometry Handler
    // output_path_cg will be populated by process_complex_geometries
    // The `process_complex_geometries` function is expected to handle the case where output_path_cg
    // might be the same as temp_input_path if no changes are made, or write to a new file.
    // For this integration, we assume it writes to temp_output_path_cg if changes occur,
    // or indicates no change (e.g. by not creating temp_output_path_cg or making it identical to input).

    // Make a mutable copy of temp_output_path_cg for process_complex_geometries
    char actual_output_path_cg[MAX_PATH_LEN];
    strncpy(actual_output_path_cg, temp_output_path_cg, MAX_PATH_LEN -1);
    actual_output_path_cg[MAX_PATH_LEN-1] = '\0';

    AEGIS_PROCESSING_STATUS cg_status = process_complex_geometries(temp_input_path, actual_output_path_cg, cg_config, instance_temp_dir);

    const char* file_to_validate = temp_input_path;
    int complex_geom_produced_new_file = 0;

    if (cg_status == AEGIS_PROCESSING_SUCCESS) {
        struct stat st_output_cg;
        // Check if the output file from complex geometry handling is different and exists
        if (strcmp(temp_input_path, actual_output_path_cg) != 0 && stat(actual_output_path_cg, &st_output_cg) == 0 && st_output_cg.st_size > 0) {
            file_to_validate = actual_output_path_cg;
            complex_geom_produced_new_file = 1;
            LOG_DEBUG("GeoJSONProc: Complex geometry handler produced new output: %s", file_to_validate);
        } else {
             // No new file, or output is same as input, or output is empty. Validate original input.
            file_to_validate = temp_input_path;
            LOG_DEBUG("GeoJSONProc: Complex geometry handler did not produce a new distinct output file. Validating: %s", file_to_validate);
        }
    } else if (cg_status == AEGIS_PROCESSING_REJECT_COMPLEX_GEOMETRY) {
        LOG_WARN("GeoJSONProc: Complex geometry handler rejected the file: %s", current_chunk->source_id ? current_chunk->source_id : "Unknown source");
        unlink(temp_input_path); // Clean up input temp file
        overall_status = AEGIS_PROCESSING_REJECT_COMPLEX_GEOMETRY;
        goto cleanup_and_exit;
    } else {
        LOG_ERROR("GeoJSONProc: Complex geometry handler failed for %s with status %d.", current_chunk->source_id ? current_chunk->source_id : "Unknown source", cg_status);
        overall_status = cg_status;
        goto cleanup_and_exit;
    }

    // 2. Read (potentially modified by CGH) GeoJSON file for validation
    json_error_t error;
    json_t* root_json = json_load_file(file_to_validate, 0, &error);
    if (!root_json) {
        LOG_ERROR("GeoJSONProc: Failed to load/parse GeoJSON from %s: %s (line %d, col %d)", file_to_validate, error.text, error.line, error.column);
        overall_status = AEGIS_PROCESSING_ERROR_INVALID_FORMAT;
        goto cleanup_and_exit;
    }

    // 3. Validate and Repair Geometries
    int modified_by_validator = 0;
    // overall_status can already be warning/error from previous steps, don't overwrite SUCCESS if it's already more severe
    AEGIS_PROCESSING_STATUS validation_op_status_tracker = AEGIS_PROCESSING_SUCCESS;

    json_t* processed_root_json = traverse_and_validate_node(root_json, gv_config, &modified_by_validator, &validation_op_status_tracker);

    if (processed_root_json != root_json && processed_root_json != NULL) {
        json_decref(root_json);
        root_json = processed_root_json;
        // modified_by_validator should already be set by traverse_and_validate_node if root itself changed
    } else if (processed_root_json == NULL) {
         LOG_ERROR("GeoJSONProc: Geometry validation traversal failed catastrophically.");
         json_decref(root_json);
         overall_status = AEGIS_PROCESSING_ERROR_GEOMETRY_VALIDATION;
         goto cleanup_and_exit;
    }
    // Update overall status if validation step had issues (but don't override a more severe prior status)
    if (overall_status == AEGIS_PROCESSING_SUCCESS) overall_status = validation_op_status_tracker;


    // 4. Update chunk if modified by CGH or validator
    if (complex_geom_produced_new_file || modified_by_validator) {
        char* new_json_str = json_dumps(root_json, JSON_INDENT(2) | JSON_PRESERVE_ORDER);
        if (new_json_str) {
            LOG_INFO("GeoJSONProc: GeoJSON content modified. Updating chunk for %s.", current_chunk->source_id ? current_chunk->source_id : "Unknown source");
            free(current_chunk->data);
            current_chunk->data = (unsigned char*)new_json_str;
            current_chunk->length = strlen(new_json_str);
            // Assuming chunk data_type remains CHUNK_TYPE_UTF8_TEXT or similar
        } else {
            LOG_ERROR("GeoJSONProc: Failed to serialize processed GeoJSON to string for %s.", current_chunk->source_id ? current_chunk->source_id : "Unknown source");
            if (overall_status == AEGIS_PROCESSING_SUCCESS) overall_status = AEGIS_PROCESSING_ERROR_SERIALIZATION;
        }
    }

    json_decref(root_json);

cleanup_and_exit:
    unlink(temp_input_path);
    // Only unlink temp_output_path_cg if it was different from input and successfully processed or used
    if (complex_geom_produced_new_file || (strcmp(file_to_validate, temp_output_path_cg) == 0 && strcmp(temp_input_path, temp_output_path_cg) !=0) ) {
        unlink(temp_output_path_cg);
    }
    // rmdir(instance_temp_dir); // Consider if instance_temp_dir is needed or if base_path is enough

    return overall_status;
}
