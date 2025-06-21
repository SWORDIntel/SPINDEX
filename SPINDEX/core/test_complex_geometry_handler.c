#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "config_manager.h" // For Config structure
#include "data_types.h"     // For AEGIS_PROCESSING_STATUS
#include "complex_geometry_handler.h" // The module to test
#include "utils.h"          // For any utility functions if needed (e.g., logging for debugging)
#include <jansson.h>        // For validating output JSON

// Helper function to create a temporary file with given content
static int create_temp_file(const char* filename, const char* content) {
    FILE* fp = fopen(filename, "w");
    if (!fp) {
        perror("Failed to create temp file");
        return 0;
    }
    fputs(content, fp);
    fclose(fp);
    return 1;
}

// Helper function to read a file into a string
static char* read_file_to_string(const char* filename) {
    FILE* fp = fopen(filename, "r");
    if (!fp) {
        //perror("Failed to read file");
        return NULL;
    }
    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    char* buffer = (char*)malloc(size + 1);
    if (!buffer) {
        fclose(fp);
        fprintf(stderr, "Failed to allocate memory for file content\n");
        return NULL;
    }
    fread(buffer, 1, size, fp);
    buffer[size] = '\0';
    fclose(fp);
    return buffer;
}

// Test loading configuration
static void test_load_config() {
    printf("Running test: test_load_config...\n");
    Config cfg;
    memset(&cfg, 0, sizeof(Config)); // Zero out the config
    ComplexGeometryHandlerConfig handler_cfg;

    // Mock a config setting - in reality, config_manager would populate this
    // For now, we'll test the default if AEGIS_COMPLEX_GEOMETRY_STRATEGY is not set in mock_config
    // and then mock setting it.
    // To properly test, we would need to use the actual config_manager functions to set test values.
    // We'll simulate by directly manipulating the cfg.settings for this test.
    // MAX_SETTINGS, MAX_KEY_LEN, MAX_VALUE_LEN are assumed from config_manager.h

    // This is a simplified version. Assume get_config_value is part of config_manager.
    // Let's assume a function `add_config_value(Config* Cfg, const char* key, const char* value)` exists for testing.
    // If not, this test is more conceptual for `load_complex_geometry_handler_config`.

    // Default strategy (SPLIT)
    cfg.num_settings = 0; // No relevant setting
    assert(load_complex_geometry_handler_config(&cfg, &handler_cfg) == 0);
    assert(handler_cfg.strategy == STRATEGY_SPLIT);
    assert(handler_cfg.filter_type[0] == '\0');

    // Mock "reject" strategy
    // Simulate adding a config value (this is a simplification)
    if (cfg.num_settings < MAX_SETTINGS) {
        strncpy(cfg.settings[cfg.num_settings].key, "AEGIS_COMPLEX_GEOMETRY_STRATEGY", MAX_KEY_LEN -1);
        strncpy(cfg.settings[cfg.num_settings].value, "reject", MAX_VALUE_LEN -1);
        cfg.num_settings++;
    }
    assert(load_complex_geometry_handler_config(&cfg, &handler_cfg) == 0);
    assert(handler_cfg.strategy == STRATEGY_REJECT);
    assert(handler_cfg.filter_type[0] == '\0');

    // Mock "filter_polygon" strategy
    // To reset for this specific value, we'll just overwrite the first entry.
    // A real config manager would handle this more cleanly.
    cfg.num_settings = 0; // Start fresh for this test value
    strncpy(cfg.settings[cfg.num_settings].key, "AEGIS_COMPLEX_GEOMETRY_STRATEGY", MAX_KEY_LEN-1);
    cfg.settings[cfg.num_settings].key[MAX_KEY_LEN-1] = '\0';
    strncpy(cfg.settings[cfg.num_settings].value, "filter_polygon", MAX_VALUE_LEN-1);
    cfg.settings[cfg.num_settings].value[MAX_VALUE_LEN-1] = '\0';
    cfg.num_settings++;

    assert(load_complex_geometry_handler_config(&cfg, &handler_cfg) == 0);
    assert(handler_cfg.strategy == STRATEGY_FILTER);
    assert(strcmp(handler_cfg.filter_type, "polygon") == 0);

    printf("test_load_config: PASSED\n");
}

// Test SPLIT strategy
static void test_split_strategy() {
    printf("Running test: test_split_strategy...\n");
    ComplexGeometryHandlerConfig cfg = { .strategy = STRATEGY_SPLIT };
    const char* input_filename = "test_split_input.json";
    char output_filename[256];
    snprintf(output_filename, sizeof(output_filename), "test_split_output.json");
    const char* temp_dir = "."; // Current directory for temp files

    const char* geojson_content_gc = "{ \"type\": \"FeatureCollection\", \"features\": [ { \"type\": \"Feature\", \"geometry\": { \"type\": \"GeometryCollection\", \"geometries\": [ { \"type\": \"Point\", \"coordinates\": [102.0, 0.5] }, { \"type\": \"LineString\", \"coordinates\": [ [102.0, 0.0], [103.0, 1.0] ] } ] }, \"properties\": {\"prop0\": \"value0\"} } ] }";
    assert(create_temp_file(input_filename, geojson_content_gc));

    AEGIS_PROCESSING_STATUS status = process_complex_geometries(input_filename, output_filename, &cfg, temp_dir);
    assert(status == AEGIS_PROCESSING_SUCCESS);

    char* output_content = read_file_to_string(output_filename);
    assert(output_content != NULL);
    // printf("Split output: %s\n", output_content); // For debugging

    json_error_t error;
    json_t* root = json_loads(output_content, 0, &error);
    assert(root != NULL);
    assert(json_is_object(root));
    assert(strcmp(json_string_value(json_object_get(root, "type")), "FeatureCollection") == 0);
    json_t* features = json_object_get(root, "features");
    assert(json_is_array(features));
    assert(json_array_size(features) == 2); // Should be split into 2 features

    json_t* feature1_geom = json_object_get(json_array_get(features, 0), "geometry");
    assert(strcmp(json_string_value(json_object_get(feature1_geom, "type")), "Point") == 0);
    json_t* feature2_geom = json_object_get(json_array_get(features, 1), "geometry");
    assert(strcmp(json_string_value(json_object_get(feature2_geom, "type")), "LineString") == 0);

    // Check properties are copied
    json_t* feature1_props = json_object_get(json_array_get(features, 0), "properties");
    assert(strcmp(json_string_value(json_object_get(feature1_props, "prop0")), "value0") == 0);


    json_decref(root);
    free(output_content);
    remove(input_filename);
    remove(output_filename);

    // Test with no GeometryCollection
    const char* geojson_content_no_gc = "{ \"type\": \"FeatureCollection\", \"features\": [ { \"type\": \"Feature\", \"geometry\": { \"type\": \"Point\", \"coordinates\": [102.0, 0.5] }, \"properties\": {\"prop0\": \"value0\"} } ] }";
    assert(create_temp_file(input_filename, geojson_content_no_gc));
    status = process_complex_geometries(input_filename, output_filename, &cfg, temp_dir);
    assert(status == AEGIS_PROCESSING_SUCCESS);
    output_content = read_file_to_string(output_filename); // output_filename might be same as input if no change
    assert(output_content != NULL);
    root = json_loads(output_content, 0, &error);
    assert(root != NULL);
    features = json_object_get(root, "features");
    assert(json_array_size(features) == 1);
    json_decref(root);
    free(output_content);
    remove(input_filename);
    remove(output_filename);

    // Test with an empty GeometryCollection
    const char* geojson_content_empty_gc = "{ \"type\": \"FeatureCollection\", \"features\": [ { \"type\": \"Feature\", \"geometry\": { \"type\": \"GeometryCollection\", \"geometries\": [] }, \"properties\": {\"prop0\": \"value0\"} } ] }";
    assert(create_temp_file(input_filename, geojson_content_empty_gc));
    status = process_complex_geometries(input_filename, output_filename, &cfg, temp_dir);
    assert(status == AEGIS_PROCESSING_SUCCESS); // Should succeed, but result in zero features from this one
    output_content = read_file_to_string(output_filename);
    assert(output_content != NULL);
    root = json_loads(output_content, 0, &error);
    assert(root != NULL);
    features = json_object_get(root, "features");
    assert(json_is_array(features));
    // Depending on implementation: could be 0 features if the GC was the only one,
    // or the original feature is removed and nothing replaces it.
    // Current code in complex_geometry_handler.c iterates geometries in GC. If empty, it won't add any new features
    // and the original feature with GC is removed by `json_array_remove(features, i)`.
    // So, if this was the only feature, features array should be empty.
    assert(json_array_size(features) == 0);
    json_decref(root);
    free(output_content);
    remove(input_filename);
    remove(output_filename);


    printf("test_split_strategy: PASSED\n");
}

// Test REJECT strategy
static void test_reject_strategy() {
    printf("Running test: test_reject_strategy...\n");
    ComplexGeometryHandlerConfig cfg = { .strategy = STRATEGY_REJECT };
    const char* input_filename = "test_reject_input.json";
    char output_filename[256]; // Not strictly needed for reject as file isn't written
    snprintf(output_filename, sizeof(output_filename), "test_reject_output.json");
    const char* temp_dir = ".";

    // Input with GeometryCollection should be rejected
    const char* geojson_content_gc = "{ \"type\": \"FeatureCollection\", \"features\": [ { \"type\": \"Feature\", \"geometry\": { \"type\": \"GeometryCollection\", \"geometries\": [ { \"type\": \"Point\", \"coordinates\": [102.0, 0.5] } ] }, \"properties\": {\"prop0\": \"value0\"} } ] }";
    assert(create_temp_file(input_filename, geojson_content_gc));
    AEGIS_PROCESSING_STATUS status = process_complex_geometries(input_filename, output_filename, &cfg, temp_dir);
    assert(status == AEGIS_PROCESSING_REJECT_COMPLEX_GEOMETRY);
    remove(input_filename);
    // remove(output_filename); // Output file might not be created or might be empty

    // Input without GeometryCollection should succeed
    const char* geojson_content_no_gc = "{ \"type\": \"FeatureCollection\", \"features\": [ { \"type\": \"Feature\", \"geometry\": { \"type\": \"Point\", \"coordinates\": [102.0, 0.5] }, \"properties\": {\"prop0\": \"value0\"} } ] }";
    assert(create_temp_file(input_filename, geojson_content_no_gc));
    status = process_complex_geometries(input_filename, output_filename, &cfg, temp_dir);
    assert(status == AEGIS_PROCESSING_SUCCESS);
    remove(input_filename);
    // remove(output_filename); // Output file might be a copy of input


    printf("test_reject_strategy: PASSED\n");
}

// Test FILTER strategy (basic placeholder)
static void test_filter_strategy() {
    printf("Running test: test_filter_strategy...\n");
    Config global_cfg; // Mock global config
    ComplexGeometryHandlerConfig handler_cfg;

    const char* input_filename = "test_filter_input.json";
    char output_filename[256];
    snprintf(output_filename, sizeof(output_filename), "test_filter_output.json");
    const char* temp_dir = ".";

    // Test Case 1: Filter for "polygon"
    memset(&global_cfg, 0, sizeof(Config));
    global_cfg.num_settings = 0;
    strncpy(global_cfg.settings[global_cfg.num_settings].key, "AEGIS_COMPLEX_GEOMETRY_STRATEGY", MAX_KEY_LEN-1);
    strncpy(global_cfg.settings[global_cfg.num_settings].value, "filter_polygon", MAX_VALUE_LEN-1);
    global_cfg.num_settings++;

    assert(load_complex_geometry_handler_config(&global_cfg, &handler_cfg) == 0);
    assert(handler_cfg.strategy == STRATEGY_FILTER);
    assert(strcmp(handler_cfg.filter_type, "polygon") == 0);

    const char* geojson_content_mixed = "{ \"type\": \"FeatureCollection\", \"features\": [ { \"type\": \"Feature\", \"geometry\": { \"type\": \"Point\", \"coordinates\": [102.0, 0.5] }, \"properties\": {\"propA\": \"valA\"} }, { \"type\": \"Feature\", \"geometry\": { \"type\": \"Polygon\", \"coordinates\": [[[100.0,0.0],[101.0,0.0],[101.0,1.0],[100.0,1.0],[100.0,0.0]]] }, \"properties\": {\"propB\": \"valB\"} } ] }";
    assert(create_temp_file(input_filename, geojson_content_mixed));
    AEGIS_PROCESSING_STATUS status = process_complex_geometries(input_filename, output_filename, &handler_cfg, temp_dir);
    assert(status == AEGIS_PROCESSING_SUCCESS);

    char* output_content = read_file_to_string(output_filename);
    assert(output_content != NULL);
    // printf("Filter output: %s\n", output_content); // For debugging

    json_error_t error;
    json_t* root = json_loads(output_content, 0, &error);
    assert(root != NULL);
    json_t* features = json_object_get(root, "features");
    assert(json_is_array(features));
    assert(json_array_size(features) == 1); // Only Polygon should remain

    json_t* feature1_geom = json_object_get(json_array_get(features, 0), "geometry");
    assert(strcmp(json_string_value(json_object_get(feature1_geom, "type")), "Polygon") == 0);

    json_decref(root);
    free(output_content);
    remove(input_filename);
    remove(output_filename);

    // Test Case 2: Filter for "point"
    memset(&global_cfg, 0, sizeof(Config));
    global_cfg.num_settings = 0;
    strncpy(global_cfg.settings[global_cfg.num_settings].key, "AEGIS_COMPLEX_GEOMETRY_STRATEGY", MAX_KEY_LEN-1);
    strncpy(global_cfg.settings[global_cfg.num_settings].value, "filter_point", MAX_VALUE_LEN-1);
    global_cfg.num_settings++;

    assert(load_complex_geometry_handler_config(&global_cfg, &handler_cfg) == 0);
    assert(handler_cfg.strategy == STRATEGY_FILTER);
    assert(strcmp(handler_cfg.filter_type, "point") == 0);

    // Use the same mixed content file
    assert(create_temp_file(input_filename, geojson_content_mixed));
    status = process_complex_geometries(input_filename, output_filename, &handler_cfg, temp_dir);
    assert(status == AEGIS_PROCESSING_SUCCESS);

    output_content = read_file_to_string(output_filename);
    assert(output_content != NULL);

    root = json_loads(output_content, 0, &error);
    assert(root != NULL);
    features = json_object_get(root, "features");
    assert(json_is_array(features));
    assert(json_array_size(features) == 1); // Only Point should remain

    json_t* feature2_geom = json_object_get(json_array_get(features, 0), "geometry");
    assert(strcmp(json_string_value(json_object_get(feature2_geom, "type")), "Point") == 0);

    json_decref(root);
    free(output_content);
    remove(input_filename);
    remove(output_filename);

    printf("test_filter_strategy: PASSED\n");
}

static void test_malformed_json() {
    printf("Running test: test_malformed_json...\n");
    ComplexGeometryHandlerConfig cfg = { .strategy = STRATEGY_SPLIT }; // Strategy doesn't matter much here
    const char* input_filename = "test_malformed_input.json";
    char output_filename[256];
    snprintf(output_filename, sizeof(output_filename), "test_malformed_output.json");
    const char* temp_dir = ".";

    // Malformed JSON content (e.g., missing closing brace)
    const char* malformed_json_content = "{ \"type\": \"FeatureCollection\", \"features\": [ { \"type\": \"Feature\", \"geometry\": { \"type\": \"Point\", \"coordinates\": [102.0, 0.5] } "; // Missing "}]}"
    assert(create_temp_file(input_filename, malformed_json_content));

    AEGIS_PROCESSING_STATUS status = process_complex_geometries(input_filename, output_filename, &cfg, temp_dir);
    // Depending on where the JSON parsing happens (inside process_complex_geometries or before),
    // the status code might vary. Assuming it's handled inside.
    // A common error code for this scenario would be related to file I/O or parse error.
    // Based on current complex_geometry_handler.c, it uses load_json_from_file which returns NULL on error.
    // The function then returns AEGIS_PROCESSING_ERROR_FILE_IO_OR_JSON_PARSE.
    assert(status == AEGIS_PROCESSING_ERROR_FILE_IO_OR_JSON_PARSE);

    remove(input_filename);
    // Output file might not be created or could be empty/incomplete
    remove(output_filename); // Attempt to remove, ignore error if it doesn't exist

    printf("test_malformed_json: PASSED\n");
}

int main() {
    // Initialize anything global if necessary (e.g. logger)
    // init_logger("test_cgh.log", LOG_LEVEL_DEBUG, 0); // Example

    printf("Starting tests for Complex Geometry Handler...\n");

    test_load_config();
    test_split_strategy();
    test_reject_strategy();
    test_filter_strategy();
    test_malformed_json();
    // TODO: Add more tests (edge cases, malformed JSON, etc.)

    printf("All Complex Geometry Handler tests completed.\n");
    return 0;
}
