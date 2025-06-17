#include "nlp_ner_parser.h"
#include "utils.h" // For logging (log_error, log_info, log_debug)
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <jansson.h> // For parsing JSON output from the script

int load_nlp_ner_parser_config(const SpindexConfig* global_config, NlpNerParserConfig* ner_config) {
    if (!global_config || !ner_config) {
        LOG_ERROR("NLP_NER: Null config provided to load_nlp_ner_parser_config");
        return -1;
    }

    ner_config->enabled = global_config->nlp_ner_enabled;
    // LOG_INFO("NLP_NER: Module enabled status: %d", ner_config->enabled); // Optional: uncomment for debug

    strncpy(ner_config->python_executable, global_config->nlp_ner_python_executable, MAX_PATH_LEN - 1);
    ner_config->python_executable[MAX_PATH_LEN - 1] = '\0';

    strncpy(ner_config->script_path, global_config->nlp_ner_script_path, MAX_PATH_LEN - 1);
    ner_config->script_path[MAX_PATH_LEN - 1] = '\0';

    // LOG_DEBUG("NLP_NER: Python executable: %s", ner_config->python_executable); // Optional
    // LOG_DEBUG("NLP_NER: Script path: %s", ner_config->script_path); // Optional
    return 0;
}

AEGIS_PROCESSING_STATUS extract_locations_from_text(
    const char* text,
    const NlpNerParserConfig* ner_config,
    char*** extracted_locations_out,
    int* num_locations_out) {

    if (!ner_config || !text || !extracted_locations_out || !num_locations_out) {
        LOG_ERROR("NLP_NER: Null arguments passed to extract_locations_from_text.");
        return AEGIS_PROCESSING_ERROR_ARGS;
    }

    *extracted_locations_out = NULL;
    *num_locations_out = 0;

    if (!ner_config->enabled) {
        LOG_INFO("NLP_NER: Module is disabled via configuration.");
        return AEGIS_PROCESSING_NLP_NER_DISABLED;
    }

    // Basic check for script existence - can be enhanced
    FILE* script_file = fopen(ner_config->script_path, "r");
    if (!script_file) {
        LOG_ERROR("NLP_NER: Script not found at path: %s", ner_config->script_path);
        return AEGIS_PROCESSING_ERROR_CONFIG; // Or a more specific error
    }
    fclose(script_file);


    // Command construction: python_executable script_path "text"
    // Need to handle escaping quotes in text if any, but popen usually handles args.
    // For simplicity, assuming text doesn't contain characters that break shell command.
    // A more robust solution might pass text via stdin or use library for subprocess management.
    size_t cmd_len = strlen(ner_config->python_executable) + 1 + strlen(ner_config->script_path) + 3 + strlen(text) + 2; // spaces, quotes, null
    char* cmd = (char*)malloc(cmd_len);
    if (!cmd) {
        LOG_ERROR("NLP_NER: Failed to allocate memory for command string.");
        return AEGIS_PROCESSING_ERROR_MEMORY;
    }
    // Using single quotes around the text argument for `sh` to handle most special characters.
    // If text itself contains single quotes, this will be problematic.
    // A production system would need much more robust argument escaping or use exec-family functions with argument arrays.
    snprintf(cmd, cmd_len, "%s %s '%s'", ner_config->python_executable, ner_config->script_path, text);
    // LOG_DEBUG("NLP_NER: Executing command: %s", cmd);

    FILE* pipe = popen(cmd, "r");
    free(cmd);
    if (!pipe) {
        LOG_ERROR("NLP_NER: popen() failed to execute NER script.");
        return AEGIS_PROCESSING_ERROR_EXTERNAL_TOOL;
    }

    char buffer[1024];
    char* full_output = NULL;
    size_t current_size = 0;
    size_t bytes_read_total = 0;

    while (fgets(buffer, sizeof(buffer), pipe) != NULL) {
        size_t bytes_read = strlen(buffer);
        char* temp = (char*)realloc(full_output, current_size + bytes_read + 1);
        if (!temp) {
            LOG_ERROR("NLP_NER: Failed to realloc memory for script output.");
            free(full_output);
            pclose(pipe);
            return AEGIS_PROCESSING_ERROR_MEMORY;
        }
        full_output = temp;
        strcpy(full_output + current_size, buffer);
        current_size += bytes_read;
        bytes_read_total += bytes_read;
    }

    int script_status = pclose(pipe);
    if (script_status != 0) {
        LOG_ERROR("NLP_NER: NER script execution failed with status %d. Output: %s", script_status, full_output ? full_output : "(empty)");
        free(full_output);
        return AEGIS_PROCESSING_ERROR_EXTERNAL_TOOL;
    }

    if (!full_output || bytes_read_total == 0) {
        LOG_INFO("NLP_NER: NER script produced no output.");
        free(full_output); // It might be allocated but empty if realloc succeeded once.
        return AEGIS_PROCESSING_SUCCESS; // No locations found is not an error.
    }
    // LOG_DEBUG("NLP_NER: Script output: %s", full_output);

    json_error_t error;
    json_t* root = json_loads(full_output, 0, &error);
    free(full_output);

    if (!root) {
        LOG_ERROR("NLP_NER: Failed to parse JSON output from NER script: %s (line %d, col %d)", error.text, error.line, error.column);
        return AEGIS_PROCESSING_ERROR_INVALID_FORMAT;
    }

    if (!json_is_array(root)) {
        LOG_ERROR("NLP_NER: JSON output from NER script is not an array.");
        json_decref(root);
        return AEGIS_PROCESSING_ERROR_INVALID_FORMAT;
    }

    size_t num_items = json_array_size(root);
    if (num_items == 0) {
        // LOG_INFO("NLP_NER: No locations found by NER script.");
        json_decref(root);
        return AEGIS_PROCESSING_SUCCESS;
    }

    *extracted_locations_out = (char**)malloc(num_items * sizeof(char*));
    if (!(*extracted_locations_out)) {
        LOG_ERROR("NLP_NER: Failed to allocate memory for locations array.");
        json_decref(root);
        return AEGIS_PROCESSING_ERROR_MEMORY;
    }
    memset(*extracted_locations_out, 0, num_items * sizeof(char*)); // Important for cleanup on error

    for (size_t i = 0; i < num_items; ++i) {
        json_t* item = json_array_get(root, i);
        if (!json_is_string(item)) {
            LOG_WARN("NLP_NER: Non-string item found in NER script output array at index %zu.", i);
            // Skip this item, or error out. For now, skip.
            continue;
        }
        const char* location_str = json_string_value(item);
        (*extracted_locations_out)[*num_locations_out] = strdup(location_str);
        if (!(*extracted_locations_out)[*num_locations_out]) {
            LOG_ERROR("NLP_NER: Failed to strdup location string.");
            // Free already duplicated strings and the array
            free_extracted_locations(*extracted_locations_out, *num_locations_out);
            *extracted_locations_out = NULL; // Ensure it's NULL for the caller
            json_decref(root);
            return AEGIS_PROCESSING_ERROR_MEMORY;
        }
        (*num_locations_out)++;
    }

    json_decref(root);
    // LOG_INFO("NLP_NER: Successfully extracted %d locations.", *num_locations_out);
    return AEGIS_PROCESSING_SUCCESS;
}

void free_extracted_locations(char** locations_array, int num_locations) {
    if (locations_array) {
        for (int i = 0; i < num_locations; ++i) {
            free(locations_array[i]); // Free each string
        }
        free(locations_array); // Free the array itself
    }
}
