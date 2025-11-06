#define _GNU_SOURCE // For strdup, mkstemp
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>

#include "sqlite_parser.h"
#include "../../include/utils.h"
#include "../../include/data_types.h" // For CHUNK_TYPE_TEXT_PARSED

static const char* G_PLUGIN_NAME = "SQLiteParser";
static const char* G_PLUGIN_VERSION = "0.2.0";
static const char* G_PLUGIN_DESCRIPTION = "Parses SQLite DB files using external tool (sqlite3 .dump).";

void spindex_plugin_get_info(PluginInfo *info) {
    if (info) {
        strncpy(info->name, G_PLUGIN_NAME, sizeof(info->name) - 1);
        info->name[sizeof(info->name) - 1] = '\0';
        strncpy(info->version, G_PLUGIN_VERSION, sizeof(info->version) - 1);
        info->version[sizeof(info->version) - 1] = '\0';
        info->type = SPINDEX_PLUGIN_TYPE_PARSER;
        strncpy(info->description, G_PLUGIN_DESCRIPTION, sizeof(info->description) - 1);
        info->description[sizeof(info->description) - 1] = '\0';
    }
}

int spindex_plugin_init() {
    LOG_INFO("[%s v%s] Initializing...", G_PLUGIN_NAME, G_PLUGIN_VERSION);
    return 0;
}

void spindex_plugin_shutdown() {
    LOG_INFO("[%s v%s] Shutting down...", G_PLUGIN_NAME, G_PLUGIN_VERSION);
}

ProcessedDataChunk* spindex_plugin_process_data(const ProcessedDataChunk *input_chunk) {
    LOG_INFO("[%s v%s] Processing data...", G_PLUGIN_NAME, G_PLUGIN_VERSION);
    if (!input_chunk || !input_chunk->data || input_chunk->length == 0) {
        LOG_ERROR("[%s] Input chunk is NULL, has no data, or length is zero.", G_PLUGIN_NAME);
        return NULL;
    }

    char actual_input_path_processed[256]; // Buffer for actual path after mkstemp + suffix
    char actual_output_path_template[] = "/tmp/spindex_sqlite_out_XXXXXX"; // Correct template for mkstemp
    char actual_output_path[256]; // Buffer to hold the actual unique name

    int fd_input = -1;
    FILE *fp_output_read = NULL;
    ProcessedDataChunk *output_chunk = NULL;
    char *output_buffer = NULL;
    long output_buffer_size = 0;

    // Create unique temporary input file path
    // We need to ensure the XXXXXX is at the end for mkstemp.
    // The .db suffix is added after mkstemp generates the unique part.
    char template_in_for_mkstemp[] = "/tmp/spindex_sqlite_in_XXXXXX";
    fd_input = mkstemp(template_in_for_mkstemp);
    if (fd_input == -1) {
        LOG_ERROR("[%s] Failed to create temporary input file base '%s': %s", G_PLUGIN_NAME, template_in_for_mkstemp, strerror(errno));
        goto cleanup;
    }
    snprintf(actual_input_path_processed, sizeof(actual_input_path_processed), "%s.db", template_in_for_mkstemp);
    if (rename(template_in_for_mkstemp, actual_input_path_processed) != 0) {
        LOG_ERROR("[%s] Failed to rename temporary file from '%s' to '%s': %s", G_PLUGIN_NAME, template_in_for_mkstemp, actual_input_path_processed, strerror(errno));
        unlink(template_in_for_mkstemp); // Clean up original temp file
        goto cleanup;
    }
    LOG_INFO("[%s] Created temporary input file: %s", G_PLUGIN_NAME, actual_input_path_processed);
    // fd_input was for template_in_for_mkstemp, already closed by rename implicitly if successful? No, close it.
    close(fd_input); // fd_input is for template_in_for_mkstemp before rename
    fd_input = -1;

    FILE* fp_input = fopen(actual_input_path_processed, "wb");
    if (!fp_input) {
        LOG_ERROR("[%s] Failed to open temporary input file '%s' for writing: %s", G_PLUGIN_NAME, actual_input_path_processed, strerror(errno));
        goto cleanup;
    }
    ssize_t bytes_written = fwrite(input_chunk->data, 1, input_chunk->length, fp_input);
    fclose(fp_input);

    if (bytes_written != (ssize_t)input_chunk->length) {
        LOG_ERROR("[%s] Failed to write all data to temporary input file '%s'. Expected %zu, wrote %zd", G_PLUGIN_NAME, actual_input_path_processed, input_chunk->length, bytes_written);
        goto cleanup;
    }

    // Prepare output file path
    // actual_output_path_template is "/tmp/spindex_sqlite_out_XXXXXX"
    strncpy(actual_output_path, actual_output_path_template, sizeof(actual_output_path) -1);
    actual_output_path[sizeof(actual_output_path)-1] = '\0';

    int fd_out_dummy = mkstemp(actual_output_path); // This makes actual_output_path the unique name
    if (fd_out_dummy == -1) {
        LOG_ERROR("[%s] Failed to create temporary output file name with template '%s': %s", G_PLUGIN_NAME, actual_output_path_template, strerror(errno));
        goto cleanup;
    }
    close(fd_out_dummy);
    // The file actual_output_path now exists. sqlite3 .dump > will overwrite it.

    char command[1024];
    snprintf(command, sizeof(command), "sqlite3 %s .dump > %s", actual_input_path_processed, actual_output_path);

    LOG_INFO("[%s] Executing command: %s", G_PLUGIN_NAME, command);
    int ret = system(command);
    if (ret != 0) {
        LOG_ERROR("[%s] Command execution failed with status %d. Command: %s", G_PLUGIN_NAME, ret, command);
        goto cleanup;
    }

    fp_output_read = fopen(actual_output_path, "rb");
    if (!fp_output_read) {
        LOG_ERROR("[%s] Failed to open temporary output file for reading: '%s': %s", G_PLUGIN_NAME, actual_output_path, strerror(errno));
        goto cleanup;
    }

    fseek(fp_output_read, 0, SEEK_END);
    output_buffer_size = ftell(fp_output_read);
     if (output_buffer_size < 0) {
        LOG_ERROR("[%s] Failed to determine size of output file or file is empty: '%s'", G_PLUGIN_NAME, actual_output_path);
        goto cleanup;
    }
    fseek(fp_output_read, 0, SEEK_SET);

    if (output_buffer_size == 0) {
        LOG_WARN("[%s] Output file is empty: %s", G_PLUGIN_NAME, actual_output_path);
        output_buffer = NULL;
    } else {
        output_buffer = (char*)malloc(output_buffer_size + 1);
        if (!output_buffer) {
            LOG_ERROR("[%s] Failed to allocate memory for output buffer (%ld bytes).", G_PLUGIN_NAME, output_buffer_size);
            goto cleanup;
        }
        size_t bytes_read_output = fread(output_buffer, 1, output_buffer_size, fp_output_read);
        if (bytes_read_output != (size_t)output_buffer_size) {
            LOG_ERROR("[%s] Failed to read all data from temporary output file. Expected %ld, read %zu", G_PLUGIN_NAME, output_buffer_size, bytes_read_output);
            free(output_buffer);
            output_buffer = NULL;
            goto cleanup;
        }
        output_buffer[output_buffer_size] = '\0';
    }

    output_chunk = (ProcessedDataChunk*)malloc(sizeof(ProcessedDataChunk));
    if (!output_chunk) {
        LOG_ERROR("[%s] Failed to allocate memory for output chunk.", G_PLUGIN_NAME);
        goto cleanup;
    }
    memset(output_chunk, 0, sizeof(ProcessedDataChunk));

    output_chunk->data = (unsigned char*)output_buffer;
    output_chunk->length = output_buffer_size;
    output_chunk->data_type = CHUNK_TYPE_TEXT_PARSED;

    char source_id_buf[512];
    if (input_chunk->source_id) {
        snprintf(source_id_buf, sizeof(source_id_buf), "%s_parsed_sqlite", input_chunk->source_id);
    } else {
        snprintf(source_id_buf, sizeof(source_id_buf), "unknown_parsed_sqlite_by_%s", G_PLUGIN_NAME);
    }
    output_chunk->source_id = strdup(source_id_buf);
    if (!output_chunk->source_id) {
        LOG_WARN("[%s] Failed to allocate memory for output source_id string.", G_PLUGIN_NAME);
    }

    LOG_INFO("[%s] Successfully parsed SQLite DB data. Output size: %ld bytes.", G_PLUGIN_NAME, output_buffer_size);
    output_buffer = NULL;

cleanup:
    if (fd_input != -1) close(fd_input); // Should be already closed.
    if (fp_output_read) fclose(fp_output_read);

    // Ensure both template_in_for_mkstemp (if different and exists) and actual_input_path_processed are cleaned
    if (template_in_for_mkstemp[0] != '\0' && access(template_in_for_mkstemp, F_OK) != -1 && strcmp(template_in_for_mkstemp, actual_input_path_processed) != 0) {
        unlink(template_in_for_mkstemp);
    }
    if (actual_input_path_processed[0] != '\0' && access(actual_input_path_processed, F_OK) != -1) {
        LOG_INFO("[%s] Deleting temporary input file: %s", G_PLUGIN_NAME, actual_input_path_processed);
        unlink(actual_input_path_processed);
    }
    if (actual_output_path[0] != '\0' && access(actual_output_path, F_OK) != -1) {
        LOG_INFO("[%s] Deleting temporary output file: %s", G_PLUGIN_NAME, actual_output_path);
        unlink(actual_output_path);
    }
    if (output_buffer) {
        free(output_buffer);
    }

    if (!output_chunk) {
        LOG_ERROR("[%s] Processing failed, returning NULL.", G_PLUGIN_NAME);
    }
    return output_chunk;
}

// Plugin dependency management (stub - no dependencies)
SpindexPluginDependency* spindex_plugin_get_dependencies(int* count) {
    if (count) *count = 0;
    return NULL;  // This plugin has no dependencies
}

void spindex_plugin_free_dependencies_list(SpindexPluginDependency* dependencies, int count) {
    // Nothing to free - we returned NULL
    (void)dependencies;
    (void)count;
}

