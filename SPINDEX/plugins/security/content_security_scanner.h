#ifndef CONTENT_SECURITY_SCANNER_H
#define CONTENT_SECURITY_SCANNER_H

#include "data_types.h" // For PluginInfo and ProcessedDataChunk

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initializes the content security scanner plugin.
 *
 * Called when the plugin is loaded.
 * @return 0 on success, non-zero on failure.
 */
int content_security_scanner_init();

/**
 * @brief Shuts down the content security scanner plugin.
 *
 * Called when the plugin is unloaded.
 */
void content_security_scanner_shutdown();

/**
 * @brief Gets information about the content security scanner plugin.
 *
 * Populates the provided PluginInfo struct.
 * @param info Pointer to a PluginInfo struct to be filled by the plugin.
 */
void content_security_scanner_get_info(PluginInfo *info);

/**
 * @brief Processes a data chunk with the content security scanner.
 *
 * Scans the input data for security threats like XXE and Zip Bombs.
 * The plugin is responsible for memory management if a new chunk is returned.
 *
 * @param input_chunk A pointer to the ProcessedDataChunk to be scanned.
 * @return A pointer to a ProcessedDataChunk. This can be:
 *         - A new chunk (if data is modified or copied and scan is clean).
 *         - The input_chunk itself (if processed in-place and scan is clean - though the directive implies returning a new chunk).
 *         - NULL if a threat is detected or a fatal error occurs.
 */
ProcessedDataChunk* content_security_scanner_process_data(const ProcessedDataChunk *input_chunk);

#ifdef __cplusplus
}
#endif

#endif // CONTENT_SECURITY_SCANNER_H
