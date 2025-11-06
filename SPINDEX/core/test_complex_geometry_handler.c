#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "config_manager.h" // For SpindexConfig structure
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
    SpindexConfig cfg;
    memset(&cfg, 0, sizeof(SpindexConfig)); // Zero out the config
    ComplexGeometryHandlerConfig handler_cfg;

    // Default strategy (SPLIT)
    cfg.num_settings = 0; // No relevant setting
    assert(load_complex_geometry_handler_config(&cfg, &handler_cfg) == 0);
    assert(handler_cfg.strategy == STRATEGY_SPLIT);
    assert(handler_cfg.filter_type[0] == '\0');

    // Mock "reject" strategy
    if (cfg.num_settings < MAX_SETTINGS) {
        strncpy(cfg.settings[cfg.num_settings].key, "AEGIS_COMPLEX_GEOMETRY_STRATEGY", MAX_KEY_LEN -1);
        strncpy(cfg.settings[cfg.num_settings].value, "reject", MAX_VALUE_LEN -1);
        cfg.num_settings++;
    }
    assert(load_complex_geometry_handler_config(&cfg, &handler_cfg) == 0);
    assert(handler_cfg.strategy == STRATEGY_REJECT);
    assert(handler_cfg.filter_type[0] == '\0');

    // Mock "filter_polygon" strategy
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
    output_content = read_file_to_string(output_filename);
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
    assert(status == AEGIS_PROCESSING_SUCCESS);
    output_content = read_file_to_string(output_filename);
    assert(output_content != NULL);
    root = json_loads(output_content, 0, &error);
    assert(root != NULL);
    features = json_object_get(root, "features");
    assert(json_is_array(features));
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
    char output_filename[256];
    snprintf(output_filename, sizeof(output_filename), "test_reject_output.json");
    const char* temp_dir = ".";

    const char* geojson_content_gc = "{ \"type\": \"FeatureCollection\", \"features\": [ { \"type\": \"Feature\", \"geometry\": { \"type\": \"GeometryCollection\", \"geometries\": [ { \"type\": \"Point\", \"coordinates\": [102.0, 0.5] } ] }, \"properties\": {\"prop0\": \"value0\"} } ] }";
    assert(create_temp_file(input_filename, geojson_content_gc));
    AEGIS_PROCESSING_STATUS status = process_complex_geometries(input_filename, output_filename, &cfg, temp_dir);
    assert(status == AEGIS_PROCESSING_REJECT_COMPLEX_GEOMETRY);
    remove(input_filename);
    remove(output_filename);

    const char* geojson_content_no_gc = "{ \"type\": \"FeatureCollection\", \"features\": [ { \"type\": \"Feature\", \"geometry\": { \"type\": \"Point\", \"coordinates\": [102.0, 0.5] }, \"properties\": {\"prop0\": \"value0\"} } ] }";
    assert(create_temp_file(input_filename, geojson_content_no_gc));
    status = process_complex_geometries(input_filename, output_filename, &cfg, temp_dir);
    assert(status == AEGIS_PROCESSING_SUCCESS);
    remove(input_filename);
    remove(output_filename);

    printf("test_reject_strategy: PASSED\n");
}

// Test FILTER strategy
static void test_filter_strategy() {
    printf("Running test: test_filter_strategy...\n");
    SpindexConfig global_cfg;
    ComplexGeometryHandlerConfig handler_cfg;
    const char* input_filename = "test_filter_input.json";
    char output_filename[256];
    snprintf(output_filename, sizeof(output_filename), "test_filter_output.json");
    const char* temp_dir = ".";

    const char* geojson_content_mixed = "{ \"type\": \"FeatureCollection\", \"features\": [ { \"type\": \"Feature\", \"geometry\": { \"type\": \"Point\", \"coordinates\": [102.0, 0.5] }, \"properties\": {\"propA\": \"valA\"} }, { \"type\": \"Feature\", \"geometry\": { \"type\": \"Polygon\", \"coordinates\": [[[100.0,0.0],[101.0,0.0],[101.0,1.0],[100.0,1.0],[100.0,0.0]]] }, \"properties\": {\"propB\": \"valB\"} } ] }";
    assert(create_temp_file(input_filename, geojson_content_mixed));

    // Test Case 1: Filter for "polygon"
    memset(&global_cfg, 0, sizeof(SpindexConfig));
    global_cfg.num_settings = 0;
    strncpy(global_cfg.settings[global_cfg.num_settings].key, "AEGIS_COMPLEX_GEOMETRY_STRATEGY", MAX_KEY_LEN-1);
    strncpy(global_cfg.settings[global_cfg.num_settings].value, "filter_polygon", MAX_VALUE_LEN-1);
    global_cfg.num_settings++;
    assert(load_complex_geometry_handler_config(&global_cfg, &handler_cfg) == 0);
    assert(handler_cfg.strategy == STRATEGY_FILTER && strcmp(handler_cfg.filter_type, "polygon") == 0);

    AEGIS_PROCESSING_STATUS status = process_complex_geometries(input_filename, output_filename, &handler_cfg, temp_dir);
    assert(status == AEGIS_PROCESSING_SUCCESS);
    char* output_content = read_file_to_string(output_filename);
    assert(output_content != NULL);
    json_error_t error;
    json_t* root = json_loads(output_content, 0, &error);
    assert(root != NULL);
    json_t* features = json_object_get(root, "features");
    assert(json_is_array(features) && json_array_size(features) == 1);
    json_t* feature1_geom = json_object_get(json_array_get(features, 0), "geometry");
    assert(strcmp(json_string_value(json_object_get(feature1_geom, "type")), "Polygon") == 0);
    json_decref(root);
    free(output_content);

    // Test Case 2: Filter for "point"
    memset(&global_cfg, 0, sizeof(SpindexConfig));
    global_cfg.num_settings = 0;
    strncpy(global_cfg.settings[global_cfg.num_settings].key, "AEGIS_COMPLEX_GEOMETRY_STRATEGY", MAX_KEY_LEN-1);
    strncpy(global_cfg.settings[global_cfg.num_settings].value, "filter_point", MAX_VALUE_LEN-1);
    global_cfg.num_settings++;
    assert(load_complex_geometry_handler_config(&global_cfg, &handler_cfg) == 0);
    assert(handler_cfg.strategy == STRATEGY_FILTER && strcmp(handler_cfg.filter_type, "point") == 0);

    status = process_complex_geometries(input_filename, output_filename, &handler_cfg, temp_dir);
    assert(status == AEGIS_PROCESSING_SUCCESS);
    output_content = read_file_to_string(output_filename);
    assert(output_content != NULL);
    root = json_loads(output_content, 0, &error);
    assert(root != NULL);
    features = json_object_get(root, "features");
    assert(json_is_array(features) && json_array_size(features) == 1);
    json_t* feature2_geom = json_object_get(json_array_get(features, 0), "geometry");
    assert(strcmp(json_string_value(json_object_get(feature2_geom, "type")), "Point") == 0);
    json_decref(root);
    free(output_content);

    remove(input_filename);
    remove(output_filename);
    printf("test_filter_strategy: PASSED\n");
}

static void test_syntactically_malformed_json() {
    printf("Running test: test_syntactically_malformed_json...\n");
    ComplexGeometryHandlerConfig cfg = { .strategy = STRATEGY_SPLIT };
    const char* input_filename = "test_malformed_input.json";
    char output_filename[256];
    snprintf(output_filename, sizeof(output_filename), "test_malformed_output.json");
    const char* temp_dir = ".";

    const char* malformed_json_content = "{ \"type\": \"FeatureCollection\", \"features\": [ }"; // Syntactically incorrect
    assert(create_temp_file(input_filename, malformed_json_content));
    AEGIS_PROCESSING_STATUS status = process_complex_geometries(input_filename, output_filename, &cfg, temp_dir);
    assert(status == AEGIS_PROCESSING_ERROR_FILE_IO_OR_JSON_PARSE);
    remove(input_filename);
    remove(output_filename);

    printf("test_syntactically_malformed_json: PASSED\n");
}

static void test_semantically_invalid_geojson() {
    printf("Running test: test_semantically_invalid_geojson...\n");
    ComplexGeometryHandlerConfig cfg = { .strategy = STRATEGY_SPLIT };
    const char* input_filename = "test_invalid_structure_input.json";
    char output_filename[256];
    snprintf(output_filename, sizeof(output_filename), "test_invalid_structure_output.json");

    const char* geojson_not_fc = "{ \"type\": \"NotAFeatureCollection\" }";
    assert(create_temp_file(input_filename, geojson_not_fc));
    AEGIS_PROCESSING_STATUS status = process_complex_geometries(input_filename, output_filename, &cfg, ".");
    assert(status == AEGIS_PROCESSING_ERROR_INVALID_FORMAT);
    remove(input_filename);
    remove(output_filename);

    printf("test_semantically_invalid_geojson: PASSED\n");
}

static void test_empty_feature_collection() {
    printf("Running test: test_empty_feature_collection...\n");
    ComplexGeometryHandlerConfig cfg = { .strategy = STRATEGY_SPLIT };
    const char* input_filename = "test_empty_fc_input.json";
    char output_filename[256];
    snprintf(output_filename, sizeof(output_filename), "test_empty_fc_output.json");

    const char* geojson_empty_fc = "{ \"type\": \"FeatureCollection\", \"features\": [] }";
    assert(create_temp_file(input_filename, geojson_empty_fc));
    AEGIS_PROCESSING_STATUS status = process_complex_geometries(input_filename, output_filename, &cfg, ".");
    assert(status == AEGIS_PROCESSING_SUCCESS);
    char* output_content = read_file_to_string(output_filename);
    assert(output_content != NULL);

    json_error_t error;
    json_t* root = json_loads(output_content, 0, &error);
    assert(root != NULL);
    json_t* features = json_object_get(root, "features");
    assert(json_is_array(features));
    assert(json_array_size(features) == 0);
    json_decref(root);
    free(output_content);
    remove(input_filename);
    remove(output_filename);

    printf("test_empty_feature_collection: PASSED\n");
}


int main() {
    printf("Starting tests for Complex Geometry Handler...\n");

    test_load_config();
    test_split_strategy();
    test_reject_strategy();
    test_filter_strategy();
    test_syntactically_malformed_json();
    test_semantically_invalid_geojson();
    test_empty_feature_collection();

    printf("All Complex Geometry Handler tests completed.\n");
    return 0;
}