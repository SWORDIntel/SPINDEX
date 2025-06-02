// json_preprocessor.c

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <ctype.h>

// Include necessary libraries for JSON parsing, schema validation, and encoding handling
#include "cJSON.h"
#include "jsonschemac.h"
#include <uchardet/uchardet.h>
#include <iconv.h>
#include <unistr.h>
#include <unorm.h>

/* Enumerations and typedefs */
typedef enum {
    LOG_LEVEL_DEBUG,
    LOG_LEVEL_INFO,
    LOG_LEVEL_WARN,
    LOG_LEVEL_ERROR
} LogLevel;

/* Global variables */
static LogLevel current_log_level = LOG_LEVEL_INFO;

/* Function prototypes */
/* Logging functions */
void set_log_level(LogLevel level);
void log_message(LogLevel level, const char *format, ...);

/* Utility functions */
char *trim_whitespace(const char *str);
char *detect_and_convert_to_utf8(const char *data, size_t data_length);
char *normalize_unicode(const char *utf8_string);

/* JSON preprocessing functions */
char *preprocess_json(const char *raw_content);

/* Schema loading and validation functions */
cJSON *load_cjson_from_file(const char *file_path);
int validate_json(cJSON *data, cJSON *schema);

/* JSON parsing function */
cJSON *parse_json(const char *processed_content, const char *schema_file_path);

/* Function implementations */

/* Logging functions */
void set_log_level(LogLevel level) {
    current_log_level = level;
}

void log_message(LogLevel level, const char *format, ...) {
    if (level < current_log_level) {
        return;
    }

    const char *level_strings[] = {"DEBUG", "INFO", "WARN", "ERROR"};
    time_t now = time(NULL);
    char time_buff[20];
    strftime(time_buff, sizeof(time_buff), "%Y-%m-%d %H:%M:%S", localtime(&now));

    fprintf(stdout, "[%s] [%s]: ", time_buff, level_strings[level]);

    va_list args;
    va_start(args, format);
    vfprintf(stdout, format, args);
    va_end(args);

    fprintf(stdout, "\n");
}

/* Utility functions */
char *trim_whitespace(const char *str) {
    if (!str) {
        return NULL;
    }

    const char *end;
    size_t len;

    /* Trim leading space */
    while (isspace((unsigned char)*str)) {
        str++;
    }

    if (*str == 0) {
        /* All spaces */
        return strdup("");
    }

    /* Trim trailing space */
    end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) {
        end--;
    }

    len = end - str + 1;

    /* Allocate memory for trimmed string */
    char *trimmed_str = (char *)malloc(len + 1);
    if (!trimmed_str) {
        return NULL;
    }

    strncpy(trimmed_str, str, len);
    trimmed_str[len] = '\0';

    return trimmed_str;
}

/* Encoding detection and conversion */
char *detect_and_convert_to_utf8(const char *data, size_t data_length) {
    /* Detect encoding */
    uchardet_t detector = uchardet_new();
    if (uchardet_handle_data(detector, data, data_length) != 0) {
        log_message(LOG_LEVEL_ERROR, "Encoding detection failed.");
        uchardet_delete(detector);
        return NULL;
    }
    uchardet_data_end(detector);
    const char *charset = uchardet_get_charset(detector);
    if (!charset || strlen(charset) == 0) {
        log_message(LOG_LEVEL_ERROR, "Failed to detect charset.");
        uchardet_delete(detector);
        return NULL;
    }
    log_message(LOG_LEVEL_INFO, "Detected charset: %s", charset);
    uchardet_delete(detector);

    /* Convert to UTF-8 if necessary */
    if (strcasecmp(charset, "UTF-8") == 0) {
        /* Data is already UTF-8 encoded */
        char *utf8_data = (char *)malloc(data_length + 1);
        if (!utf8_data) {
            log_message(LOG_LEVEL_ERROR, "Memory allocation failed during UTF-8 conversion.");
            return NULL;
        }
        memcpy(utf8_data, data, data_length);
        utf8_data[data_length] = '\0';
        return utf8_data;
    } else {
        /* Use iconv for conversion */
        iconv_t cd = iconv_open("UTF-8", charset);
        if (cd == (iconv_t)-1) {
            log_message(LOG_LEVEL_ERROR, "iconv_open failed for charset: %s", charset);
            return NULL;
        }

        size_t inbytesleft = data_length;
        size_t outbytesleft = data_length * 4; /* Allocating more space for output */
        char *outbuf = (char *)malloc(outbytesleft + 1);
        if (!outbuf) {
            log_message(LOG_LEVEL_ERROR, "Memory allocation failed during UTF-8 conversion.");
            iconv_close(cd);
            return NULL;
        }

        char *inbuf = (char *)data;
        char *outptr = outbuf;

        size_t ret = iconv(cd, &inbuf, &inbytesleft, &outptr, &outbytesleft);
        if (ret == (size_t)-1) {
            log_message(LOG_LEVEL_ERROR, "iconv conversion failed.");
            free(outbuf);
            iconv_close(cd);
            return NULL;
        }

        *outptr = '\0';
        iconv_close(cd);

        return outbuf;
    }
}

/* Unicode normalization */
char *normalize_unicode(const char *utf8_string) {
    if (!utf8_string) {
        return NULL;
    }

    size_t length = strlen(utf8_string);
    uint8_t *dest = (uint8_t *)malloc(length * 4); /* Allocate buffer for normalized string */
    if (!dest) {
        log_message(LOG_LEVEL_ERROR, "Memory allocation failed during Unicode normalization.");
        return NULL;
    }

    size_t normalized_length = unorm_normalize(UNORM_NFC, (const uint8_t *)utf8_string, length, dest, length * 4);
    if (normalized_length == (size_t)-1) {
        log_message(LOG_LEVEL_ERROR, "Unicode normalization failed.");
        free(dest);
        return NULL;
    }

    char *normalized_str = (char *)malloc(normalized_length + 1);
    if (!normalized_str) {
        log_message(LOG_LEVEL_ERROR, "Memory allocation failed during Unicode normalization.");
        free(dest);
        return NULL;
    }

    memcpy(normalized_str, dest, normalized_length);
    normalized_str[normalized_length] = '\0';

    free(dest);
    return normalized_str;
}

/* JSON preprocessing */
char *preprocess_json(const char *raw_content) {
    if (!raw_content) {
        log_message(LOG_LEVEL_ERROR, "Raw content is NULL.");
        return NULL;
    }

    size_t data_length = strlen(raw_content);

    /* Detect and convert encoding to UTF-8 */
    char *utf8_content = detect_and_convert_to_utf8(raw_content, data_length);
    if (!utf8_content) {
        log_message(LOG_LEVEL_ERROR, "Failed to convert content to UTF-8.");
        return NULL;
    }

    /* Normalize Unicode */
    char *normalized_content = normalize_unicode(utf8_content);
    free(utf8_content);
    if (!normalized_content) {
        log_message(LOG_LEVEL_ERROR, "Failed to normalize Unicode content.");
        return NULL;
    }

    /* Trim whitespace */
    char *trimmed_content = trim_whitespace(normalized_content);
    free(normalized_content);
    if (!trimmed_content) {
        log_message(LOG_LEVEL_ERROR, "Memory allocation failed while trimming content.");
        return NULL;
    }

    /* Check if the content starts with valid JSON characters */
    if (trimmed_content[0] != '{' && trimmed_content[0] != '[') {
        /* Wrap the content in an array and object */
        size_t len = strlen(trimmed_content) + 10; /* Add extra space for wrapping */
        char *processed_content = (char *)malloc(len);
        if (!processed_content) {
            log_message(LOG_LEVEL_ERROR, "Memory allocation failed while preprocessing JSON.");
            free(trimmed_content);
            return NULL;
        }
        snprintf(processed_content, len, "[{%s}]", trimmed_content);
        log_message(LOG_LEVEL_INFO, "Content wrapped for JSON compatibility.");
        free(trimmed_content);
        return processed_content;
    }

    /* Content is already valid JSON */
    log_message(LOG_LEVEL_INFO, "Content is already valid JSON.");
    return trimmed_content;
}

/* Schema loading and validation */

/* Function to load cJSON object from file */
cJSON *load_cjson_from_file(const char *file_path) {
    FILE *file = fopen(file_path, "r");
    if (!file) {
        log_message(LOG_LEVEL_ERROR, "Could not open file: %s", file_path);
        return NULL;
    }

    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    rewind(file);

    char *file_content = (char *)malloc(file_size + 1);
    if (!file_content) {
        log_message(LOG_LEVEL_ERROR, "Memory allocation failed while reading file.");
        fclose(file);
        return NULL;
    }

    size_t read_size = fread(file_content, 1, file_size, file);
    file_content[read_size] = '\0';
    fclose(file);

    cJSON *json = cJSON_Parse(file_content);
    if (!json) {
        log_message(LOG_LEVEL_ERROR, "Failed to parse JSON from file %s: %s", file_path, cJSON_GetErrorPtr());
        free(file_content);
        return NULL;
    }

    free(file_content);
    return json;
}

int validate_json(cJSON *data, cJSON *schema) {
    if (!data || !schema) {
        log_message(LOG_LEVEL_ERROR, "Data or schema is NULL.");
        return 0;
    }

    /* Convert cJSON objects to strings */
    char *data_string = cJSON_PrintUnformatted(data);
    char *schema_string = cJSON_PrintUnformatted(schema);

    if (!data_string || !schema_string) {
        log_message(LOG_LEVEL_ERROR, "Failed to convert cJSON to string.");
        if (data_string) free(data_string);
        if (schema_string) free(schema_string);
        return 0;
    }

    /* Parse JSON data and schema using jsonschema-c */
    json_value *json = json_parse((json_char *)data_string, strlen(data_string));
    json_value *schema_json = json_parse((json_char *)schema_string, strlen(schema_string));

    free(data_string);
    free(schema_string);

    if (json == NULL || schema_json == NULL) {
        log_message(LOG_LEVEL_ERROR, "Failed to parse JSON data or schema using jsonschema-c.");
        if (json) json_value_free(json);
        if (schema_json) json_value_free(schema_json);
        return 0;
    }

    /* Validate the JSON data against the schema */
    int validation_result = jsonschema_is_valid(schema_json, json);

    if (validation_result) {
        log_message(LOG_LEVEL_INFO, "JSON validation succeeded.");
    } else {
        /* Get the error message */
        jsonschema_validation_errors *errors = jsonschema_get_errors();
        if (errors != NULL && errors->message != NULL) {
            log_message(LOG_LEVEL_ERROR, "JSON validation failed: %s", errors->message);
        } else {
            log_message(LOG_LEVEL_ERROR, "JSON validation failed with unknown error.");
        }
        jsonschema_validation_errors_free(errors);
    }

    /* Cleanup */
    json_value_free(json);
    json_value_free(schema_json);

    return validation_result;
}

/* JSON parsing */
cJSON *parse_json(const char *processed_content, const char *schema_file_path) {
    if (!processed_content) {
        log_message(LOG_LEVEL_ERROR, "Processed content is NULL. Cannot parse JSON.");
        return NULL;
    }

    cJSON *json = cJSON_Parse(processed_content);
    if (!json) {
        log_message(LOG_LEVEL_ERROR, "Failed to parse JSON: %s", cJSON_GetErrorPtr());
        return NULL;
    }
    log_message(LOG_LEVEL_INFO, "JSON parsed successfully.");

    /* Load schema */
    cJSON *schema = load_cjson_from_file(schema_file_path);
    if (!schema) {
        log_message(LOG_LEVEL_ERROR, "Failed to load JSON schema from file: %s", schema_file_path);
        cJSON_Delete(json);
        return NULL;
    }

    /* Validate JSON against the schema */
    if (!validate_json(json, schema)) {
        log_message(LOG_LEVEL_WARN, "JSON validation against schema failed.");
        cJSON_Delete(json);
        cJSON_Delete(schema);
        return NULL;
    }
    log_message(LOG_LEVEL_INFO, "JSON validation successful.");

    cJSON_Delete(schema);
    return json;
}

/* Main function (for demonstration purposes) */
int main(int argc, char *argv[]) {
    /* Set log level (can be modified as needed) */
    set_log_level(LOG_LEVEL_DEBUG);

    if (argc < 3) {
        fprintf(stderr, "Usage: %s <json_file> <schema_file>\n", argv[0]);
        return 1;
    }

    const char *json_file_path = argv[1];
    const char *schema_file_path = argv[2];

    /* Read raw JSON content from file */
    FILE *json_file = fopen(json_file_path, "r");
    if (!json_file) {
        log_message(LOG_LEVEL_ERROR, "Could not open JSON file: %s", json_file_path);
        return 1;
    }

    fseek(json_file, 0, SEEK_END);
    long json_file_size = ftell(json_file);
    rewind(json_file);

    char *raw_json_content = (char *)malloc(json_file_size + 1);
    if (!raw_json_content) {
        log_message(LOG_LEVEL_ERROR, "Memory allocation failed while reading JSON file.");
        fclose(json_file);
        return 1;
    }

    size_t read_size = fread(raw_json_content, 1, json_file_size, json_file);
    raw_json_content[read_size] = '\0';
    fclose(json_file);

    /* Preprocess JSON */
    char *processed_content = preprocess_json(raw_json_content);
    free(raw_json_content);
    if (!processed_content) {
        log_message(LOG_LEVEL_ERROR, "Failed to preprocess JSON content.");
        return 1;
    }

    /* Parse and validate JSON */
    cJSON *json = parse_json(processed_content, schema_file_path);
    free(processed_content);
    if (!json) {
        log_message(LOG_LEVEL_ERROR, "Failed to parse and validate JSON content.");
        return 1;
    }

    /* Proceed with using the validated JSON data */
    /* For demonstration, we'll print the JSON content */
    char *json_string = cJSON_Print(json);
    if (json_string) {
        printf("Validated JSON:\n%s\n", json_string);
        free(json_string);
    }

    /* Clean up */
    cJSON_Delete(json);

    return 0;
}
