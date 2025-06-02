#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cJSON.h"
#include "json_preprocessor.h"

// Function to preprocess JSON-like strings
char *preprocess_json(const char *raw_content) {
    if (!raw_content) {
        fprintf(stderr, "[ERROR] Raw content is NULL.\n");
        return NULL;
    }

    // Check if the content starts with valid JSON characters
    if (raw_content[0] != '{' && raw_content[0] != '[') {
        // Wrap the content in an array and object
        size_t len = strlen(raw_content) + 10; // Add extra space for wrapping
        char *processed_content = (char *)malloc(len);
        if (!processed_content) {
            fprintf(stderr, "[ERROR] Memory allocation failed while preprocessing JSON.\n");
            return NULL;
        }
        snprintf(processed_content, len, "[{%s}]", raw_content);
        fprintf(stdout, "[INFO] Content wrapped for JSON compatibility: %s\n", processed_content);
        return processed_content;
    }

    // If already valid, return a copy of the raw content
    char *copied_content = strdup(raw_content);
    if (!copied_content) {
        fprintf(stderr, "[ERROR] Memory allocation failed while copying raw content.\n");
        return NULL;
    }
    fprintf(stdout, "[INFO] Content is already valid JSON.\n");
    return copied_content;
}

// Function to parse JSON using cJSON
cJSON *parse_json(const char *processed_content) {
    if (!processed_content) {
        fprintf(stderr, "[ERROR] Processed content is NULL. Cannot parse JSON.\n");
        return NULL;
    }

    cJSON *json = cJSON_Parse(processed_content);
    if (!json) {
        fprintf(stderr, "[ERROR] Failed to parse JSON: %s\n", cJSON_GetErrorPtr());
        return NULL;
    }
    fprintf(stdout, "[INFO] JSON parsed successfully.\n");
    return json;
}
