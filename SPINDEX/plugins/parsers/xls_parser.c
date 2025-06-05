#define _GNU_SOURCE // For strdup, mkstemp if needed (though using fixed paths for now)
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>

#include "xls_parser.h"
#include "../../include/utils.h"
#include "../../include/data_types.h" // For CHUNK_TYPE_TEXT_PARSED etc.

static const char* G_PLUGIN_NAME = "XLSParser";
static const char* G_PLUGIN_VERSION = "0.2.0";
static const char* G_PLUGIN_DESCRIPTION = "Parses XLS/XLSX files using external tool (ssconvert).";

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
    // Check if ssconvert is available? Maybe too much for init.
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

    char temp_input_file_path[] = "/tmp/spindex_xls_parser_in_XXXXXX.xls"; // Add .xls for ssconvert
    char temp_output_file_path[] = "/tmp/spindex_xls_parser_out_XXXXXX.txt"; // Explicit .txt output
    int fd_input = -1;
    FILE *fp_output_read = NULL; // For reading the text output
    ProcessedDataChunk *output_chunk = NULL;
    char *output_buffer = NULL;
    long output_buffer_size = 0;

    // Create a unique temporary input file path
    // mkstemp needs a slightly different template format (XXXXXX at the end, not in middle)
    // For simplicity and control, let's use fixed names with pid or just fixed for now,
    // but proper temp file creation is better.
    // Using mkstemp for input name generation.
    char template_in[] = "/tmp/spindex_xls_in_XXXXXX";
    // char template_out[] = "/tmp/spindex_xls_out_XXXXXX.txt"; // Unused

    fd_input = mkstemp(template_in);
    if (fd_input == -1) {
        LOG_ERROR("[%s] Failed to create temporary input file using template %s: %s", G_PLUGIN_NAME, template_in, strerror(errno));
        goto cleanup;
    }
    // We have an fd, now construct the full path for the command.
    // It's already in template_in. We need to ensure it has a .xls suffix for ssconvert if it relies on it.
    // A better approach: write to template_in, then rename to template_in.xls or pass to ssconvert correctly.
    // For now, let's assume ssconvert does not strictly need the suffix on input if it can detect type.
    // Or, more safely, create template_in.xls directly.
    // Let's try creating it with the desired extension directly for ssconvert.
    close(fd_input); // Close it, we'll reopen with standard I/O or pass path to ssconvert.
    unlink(template_in); // Remove the file created by mkstemp without suffix.

    // Reconstruct input path with .xls (or .xlsx - ssconvert handles both)
    // Forcing .xls for simplicity, assuming ssconvert handles the actual format.
    snprintf(temp_input_file_path, sizeof(temp_input_file_path), "%s.xls", template_in);
                                                                    // This reuses the XXXXXX part.

    // Create and write to the input file
    FILE* fp_input = fopen(temp_input_file_path, "wb");
    if (!fp_input) {
        LOG_ERROR("[%s] Failed to open temporary input file '%s' for writing: %s", G_PLUGIN_NAME, temp_input_file_path, strerror(errno));
        goto cleanup;
    }
    size_t bytes_written = fwrite(input_chunk->data, 1, input_chunk->length, fp_input);
    if (bytes_written != input_chunk->length) {
        LOG_ERROR("[%s] Failed to write all data to temporary input file '%s'. Expected %zu, wrote %zu", G_PLUGIN_NAME, temp_input_file_path, input_chunk->length, bytes_written);
        fclose(fp_input);
        goto cleanup;
    }
    fclose(fp_input);
    LOG_INFO("[%s] Successfully wrote %zu bytes to temporary input file: %s", G_PLUGIN_NAME, bytes_written, temp_input_file_path);

    // Generate temp output file path (already has .txt)
    // We don't need to create it with mkstemp if ssconvert creates it.
    // But to be safe, let's ensure it's a unique name that ssconvert can write to.
    // Forcing a specific name for output.txt based on the input template.
    snprintf(temp_output_file_path, sizeof(temp_output_file_path), "%s_output.txt", template_in);


    char command[1024];
    snprintf(command, sizeof(command), "ssconvert %s %s", temp_input_file_path, temp_output_file_path);

    LOG_INFO("[%s] Executing command: %s", G_PLUGIN_NAME, command);
    int ret = system(command);
    if (ret != 0) {
        LOG_ERROR("[%s] Command execution failed with status %d. Command: %s", G_PLUGIN_NAME, ret, command);
        goto cleanup;
    }

    fp_output_read = fopen(temp_output_file_path, "rb");
    if (!fp_output_read) {
        LOG_ERROR("[%s] Failed to open temporary output file for reading: '%s': %s", G_PLUGIN_NAME, temp_output_file_path, strerror(errno));
        goto cleanup;
    }

    fseek(fp_output_read, 0, SEEK_END);
    output_buffer_size = ftell(fp_output_read);
    if (output_buffer_size < 0) { // Error or empty file
        LOG_ERROR("[%s] Failed to determine size of output file or file is empty: '%s'", G_PLUGIN_NAME, temp_output_file_path);
        goto cleanup;
    }
    fseek(fp_output_read, 0, SEEK_SET);

    if (output_buffer_size == 0) {
        LOG_WARN("[%s] Output file is empty: %s", G_PLUGIN_NAME, temp_output_file_path);
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
            free(output_buffer); // free partially allocated buffer
            output_buffer = NULL; // ensure it's null for cleanup logic
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
        snprintf(source_id_buf, sizeof(source_id_buf), "%s_parsed_xls", input_chunk->source_id);
    } else {
        snprintf(source_id_buf, sizeof(source_id_buf), "unknown_parsed_xls_by_%s", G_PLUGIN_NAME);
    }
    output_chunk->source_id = strdup(source_id_buf);
    if (!output_chunk->source_id) {
        LOG_WARN("[%s] Failed to allocate memory for output source_id string.", G_PLUGIN_NAME);
    }

    LOG_INFO("[%s] Successfully parsed XLS/XLSX data. Output size: %ld bytes.", G_PLUGIN_NAME, output_buffer_size);
    output_buffer = NULL; // Ownership transferred

cleanup:
    if (fp_output_read) fclose(fp_output_read);
    if (temp_input_file_path[0] != '\0' && access(temp_input_file_path, F_OK) != -1) {
        LOG_INFO("[%s] Deleting temporary input file: %s", G_PLUGIN_NAME, temp_input_file_path);
        unlink(temp_input_file_path);
    }
    // Construct the original template_in path for deletion if mkstemp made it and it wasn't the same as temp_input_file_path
    if (template_in[0] != '\0' && strcmp(template_in, temp_input_file_path) != 0 && access(template_in, F_OK) != -1) {
         unlink(template_in); // remove original mkstemp file if it exists and is different
    }

    if (temp_output_file_path[0] != '\0' && access(temp_output_file_path, F_OK) != -1) {
        LOG_INFO("[%s] Deleting temporary output file: %s", G_PLUGIN_NAME, temp_output_file_path);
        unlink(temp_output_file_path);
    }
    if (output_buffer) {
        free(output_buffer);
    }

    if (!output_chunk) {
        LOG_ERROR("[%s] Processing failed, returning NULL.", G_PLUGIN_NAME);
    }
    return output_chunk;
}
