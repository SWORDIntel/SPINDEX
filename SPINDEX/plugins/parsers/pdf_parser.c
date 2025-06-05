#define _GNU_SOURCE // For strdup, mkstemp
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>

#include "pdf_parser.h"
#include "../../include/utils.h"
#include "../../include/data_types.h" // For CHUNK_TYPE_TEXT_PARSED

static const char* G_PLUGIN_NAME = "PDFParser";
static const char* G_PLUGIN_VERSION = "0.2.0";
static const char* G_PLUGIN_DESCRIPTION = "Parses PDF files using external tool (pdftotext).";

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

    char actual_input_path[] = "/tmp/spindex_pdf_in_XXXXXX"; // Template for mkstemp
    char actual_output_path[] = "/tmp/spindex_pdf_out_XXXXXX"; // Template for mkstemp
    char final_output_path_with_txt[256]; // Buffer for output path + .txt

    int fd_input = -1;
    int fd_output_dummy = -1; // For mkstemp creating the output file name template
    FILE *fp_output_read = NULL;
    ProcessedDataChunk *output_chunk = NULL;
    char *output_buffer = NULL;
    long output_buffer_size = 0;

    // Create unique temporary input file path
    fd_input = mkstemp(actual_input_path);
    if (fd_input == -1) {
        LOG_ERROR("[%s] Failed to create temporary input file with template '%s': %s", G_PLUGIN_NAME, actual_input_path, strerror(errno));
        goto cleanup;
    }
    LOG_INFO("[%s] Created temporary input file: %s", G_PLUGIN_NAME, actual_input_path);

    // Write input chunk data to temporary file
    ssize_t bytes_written = write(fd_input, input_chunk->data, input_chunk->length);
    close(fd_input); // Close immediately after write
    fd_input = -1;   // Mark as closed

    if (bytes_written != (ssize_t)input_chunk->length) {
        LOG_ERROR("[%s] Failed to write all data to temporary input file '%s'. Expected %zu, wrote %zd", G_PLUGIN_NAME, actual_input_path, input_chunk->length, bytes_written);
        goto cleanup;
    }

    // Create a unique name for the output file, pdftotext will create it.
    // We need to append .txt for pdftotext to know to create text.
    // However, pdftotext appends .txt to the given output path if it's not "-",
    // so we just need a base name.
    fd_output_dummy = mkstemp(actual_output_path);
    if (fd_output_dummy == -1) {
        LOG_ERROR("[%s] Failed to create temporary output file name with template '%s': %s", G_PLUGIN_NAME, actual_output_path, strerror(errno));
        goto cleanup;
    }
    close(fd_output_dummy); // Close and remove, we just wanted the name.
    unlink(actual_output_path); // pdftotext will create it based on this name + .txt
                                 // Or, if we give it actual_output_path, it writes to that exact file.
                                 // pdftotext [options] <PDF-file> [<text-file>]
                                 // If <text-file> is '-', text is sent to stdout.
                                 // If <text-file> is omitted, output is 'pdffilename.txt'.
                                 // We want to specify the output path.

    // Construct the final output path with .txt, as pdftotext might not add it if path is fully specified.
    // Or, let pdftotext create actual_output_path directly.
    // Forcing .txt for clarity and ensuring what we read.
    snprintf(final_output_path_with_txt, sizeof(final_output_path_with_txt), "%s.txt", actual_output_path);
    // We will tell pdftotext to write to final_output_path_with_txt
    // Need to ensure actual_output_path (without .txt) is cleaned up if pdftotext does not use it.
    // Simpler: just use actual_output_path directly and pdftotext will write to it.
    // If it needs a .txt extension to produce text, then the template should be actual_output_path.txt.
    // Let's use actual_output_path directly. pdftotext is fine with any output name.

    char command[1024];
    snprintf(command, sizeof(command), "pdftotext %s %s", actual_input_path, actual_output_path);

    LOG_INFO("[%s] Executing command: %s", G_PLUGIN_NAME, command);
    int ret = system(command);
    if (ret != 0) {
        // pdftotext returns 1 for most errors, 2 for output device err, 3 for permission err, 99 for other.
        LOG_ERROR("[%s] Command execution failed with status %d. Command: %s", G_PLUGIN_NAME, ret, command);
        // If pdftotext fails, it might not create the output file, or it might be empty.
        // The fopen check below will handle non-existent output file.
        goto cleanup;
    }

    fp_output_read = fopen(actual_output_path, "rb");
    if (!fp_output_read) {
        LOG_ERROR("[%s] Failed to open pdftotext output file for reading: '%s': %s", G_PLUGIN_NAME, actual_output_path, strerror(errno));
        goto cleanup;
    }

    fseek(fp_output_read, 0, SEEK_END);
    output_buffer_size = ftell(fp_output_read);
     if (output_buffer_size < 0) {
        LOG_ERROR("[%s] Failed to determine size of output file or file is empty: '%s'", G_PLUGIN_NAME, actual_output_path); // This path is correct
        goto cleanup;
    }
    fseek(fp_output_read, 0, SEEK_SET);

    if (output_buffer_size == 0) {
        LOG_WARN("[%s] pdftotext output file is empty: %s", G_PLUGIN_NAME, actual_output_path);
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
        snprintf(source_id_buf, sizeof(source_id_buf), "%s_parsed_pdf", input_chunk->source_id);
    } else {
        snprintf(source_id_buf, sizeof(source_id_buf), "unknown_parsed_pdf_by_%s", G_PLUGIN_NAME);
    }
    output_chunk->source_id = strdup(source_id_buf);
    if (!output_chunk->source_id) {
        LOG_WARN("[%s] Failed to allocate memory for output source_id string.", G_PLUGIN_NAME);
    }

    LOG_INFO("[%s] Successfully parsed PDF data. Output size: %ld bytes.", G_PLUGIN_NAME, output_buffer_size);
    output_buffer = NULL; // Ownership transferred

cleanup:
    if (fd_input != -1) close(fd_input); // Should be already closed if write succeeded
    if (fp_output_read) fclose(fp_output_read);

    if (actual_input_path[0] != '\0' && access(actual_input_path, F_OK) != -1) {
        LOG_INFO("[%s] Deleting temporary input file: %s", G_PLUGIN_NAME, actual_input_path);
        unlink(actual_input_path);
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
