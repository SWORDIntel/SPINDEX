#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h> // For errno

#include "doc_parser.h"
#include "../../include/utils.h" // For LOG_INFO, LOG_ERROR etc.
// data_types.h and plugin_interface.h are included via doc_parser.h indirectly or directly

static const char* G_PLUGIN_NAME = "DOCParser";
static const char* G_PLUGIN_VERSION = "0.2.0";
static const char* G_PLUGIN_DESCRIPTION = "Parses DOC/DOCX files using external tools (antiword/docx2txt).";

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

    char temp_input_file_path[] = "/tmp/spindex_doc_parser_in_XXXXXX";
    char temp_output_file_path[] = "/tmp/spindex_doc_parser_out_XXXXXX";
    int fd_input = -1;
    int fd_output = -1;
    FILE *fp_output = NULL;
    ProcessedDataChunk *output_chunk = NULL;
    char *output_buffer = NULL;
    long output_buffer_size = 0;

    fd_input = mkstemp(temp_input_file_path);
    if (fd_input == -1) {
        LOG_ERROR("[%s] Failed to create temporary input file: %s", G_PLUGIN_NAME, strerror(errno));
        goto cleanup;
    }
    LOG_INFO("[%s] Created temporary input file: %s", G_PLUGIN_NAME, temp_input_file_path);

    ssize_t bytes_written = write(fd_input, input_chunk->data, input_chunk->length);
    if (bytes_written != input_chunk->length) {
        LOG_ERROR("[%s] Failed to write all data to temporary input file. Expected %zu, wrote %zd", G_PLUGIN_NAME, input_chunk->length, bytes_written);
        goto cleanup;
    }
    close(fd_input); // Close now, command will reopen by path
    fd_input = -1; // Mark as closed

    fd_output = mkstemp(temp_output_file_path);
    if (fd_output == -1) {
        LOG_ERROR("[%s] Failed to create temporary output file: %s", G_PLUGIN_NAME, strerror(errno));
        goto cleanup;
    }
    LOG_INFO("[%s] Created temporary output file: %s", G_PLUGIN_NAME, temp_output_file_path);
    close(fd_output); // Close descriptor, system() command will handle file by path
    fd_output = -1; // Mark as closed

    char command[1024];
    const char *extension = strrchr(input_chunk->source_id ? input_chunk->source_id : "", '.');
    int is_docx = 0;
    if (extension && strcmp(extension, ".docx") == 0) {
        is_docx = 1;
    }

    if (is_docx) {
        snprintf(command, sizeof(command), "docx2txt %s - > %s", temp_input_file_path, temp_output_file_path);
        LOG_INFO("[%s] Using docx2txt for %s", G_PLUGIN_NAME, input_chunk->source_id ? input_chunk->source_id : "unknown source");
    } else {
        snprintf(command, sizeof(command), "antiword %s > %s", temp_input_file_path, temp_output_file_path);
        LOG_INFO("[%s] Using antiword for %s (assuming .doc or unknown)", G_PLUGIN_NAME, input_chunk->source_id ? input_chunk->source_id : "unknown source");
    }

    LOG_INFO("[%s] Executing command: %s", G_PLUGIN_NAME, command);
    int ret = system(command);
    if (ret != 0) {
        LOG_ERROR("[%s] Command execution failed with status %d. Command: %s", G_PLUGIN_NAME, ret, command);
        // Consider logging stderr of the command if possible, for now, this is good.
        goto cleanup;
    }

    fp_output = fopen(temp_output_file_path, "rb");
    if (!fp_output) {
        LOG_ERROR("[%s] Failed to open temporary output file for reading: %s", G_PLUGIN_NAME, temp_output_file_path);
        goto cleanup;
    }

    fseek(fp_output, 0, SEEK_END);
    output_buffer_size = ftell(fp_output);
    fseek(fp_output, 0, SEEK_SET);

    if (output_buffer_size < 0) {
        LOG_ERROR("[%s] Failed to determine size of output file or file is empty: %s", G_PLUGIN_NAME, temp_output_file_path);
        goto cleanup;
    }
    // Allow empty output, some docs might be empty
    if (output_buffer_size == 0) {
        LOG_WARN("[%s] Output file is empty: %s", G_PLUGIN_NAME, temp_output_file_path);
        output_buffer = NULL; // Explicitly set to NULL for empty files
    } else {
        output_buffer = (char*)malloc(output_buffer_size + 1); // +1 for null terminator if text
        if (!output_buffer) {
            LOG_ERROR("[%s] Failed to allocate memory for output buffer (%ld bytes).", G_PLUGIN_NAME, output_buffer_size);
            goto cleanup;
        }
        size_t bytes_read = fread(output_buffer, 1, output_buffer_size, fp_output);
        if (bytes_read != (size_t)output_buffer_size) {
            LOG_ERROR("[%s] Failed to read all data from temporary output file. Expected %ld, read %zu", G_PLUGIN_NAME, output_buffer_size, bytes_read);
            goto cleanup;
        }
        output_buffer[output_buffer_size] = '\0'; // Null-terminate, useful for text
    }


    output_chunk = (ProcessedDataChunk*)malloc(sizeof(ProcessedDataChunk));
    if (!output_chunk) {
        LOG_ERROR("[%s] Failed to allocate memory for output chunk.", G_PLUGIN_NAME);
        goto cleanup;
    }
    memset(output_chunk, 0, sizeof(ProcessedDataChunk)); // Initialize to zero

    output_chunk->data = (unsigned char*)output_buffer;
    output_chunk->length = output_buffer_size;
    output_chunk->data_type = CHUNK_TYPE_TEXT_PARSED; // Using existing enum value

    char source_id_buf[512];
    if (input_chunk->source_id) {
        snprintf(source_id_buf, sizeof(source_id_buf), "%s_parsed_by_%s", input_chunk->source_id, G_PLUGIN_NAME);
    } else {
        snprintf(source_id_buf, sizeof(source_id_buf), "unknown_parsed_by_%s", G_PLUGIN_NAME);
    }
    output_chunk->source_id = strdup(source_id_buf);
    if (!output_chunk->source_id) {
        LOG_WARN("[%s] Failed to allocate memory for output source_id string.", G_PLUGIN_NAME);
        // Non-fatal, can proceed without source_id or with a default
    }

    LOG_INFO("[%s] Successfully parsed data. Output size: %ld bytes.", G_PLUGIN_NAME, output_buffer_size);
    output_buffer = NULL; // Ownership transferred to output_chunk

cleanup:
    if (fd_input != -1) close(fd_input);
    if (fd_output != -1) close(fd_output); // Should be already closed if mkstemp succeeded.
    if (fp_output) fclose(fp_output);
    if (temp_input_file_path[0] != '\0' && access(temp_input_file_path, F_OK) != -1) {
        LOG_INFO("[%s] Deleting temporary input file: %s", G_PLUGIN_NAME, temp_input_file_path);
        unlink(temp_input_file_path);
    }
    if (temp_output_file_path[0] != '\0' && access(temp_output_file_path, F_OK) != -1) {
        LOG_INFO("[%s] Deleting temporary output file: %s", G_PLUGIN_NAME, temp_output_file_path);
        unlink(temp_output_file_path);
    }
    if (output_buffer) { // If output_chunk allocation failed or other error before transfer
        free(output_buffer);
    }

    if (!output_chunk) { // Ensure cleanup if we are returning NULL due to error
        LOG_ERROR("[%s] Processing failed, returning NULL.", G_PLUGIN_NAME);
    }
    return output_chunk;
}
