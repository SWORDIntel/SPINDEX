#include "complex_geometry_handler.h"
#include "utils.h" // For logging or other utilities
#include <stdio.h>
#include <string.h>
#include <jansson.h> // Using Jansson as an example JSON library

// Placeholder for actual implementation of load_complex_geometry_handler_config
int load_complex_geometry_handler_config(const Config* config, ComplexGeometryHandlerConfig* handler_config) {
    if (!config || !handler_config) {
        // log_error("Null config provided to load_complex_geometry_handler_config");
        return -1;
    }
    // Example: Read strategy from global config
    // For now, hardcoding for structure, actual reading from config->aegis_complex_geometry_strategy needed
    const char* strategy_str = get_config_value(config, "AEGIS_COMPLEX_GEOMETRY_STRATEGY");

    if (strategy_str == NULL || strcmp(strategy_str, "split") == 0) {
        handler_config->strategy = STRATEGY_SPLIT;
        // log_info("Complex geometry strategy: SPLIT");
        handler_config->filter_type[0] = '\0'; // No filter type for split
    } else if (strcmp(strategy_str, "reject") == 0) {
        handler_config->strategy = STRATEGY_REJECT;
        // log_info("Complex geometry strategy: REJECT");
        handler_config->filter_type[0] = '\0'; // No filter type for reject
    } else if (strncmp(strategy_str, "filter_", 7) == 0) {
        handler_config->strategy = STRATEGY_FILTER;
        // log_info("Complex geometry strategy: FILTER (%s)", strategy_str + 7);
        strncpy(handler_config->filter_type, strategy_str + 7, MAX_TYPE_NAME_LEN_LOCAL - 1);
        handler_config->filter_type[MAX_TYPE_NAME_LEN_LOCAL - 1] = '\0';
        // log_info("Filter type set to: %s", handler_config->filter_type);
    } else {
        // log_warn("Unknown or missing AEGIS_COMPLEX_GEOMETRY_STRATEGY: '%s'. Defaulting to SPLIT.", strategy_str ? strategy_str : "NULL");
        handler_config->strategy = STRATEGY_SPLIT; // Default strategy
        handler_config->filter_type[0] = '\0';
    }
    return 0;
}


// Helper function to process a single feature (conceptual)
static AEGIS_PROCESSING_STATUS process_feature(json_t* feature, const ComplexGeometryHandlerConfig* config, json_t* output_features_array) {
    json_t *geometry = json_object_get(feature, "geometry");
    if (!json_is_object(geometry)) {
        // log_warn("Feature without a valid geometry object.");
        json_object_set_new(feature, "processing_error", json_string("Missing or invalid geometry object"));
        json_array_append_new(output_features_array, json_incref(feature)); // Keep problematic feature for now
        return AEGIS_PROCESSING_SUCCESS; // Or a more specific error code if we want to flag it
    }

    json_t *type_json = json_object_get(geometry, "type");
    if (!json_is_string(type_json)) {
        // log_warn("Geometry without a type string.");
        json_object_set_new(feature, "processing_error", json_string("Missing or invalid geometry type"));
        json_array_append_new(output_features_array, json_incref(feature));
        return AEGIS_PROCESSING_SUCCESS;
    }
    const char *geometry_type = json_string_value(type_json);

    if (strcmp(geometry_type, "GeometryCollection") == 0) {
        switch (config->strategy) {
            case STRATEGY_SPLIT:
                // log_info("Splitting GeometryCollection");
                json_t *geometries = json_object_get(geometry, "geometries");
                if (!json_is_array(geometries)) {
                    // log_error("GeometryCollection without a 'geometries' array.");
                    json_object_set_new(feature, "processing_error", json_string("Malformed GeometryCollection"));
                    json_array_append_new(output_features_array, json_incref(feature));
                    return AEGIS_PROCESSING_ERROR_IO; // Indicates structural issue
                }
                size_t num_geoms = json_array_size(geometries);
                for (size_t i = 0; i < num_geoms; ++i) {
                    json_t *sub_geometry = json_array_get(geometries, i);
                    json_t *new_feature = json_deep_copy(feature); // Copy properties
                    json_object_set(new_feature, "geometry", json_incref(sub_geometry)); // Replace geometry
                    json_object_del(new_feature, "processing_error"); // Clear previous error if any
                    json_array_append_new(output_features_array, new_feature);
                }
                // Original GeometryCollection feature is not added, only its split parts.
                break;
            case STRATEGY_FILTER:
                // log_info("Filtering GeometryCollection (discarding by default based on current design)");
                // Based on current design, filter discards GeometryCollections unless explicitly allowed.
                // This part might need refinement if filter should look inside GeometryCollections.
                // For now, we discard the entire feature.
                break; // Discard
            case STRATEGY_REJECT:
                // log_info("Rejecting dataset due to GeometryCollection");
                return AEGIS_PROCESSING_REJECT_COMPLEX_GEOMETRY;
            default: // STRATEGY_UNSET or unknown
                // log_warn("Unknown strategy for GeometryCollection, treating as SPLIT");
                // Fallthrough to SPLIT logic or handle as error. For now, let's be explicit.
                 return AEGIS_PROCESSING_ERROR_CONFIG; // Should not happen if config is loaded correctly
        }
    } else { // Simple geometry type
        switch (config->strategy) {
            case STRATEGY_SPLIT:
            case STRATEGY_REJECT: // Simple types are fine for REJECT strategy (it targets Collections)
                json_array_append_new(output_features_array, json_incref(feature));
                break;
            case STRATEGY_FILTER:
                // Allow exact match for simple type or its "Multi" version for now.
                if (strcmp(geometry_type, config->filter_type) == 0 || (strncmp(config->filter_type, "Multi", 5) != 0 && strncmp(geometry_type, "Multi", 5) == 0 && strcmp(geometry_type + 5, config->filter_type) == 0)) {
                    // log_debug("Filter: Keeping feature of type %s", geometry_type);
                    json_array_append_new(output_features_array, json_incref(feature));
                } else {
                    // log_debug("Filter: Discarding feature of type %s", geometry_type);
                }
                break;
            default: // STRATEGY_UNSET or unknown
                return AEGIS_PROCESSING_ERROR_CONFIG;
        }
    }
    return AEGIS_PROCESSING_SUCCESS;
}


AEGIS_PROCESSING_STATUS process_complex_geometries(
    const char* input_geojson_path,
    char* output_geojson_path, // Note: May need to allocate and update this if a new file is created
    const ComplexGeometryHandlerConfig* handler_config,
    const char* temp_dir) {

    if (!input_geojson_path || !output_geojson_path || !handler_config) {
        // log_error("Null arguments for process_complex_geometries");
        return AEGIS_PROCESSING_ERROR_ARGS;
    }

    json_error_t error;
    json_t *root = json_load_file(input_geojson_path, 0, &error);

    if (!root) {
        // log_error("Failed to load/parse GeoJSON from %s: %s (line %d, col %d)", input_geojson_path, error.text, error.line, error.column);
        return AEGIS_PROCESSING_ERROR_IO;
    }

    json_t *features_array;
    const char *original_geojson_type = "";

    if (json_is_object(root)) { // Could be a single Feature or a FeatureCollection
        const char* type_str = json_string_value(json_object_get(root, "type"));
        if (type_str && strcmp(type_str, "FeatureCollection") == 0) {
            original_geojson_type = "FeatureCollection";
            features_array = json_object_get(root, "features");
            if (!json_is_array(features_array)) {
                // log_error("FeatureCollection in %s does not contain a valid 'features' array.", input_geojson_path);
                json_decref(root);
                return AEGIS_PROCESSING_ERROR_INVALID_FORMAT;
            }
        } else if (type_str && strcmp(type_str, "Feature") == 0) {
            original_geojson_type = "Feature";
            features_array = json_array(); // Create a new array and add the single feature
            json_array_append_new(features_array, json_incref(root)); // root is the feature itself
        } else {
            // log_error("Input %s is not a GeoJSON Feature or FeatureCollection.", input_geojson_path);
            json_decref(root);
            return AEGIS_PROCESSING_ERROR_INVALID_FORMAT;
        }
    } else {
        // log_error("Input %s is not a valid JSON object.", input_geojson_path);
        json_decref(root);
        return AEGIS_PROCESSING_ERROR_INVALID_FORMAT;
    }

    json_t *output_features_array = json_array();
    AEGIS_PROCESSING_STATUS overall_status = AEGIS_PROCESSING_SUCCESS;
    int requires_rewrite = 0; // Flag to check if output differs from input

    for (size_t i = 0; i < json_array_size(features_array); ++i) {
        json_t *feature = json_array_get(features_array, i);
        AEGIS_PROCESSING_STATUS feature_status = process_feature(feature, handler_config, output_features_array);

        if (feature_status == AEGIS_PROCESSING_REJECT_COMPLEX_GEOMETRY) {
            overall_status = AEGIS_PROCESSING_REJECT_COMPLEX_GEOMETRY;
            // log_info("Rejection triggered by feature %zu in %s", i, input_geojson_path);
            break; // Stop processing further features for this file
        }
        if (feature_status != AEGIS_PROCESSING_SUCCESS) {
            // Potentially collect/log errors for specific features
            // For now, let overall_status reflect the most severe outcome.
            if (overall_status == AEGIS_PROCESSING_SUCCESS) overall_status = feature_status;
        }
    }

    if (overall_status == AEGIS_PROCESSING_REJECT_COMPLEX_GEOMETRY) {
        json_decref(root); // Original root
        if (strcmp(original_geojson_type, "Feature") == 0) { // If we wrapped a single feature
             json_decref(features_array); // The temporary array we created
        }
        json_decref(output_features_array);
        return AEGIS_PROCESSING_REJECT_COMPLEX_GEOMETRY;
    }

    // Determine if a rewrite is needed
    // This simple check assumes that if split/filter happens, the number of features might change,
    // or the content of features changes. A more robust check might involve deep comparison or tracking modifications.
    if (json_array_size(output_features_array) != json_array_size(features_array) || handler_config->strategy == STRATEGY_SPLIT) { // SPLIT always implies rewrite if GC found and split
        requires_rewrite = 1;
    } else {
        // For filter, if sizes are same, could be no change. For other strategies, if no GCs, could be no change.
        // For now, let's assume if sizes are same and not SPLIT, no rewrite. This might need refinement.
        // A quick check: compare pointers if no modification happened in process_feature (e.g. all features were appended as is)
        // This simple check is not perfect.
        if (json_array_size(output_features_array) > 0 && json_array_get(output_features_array,0) != json_array_get(features_array,0)) {
             requires_rewrite = 1;
        }
    }


    if (requires_rewrite) {
        // log_info("Rewriting GeoJSON to %s due to complex geometry processing.", output_geojson_path);
        json_t *output_root;
        if (strcmp(original_geojson_type, "FeatureCollection") == 0) {
            output_root = json_object();
            json_object_set_new(output_root, "type", json_string("FeatureCollection"));
            json_object_set_new(output_root, "features", output_features_array);
            // Copy other top-level properties from original root if any (e.g., "crs", "name")
            json_t *original_crs = json_object_get(root, "crs");
            if (original_crs) json_object_set(output_root, "crs", json_incref(original_crs));
        } else { // Was a single Feature, output the first (and likely only) feature from output_features_array
             if (json_array_size(output_features_array) > 0) {
                output_root = json_incref(json_array_get(output_features_array, 0));
                // If split happened on a single Feature that was a GC, output_features_array might have multiple.
                // The current design implies outputting a FeatureCollection in such a case.
                // This part needs clarification: if input is Feature (GC) and it's split, is output FeatureCollection or error?
                // For now, let's assume if input was single Feature, output is the first processed feature.
                // If it was split, this would be the first part of the split. This is likely not ideal.
                // A better approach for split single Feature (GC): output a FeatureCollection.
                if (handler_config->strategy == STRATEGY_SPLIT && json_array_size(output_features_array) > 1) {
                    // log_info("Single input Feature resulted in multiple features after split. Outputting as FeatureCollection.");
                    json_decref(output_root); // was the first feature
                    output_root = json_object();
                    json_object_set_new(output_root, "type", json_string("FeatureCollection"));
                    json_object_set_new(output_root, "features", output_features_array); // output_features_array is already incref'd
                } else {
                     // output_features_array is consumed by output_root in this case
                }
            } else { // Filtered out the single feature
                output_root = json_null(); // Or handle as an error/empty FeatureCollection
                // log_info("Single input Feature was filtered out.");
            }
        }


        if (json_dump_file(output_root, output_geojson_path, JSON_INDENT(2) | JSON_PRESERVE_ORDER) != 0) {
            // log_error("Failed to write processed GeoJSON to %s", output_geojson_path);
            overall_status = AEGIS_PROCESSING_ERROR_IO;
        }
        json_decref(output_root); // This will also decref output_features_array if it was part of output_root
    } else {
        // log_info("No changes to GeoJSON features, no rewrite needed for %s.", input_geojson_path);
        // If output_geojson_path is different from input_geojson_path, we should copy the file.
        if (strcmp(input_geojson_path, output_geojson_path) != 0) {
            // Crude copy, replace with a proper file copy utility if available
            FILE *in_file = fopen(input_geojson_path, "rb");
            FILE *out_file = fopen(output_geojson_path, "wb");
            if (in_file && out_file) {
                char buffer[4096];
                size_t bytes_read;
                while ((bytes_read = fread(buffer, 1, sizeof(buffer), in_file)) > 0) {
                    fwrite(buffer, 1, bytes_read, out_file);
                }
                fclose(in_file);
                fclose(out_file);
            } else {
                // log_error("Failed to copy %s to %s", input_geojson_path, output_geojson_path);
                overall_status = AEGIS_PROCESSING_ERROR_IO;
                if(in_file) fclose(in_file);
                if(out_file) fclose(out_file);
            }
        }
         json_decref(output_features_array); // Not used in a new root, so decref
    }

    // Cleanup
    json_decref(root); // Original root
    if (strcmp(original_geojson_type, "Feature") == 0 && features_array != output_features_array) {
        // If input was a single feature, features_array was created by us, and not used by output_root
        json_decref(features_array);
    }
    // output_features_array is either consumed by output_root or decref'd if no rewrite.

    return overall_status;
}
