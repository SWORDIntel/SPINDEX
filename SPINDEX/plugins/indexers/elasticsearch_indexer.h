#ifndef ELASTICSEARCH_INDEXER_H
#define ELASTICSEARCH_INDEXER_H

// Forward declaration, actual definitions in plugin_interface.h and data_types.h
struct SpindexConfig;
struct PluginInfo;
struct ProcessedDataChunk;

// The actual plugin interface functions (spindex_plugin_get_info, spindex_plugin_init, etc.)
// are expected by the plugin_loader using generic names.
// This header is for any internal declarations specific to this plugin, if it grew complex.
// For now, it's minimal. The implementations in elasticsearch_indexer.c will use the
// standard names like "spindex_plugin_init", "spindex_plugin_get_info", etc.

// If this plugin had unique exported functions beyond the standard interface, they would go here.

#endif // ELASTICSEARCH_INDEXER_H
