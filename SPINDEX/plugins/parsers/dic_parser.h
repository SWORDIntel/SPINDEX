#ifndef DIC_PARSER_H
#define DIC_PARSER_H

#include "../../include/plugin_interface.h"

void spindex_plugin_get_info(PluginInfo *info);
int spindex_plugin_init();
void spindex_plugin_shutdown();
ProcessedDataChunk* spindex_plugin_process_data(const ProcessedDataChunk *input_chunk);

#endif // DIC_PARSER_H
