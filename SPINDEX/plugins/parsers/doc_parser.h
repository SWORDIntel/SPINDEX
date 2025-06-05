#ifndef DOC_PARSER_H
#define DOC_PARSER_H

#include "../../include/plugin_interface.h" // For SpindexPlugin, ProcessedDataChunk

// Declare the plugin functions that will be exported from doc_parser.c
// These are the functions dlsym will look for.

/**
 * @brief Get information about the DOC parser plugin.
 * Fills the PluginInfo struct.
 */
void spindex_plugin_get_info(PluginInfo *info);

/**
 * @brief Initialize the DOC parser plugin.
 * @return 0 on success, non-zero on failure.
 */
int spindex_plugin_init();

/**
 * @brief Shutdown the DOC parser plugin.
 */
void spindex_plugin_shutdown();

/**
 * @brief Process DOC data. (Stub implementation)
 * @param input_chunk The input data chunk.
 * @return ProcessedDataChunk* or NULL on failure.
 */
ProcessedDataChunk* spindex_plugin_process_data(const ProcessedDataChunk *input_chunk);

#endif // DOC_PARSER_H
