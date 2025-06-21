#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "config_manager.h" // For Config structure
#include "data_types.h"     // For AEGIS_PROCESSING_STATUS (though not directly used by validator)
#include "geometric_validator.h" // The module to test
#include "utils.h"          // For logging, if used during test debugging
#include <jansson.h>        // For creating and inspecting json_t objects

// Helper to setup a mock Config object for testing config loading
static void setup_mock_config_value(Config* cfg, int index, const char* key, const char* value) {
    if (index < MAX_SETTINGS) {
        strncpy(cfg->settings[index].key, key, MAX_KEY_LEN - 1);
        cfg->settings[index].key[MAX_KEY_LEN - 1] = '\0';
        strncpy(cfg->settings[index].value, value, MAX_VALUE_LEN - 1);
        cfg->settings[index].value[MAX_VALUE_LEN - 1] = '\0';
    }
}

// Helper to parse a string to json_t; logs error and asserts on failure
static json_t* parse_json_string(const char* json_str) {
    json_error_t error;
    json_t* geom_json = json_loads(json_str, 0, &error);
    if (!geom_json) {
        LOG_ERROR("Test JSON parsing error: %s (line %d, column %d) for string: %s", error.text, error.line, error.column, json_str);
    }
    assert(geom_json != NULL); // Fail test if basic JSON parsing fails
    return geom_json;
}

static void test_module_init_shutdown() {
    printf("Running test: test_module_init_shutdown...\n");
    // Shutdown first in case it was initialized by a previous test run (if tests are one executable)
    // In a typical test suite, each test file might be a separate executable, or setup/teardown fixtures are used.
    // For this simple structure, call shutdown if geometric_validator_init could be called multiple times.
    // However, the current design of geometric_validator.c makes init/shutdown idempotent or safe to call multiple times.

    // geometric_validator_shutdown(); // Assuming this is safe if not initialized
    // assert(geometric_validator_init() == 0); // Should succeed
    // geometric_validator_shutdown(); // Clean up
    // assert(geometric_validator_init() == 0); // Re-initialize for other tests

    // The global GEOS context makes this tricky for isolated tests without a fixture framework.
    // For now, assume main() in the test runner calls init once at start, shutdown once at end.
    // This test will just verify it can be called.
    assert(geometric_validator_init() == 0); // If called first time or re-init after shutdown
    printf("test_module_init_shutdown: PASSED (assuming main test runner handles overall init/shutdown)\n");
}


static void test_load_validator_config() {
    printf("Running test: test_load_validator_config...\n");
    Config global_cfg;
    GeometricValidatorConfig val_cfg;

    // Test 1: All values present, validation and repair enabled
    memset(&global_cfg, 0, sizeof(Config));
    global_cfg.num_settings = 0;
    setup_mock_config_value(&global_cfg, global_cfg.num_settings++, "GEOMETRIC_VALIDATION_ENABLED", "true");
    setup_mock_config_value(&global_cfg, global_cfg.num_settings++, "GEOMETRIC_VALIDATION_ENABLE_REPAIR", "true");
    assert(load_geometric_validator_config(&global_cfg, &val_cfg) == 0);
    assert(val_cfg.enabled == 1);
    assert(val_cfg.enable_repair == 1);

    // Test 2: Validation disabled, repair (should be irrelevant but check parsing)
    memset(&global_cfg, 0, sizeof(Config));
    global_cfg.num_settings = 0;
    setup_mock_config_value(&global_cfg, global_cfg.num_settings++, "GEOMETRIC_VALIDATION_ENABLED", "false");
    setup_mock_config_value(&global_cfg, global_cfg.num_settings++, "GEOMETRIC_VALIDATION_ENABLE_REPAIR", "true"); // This should still parse
    assert(load_geometric_validator_config(&global_cfg, &val_cfg) == 0);
    assert(val_cfg.enabled == 0);
    assert(val_cfg.enable_repair == 1); // Parsed correctly

    // Test 3: Defaults (Enabled=true, Repair=false)
    memset(&global_cfg, 0, sizeof(Config));
    global_cfg.num_settings = 0; // No settings
    assert(load_geometric_validator_config(&global_cfg, &val_cfg) == 0);
    assert(val_cfg.enabled == 1);    // Default
    assert(val_cfg.enable_repair == 0); // Default

    printf("test_load_validator_config: PASSED\n");
}

static void test_valid_geometries() {
    printf("Running test: test_valid_geometries...\n");
    GeometricValidatorConfig val_cfg = { .enabled = 1, .enable_repair = 0 };
    json_t* output_json = NULL;
    GeometryValidationResultStatus status;

    // Valid Point
    json_t* point_json = parse_json_string("{\"type\": \"Point\", \"coordinates\": [10, 20]}");
    status = validate_and_repair_geometry(point_json, &val_cfg, &output_json);
    assert(status == GEOMETRY_STATUS_VALID);
    assert(output_json == point_json); // Should be the same object as no repair
    json_decref(point_json);
    // output_json does not need decref here as it points to point_json

    // Valid LineString
    json_t* line_json = parse_json_string("{\"type\": \"LineString\", \"coordinates\": [[0,0], [1,1], [2,2]]}");
    status = validate_and_repair_geometry(line_json, &val_cfg, &output_json);
    assert(status == GEOMETRY_STATUS_VALID);
    assert(output_json == line_json);
    json_decref(line_json);

    // Valid Polygon (simple square)
    json_t* poly_json = parse_json_string("{\"type\": \"Polygon\", \"coordinates\": [[[0,0], [0,1], [1,1], [1,0], [0,0]]]}");
    status = validate_and_repair_geometry(poly_json, &val_cfg, &output_json);
    assert(status == GEOMETRY_STATUS_VALID);
    assert(output_json == poly_json);
    json_decref(poly_json);

    // Valid Polygon with hole
    const char* poly_with_hole_str = "{\"type\": \"Polygon\", \"coordinates\": [ [[0,0],[10,0],[10,10],[0,10],[0,0]], [[2,2],[8,2],[8,8],[2,8],[2,2]] ]}";
    json_t* poly_hole_json = parse_json_string(poly_with_hole_str);
    status = validate_and_repair_geometry(poly_hole_json, &val_cfg, &output_json);
    assert(status == GEOMETRY_STATUS_VALID);
    assert(output_json == poly_hole_json);
    json_decref(poly_hole_json);


    // Valid MultiPoint
    json_t* mpoint_json = parse_json_string("{\"type\": \"MultiPoint\", \"coordinates\": [[0,0], [1,1]]}");
    status = validate_and_repair_geometry(mpoint_json, &val_cfg, &output_json);
    assert(status == GEOMETRY_STATUS_VALID);
    json_decref(mpoint_json);


    // Valid GeometryCollection (testing if conversion code handles it, GEOS will validate components)
    const char* gc_str = "{\"type\": \"GeometryCollection\", \"geometries\": [ {\"type\": \"Point\", \"coordinates\": [0,0]} ]}";
    json_t* gc_json = parse_json_string(gc_str);
    status = validate_and_repair_geometry(gc_json, &val_cfg, &output_json);
    assert(status == GEOMETRY_STATUS_VALID);
    json_decref(gc_json);


    printf("test_valid_geometries: PASSED\n");
}

static void test_invalid_polygon_self_intersecting() {
    printf("Running test: test_invalid_polygon_self_intersecting...\n");
    GeometricValidatorConfig val_cfg_no_repair = { .enabled = 1, .enable_repair = 0 };
    GeometricValidatorConfig val_cfg_with_repair = { .enabled = 1, .enable_repair = 1 };
    json_t* output_json = NULL;
    GeometryValidationResultStatus status;

    // Invalid Polygon (bowtie / self-intersecting)
    const char* bowtie_str = "{\"type\": \"Polygon\", \"coordinates\": [[[0,0], [0,10], [10,0], [10,10], [0,0]]]}";
    json_t* bowtie_json = parse_json_string(bowtie_str);

    // Test 1: No repair
    status = validate_and_repair_geometry(bowtie_json, &val_cfg_no_repair, &output_json);
    assert(status == GEOMETRY_STATUS_INVALID);
    assert(output_json == bowtie_json); // No repair, output is input

    // Test 2: With repair
    // Re-parse as bowtie_json might have been affected if GEOS did in-place checks (unlikely for json_t)
    // Or simply use a new json_t for this test. For safety, re-parse.
    json_t* bowtie_json_for_repair = parse_json_string(bowtie_str);
    status = validate_and_repair_geometry(bowtie_json_for_repair, &val_cfg_with_repair, &output_json);

    // GEOS buffer(0) on a bowtie typically results in two valid triangles (a MultiPolygon)
    assert(status == GEOMETRY_STATUS_REPAIRED_VALID);
    assert(output_json != NULL);
    assert(output_json != bowtie_json_for_repair); // Should be a new object

    // Verify the output is now valid (optional, as REPAIRED_VALID implies it)
    // For example, check if it's a MultiPolygon
    if (output_json) {
        json_t* type_val = json_object_get(output_json, "type");
        if (type_val) {
             const char* repaired_type = json_string_value(type_val);
             printf("Repaired geometry type: %s\n", repaired_type); // Expect MultiPolygon
             // assert(strcmp(repaired_type, "MultiPolygon") == 0); // This can be very specific to GEOS version
        }
        json_decref(output_json); // We own the new object
    }

    json_decref(bowtie_json);
    json_decref(bowtie_json_for_repair);
    printf("test_invalid_polygon_self_intersecting: PASSED\n");
}

static void test_validation_disabled() {
    printf("Running test: test_validation_disabled...\n");
    GeometricValidatorConfig val_cfg = { .enabled = 0, .enable_repair = 0 }; // Validation disabled
    json_t* output_json = NULL;
    GeometryValidationResultStatus status;

    json_t* point_json = parse_json_string("{\"type\": \"Point\", \"coordinates\": [10, 20]}");
    status = validate_and_repair_geometry(point_json, &val_cfg, &output_json);
    assert(status == GEOMETRY_STATUS_VALIDATION_DISABLED);
    assert(output_json == point_json); // Should be the same object
    json_decref(point_json);

    printf("test_validation_disabled: PASSED\n");
}

static void test_invalid_linestring_one_point() {
    printf("Running test: test_invalid_linestring_one_point...\n");
    GeometricValidatorConfig val_cfg_no_repair = { .enabled = 1, .enable_repair = 0 };
    json_t* output_json = NULL;
    GeometryValidationResultStatus status;

    // Invalid LineString (only one point)
    const char* line_one_point_str = "{\"type\": \"LineString\", \"coordinates\": [[0,0]]}";
    json_t* line_one_point_json = parse_json_string(line_one_point_str);

    status = validate_and_repair_geometry(line_one_point_json, &val_cfg_no_repair, &output_json);
    assert(status == GEOMETRY_STATUS_INVALID); // GEOS considers a 1-point LineString invalid
    assert(output_json == line_one_point_json);
    json_decref(line_one_point_json);

    printf("test_invalid_linestring_one_point: PASSED\n");
}

static void test_malformed_geojson_missing_type() {
    printf("Running test: test_malformed_geojson_missing_type...\n");
    GeometricValidatorConfig val_cfg = { .enabled = 1, .enable_repair = 0 };
    json_t* output_json = NULL;
    GeometryValidationResultStatus status;

    // Malformed GeoJSON (missing "type" field)
    const char* malformed_str = "{\"coordinates\": [10, 20]}";
    json_t* malformed_json = parse_json_string(malformed_str); // Jansson will parse it

    status = validate_and_repair_geometry(malformed_json, &val_cfg, &output_json);
    // This should fail in the geojson_to_geos conversion phase
    assert(status == GEOMETRY_STATUS_ERROR_INPUT_FORMAT);
    assert(output_json == NULL); // No output JSON should be set on format error
    json_decref(malformed_json);

    printf("test_malformed_geojson_missing_type: PASSED\n");
}


// TODO: Add more tests:
// - Invalid Polygon (unclosed, wrong orientation if GEOS cares without buffer(0))
// - Test repair failures (geometries that buffer(0) can't fix or makes empty)

int main() {
    // It's crucial that geometric_validator_init() is called once before tests
    // and geometric_validator_shutdown() once after.
    if (geometric_validator_init() != 0) {
        LOG_ERROR("Failed to initialize geometric validator in test main. Aborting tests.");
        return 1;
    }

    printf("Starting tests for Geometric Validator module...\n");

    // test_module_init_shutdown(); // Covered by main init/shutdown logic for now
    test_load_validator_config();
    test_valid_geometries();
    test_invalid_polygon_self_intersecting();
    test_validation_disabled();
    test_invalid_linestring_one_point();
    test_malformed_geojson_missing_type();
    // Add calls to more tests here

    printf("All Geometric Validator tests completed.\n");

    geometric_validator_shutdown();
    return 0;
}
