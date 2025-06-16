#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "config_manager.h" // For Config structure
#include "data_types.h"     // For AEGIS_PROCESSING_STATUS
#include "nlp_ner_parser.h" // The module to test
#include "utils.h"          // For logging, if used during test debugging

// Helper to setup a mock Config object for testing config loading
static void setup_mock_config_value(Config* cfg, int index, const char* key, const char* value) {
    if (index < MAX_SETTINGS) {
        strncpy(cfg->settings[index].key, key, MAX_KEY_LEN - 1);
        cfg->settings[index].key[MAX_KEY_LEN - 1] = '\0';
        strncpy(cfg->settings[index].value, value, MAX_VALUE_LEN - 1);
        cfg->settings[index].value[MAX_VALUE_LEN - 1] = '\0';
    }
}

static void test_load_ner_config() {
    printf("Running test: test_load_ner_config...\n");
    Config global_cfg;
    NlpNerParserConfig ner_cfg;

    // Test 1: All values present and NER enabled
    memset(&global_cfg, 0, sizeof(Config));
    global_cfg.num_settings = 0;
    setup_mock_config_value(&global_cfg, global_cfg.num_settings++, "NLP_NER_ENABLED", "true");
    setup_mock_config_value(&global_cfg, global_cfg.num_settings++, "NLP_NER_PYTHON_EXECUTABLE", "/usr/bin/python3_test");
    setup_mock_config_value(&global_cfg, global_cfg.num_settings++, "NLP_NER_SCRIPT_PATH", "scripts/test_ner.py");

    assert(load_nlp_ner_parser_config(&global_cfg, &ner_cfg) == 0);
    assert(ner_cfg.enabled == 1);
    assert(strcmp(ner_cfg.python_executable, "/usr/bin/python3_test") == 0);
    assert(strcmp(ner_cfg.script_path, "scripts/test_ner.py") == 0);

    // Test 2: NER disabled
    memset(&global_cfg, 0, sizeof(Config));
    global_cfg.num_settings = 0;
    setup_mock_config_value(&global_cfg, global_cfg.num_settings++, "NLP_NER_ENABLED", "false");
    // Python exe and script path will use defaults if not specified
    assert(load_nlp_ner_parser_config(&global_cfg, &ner_cfg) == 0);
    assert(ner_cfg.enabled == 0);
    // Check defaults (assuming defines DEFAULT_NLP_NER_PYTHON_EXECUTABLE and DEFAULT_NLP_NER_SCRIPT_PATH exist in nlp_ner_parser.c)
    // These defaults are "python3" and "scripts/ner_spacy.py"
    assert(strcmp(ner_cfg.python_executable, "python3") == 0); // Default
    assert(strcmp(ner_cfg.script_path, "scripts/ner_spacy.py") == 0); // Default

    // Test 3: Defaults used when keys are missing (NER should be disabled by default if NLP_NER_ENABLED is missing)
    memset(&global_cfg, 0, sizeof(Config));
    global_cfg.num_settings = 0; // No settings
    assert(load_nlp_ner_parser_config(&global_cfg, &ner_cfg) == 0);
    assert(ner_cfg.enabled == 0); // Default for enabled is false if key missing
    assert(strcmp(ner_cfg.python_executable, "python3") == 0);
    assert(strcmp(ner_cfg.script_path, "scripts/ner_spacy.py") == 0);

    printf("test_load_ner_config: PASSED\n");
}

// This test requires the actual scripts/ner_spacy.py and a valid python environment with spacy
static void test_extract_locations_success() {
    printf("Running test: test_extract_locations_success...\n");
    NlpNerParserConfig ner_cfg;
    AEGIS_PROCESSING_STATUS status;
    char** locations = NULL;
    int num_locations = 0;

    // Setup config to use default python3 and script path, and enable NER
    Config global_cfg;
    memset(&global_cfg, 0, sizeof(Config));
    global_cfg.num_settings = 0;
    setup_mock_config_value(&global_cfg, global_cfg.num_settings++, "NLP_NER_ENABLED", "true");
    // Assuming default script path "scripts/ner_spacy.py" and "python3" are valid in test environment
    // and that script `scripts/ner_spacy.py` exists relative to where the test is run.
    // The Makefile might need to ensure `scripts/ner_spacy.py` is accessible.
    // For local testing, ensure `SPINDEX/scripts/ner_spacy.py` exists.

    // A bit of a hack: nlp_ner_parser.c uses fopen to check script path.
    // Let's ensure a dummy script exists at the default path for this part of the test,
    // if the real one isn't found by the fopen check in extract_locations_from_text.
    // The actual popen call will use the real script.
    FILE* dummy_script_fp = fopen("scripts/ner_spacy.py", "r");
    if (!dummy_script_fp) {
        // Attempt to create a dummy if not found, to pass initial check in extract_locations_from_text
        // This is only to satisfy the fopen check, the actual execution will fail if python/spacy aren't set up.
        printf("WARN: Real 'scripts/ner_spacy.py' not found, creating dummy for fopen check. Actual NER call might fail.\n");
        system("mkdir -p scripts"); // Ensure directory exists
        FILE* f = fopen("scripts/ner_spacy.py", "w");
        if(f) { fprintf(f, "#dummy\nprint(\"[]\")"); fclose(f); }
    } else {
        fclose(dummy_script_fp);
    }


    assert(load_nlp_ner_parser_config(&global_cfg, &ner_cfg) == 0);
    assert(ner_cfg.enabled == 1);

    const char* text_with_locations = "I travelled from London to Paris, stopping by Berlin.";
    status = extract_locations_from_text(text_with_locations, &ner_cfg, &locations, &num_locations);

    // This assertion depends HEAVILY on the test environment having python, spacy, and the model.
    // If the environment is not set up, this will likely fail here or return 0 locations.
    assert(status == AEGIS_PROCESSING_SUCCESS);
    printf("Found %d locations:\n", num_locations);
    int london_found = 0;
    int paris_found = 0;
    int berlin_found = 0;
    for (int i = 0; i < num_locations; ++i) {
        printf("  - %s\n", locations[i]);
        if (strcmp(locations[i], "London") == 0) london_found = 1;
        if (strcmp(locations[i], "Paris") == 0) paris_found = 1;
        if (strcmp(locations[i], "Berlin") == 0) berlin_found = 1;
    }
    // Check if at least these were found. SpaCy might find more or variations.
    assert(num_locations >= 3); // Expect at least these three
    assert(london_found && paris_found && berlin_found);

    free_extracted_locations(locations, num_locations);

    // Test with no locations
    const char* text_no_locations = "This is a simple sentence without any cities.";
    status = extract_locations_from_text(text_no_locations, &ner_cfg, &locations, &num_locations);
    assert(status == AEGIS_PROCESSING_SUCCESS);
    assert(num_locations == 0);
    free_extracted_locations(locations, num_locations);

    printf("test_extract_locations_success: PASSED (or skipped if env not set up)\n");
}

static void test_ner_disabled() {
    printf("Running test: test_ner_disabled...\n");
    NlpNerParserConfig ner_cfg;
    AEGIS_PROCESSING_STATUS status;
    char** locations = NULL;
    int num_locations = 0;

    Config global_cfg;
    memset(&global_cfg, 0, sizeof(Config));
    global_cfg.num_settings = 0;
    setup_mock_config_value(&global_cfg, global_cfg.num_settings++, "NLP_NER_ENABLED", "false");
    assert(load_nlp_ner_parser_config(&global_cfg, &ner_cfg) == 0);
    assert(ner_cfg.enabled == 0);

    const char* text = "Some text here.";
    status = extract_locations_from_text(text, &ner_cfg, &locations, &num_locations);
    assert(status == AEGIS_PROCESSING_NLP_NER_DISABLED);
    assert(num_locations == 0);
    assert(locations == NULL);

    printf("test_ner_disabled: PASSED\n");
}

static void test_script_not_found() {
    printf("Running test: test_script_not_found...\n");
    NlpNerParserConfig ner_cfg;
    AEGIS_PROCESSING_STATUS status;
    char** locations = NULL;
    int num_locations = 0;

    Config global_cfg;
    memset(&global_cfg, 0, sizeof(Config));
    global_cfg.num_settings = 0;
    setup_mock_config_value(&global_cfg, global_cfg.num_settings++, "NLP_NER_ENABLED", "true");
    setup_mock_config_value(&global_cfg, global_cfg.num_settings++, "NLP_NER_SCRIPT_PATH", "non_existent_scripts/non_existent_ner.py");
    assert(load_nlp_ner_parser_config(&global_cfg, &ner_cfg) == 0);

    const char* text = "Some text.";
    // This test relies on the fopen check in extract_locations_from_text
    status = extract_locations_from_text(text, &ner_cfg, &locations, &num_locations);
    assert(status == AEGIS_PROCESSING_ERROR_CONFIG); // Because script path check fails
    assert(num_locations == 0);
    assert(locations == NULL);

    printf("test_script_not_found: PASSED\n");
}


int main() {
    // init_logger for debugging if necessary, e.g.
    // init_logger("test_nlp_ner_parser.log", LOG_LEVEL_DEBUG, 0);

    printf("Starting tests for NLP/NER Parser module...\n");

    test_load_ner_config();
    test_ner_disabled();
    test_script_not_found();

    // The success test is environment-dependent.
    // Consider adding a check or environment variable to skip it if Python/spaCy is not available.
    printf("NOTE: test_extract_locations_success requires a functional Python3, spaCy, and en_core_web_sm model.\n");
    printf("      It also expects 'scripts/ner_spacy.py' to be accessible from the build execution directory.\n");
    test_extract_locations_success();


    printf("All NLP/NER Parser tests completed.\n");
    return 0;
}
