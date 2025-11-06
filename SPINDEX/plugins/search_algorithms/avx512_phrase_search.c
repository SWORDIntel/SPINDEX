#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdbool.h>
#include <ctype.h>
#include <immintrin.h>

#include "../../include/plugin_interface.h"
#include "../../include/data_types.h"
#include "../../include/hardware_detection.h"
#include "../../include/avx_search_params.h"
#include "../../include/utils.h"

// --- Helper an Structs ---
typedef struct {
    char *buffer;
    size_t length;
    size_t capacity;
} DynamicBuffer;

// --- Forward declarations ---
void avx512_get_info(PluginInfo *info);
int avx512_init();
void avx512_shutdown();
ProcessedDataChunk* avx512_process_data(const ProcessedDataChunk *input_chunk);
static bool db_init(DynamicBuffer *db, size_t initial_capacity);
static void db_free(DynamicBuffer *db);
static void db_append_str(DynamicBuffer *db, const char *str);
static void db_append_char(DynamicBuffer *db, char c);
static void escape_json_string(const char *input, DynamicBuffer *db);

// --- Exposed plugin entry points ---
void spindex_plugin_get_info(PluginInfo *info) { avx512_get_info(info); }
int spindex_plugin_init() { return avx512_init(); }
void spindex_plugin_shutdown() { avx512_shutdown(); }
ProcessedDataChunk* spindex_plugin_process(const ProcessedDataChunk *input_chunk) { return avx512_process_data(input_chunk); }

// --- Plugin Implementation ---

void avx512_get_info(PluginInfo *info) {
    if (!info) return;
    strncpy(info->name, "AVX-512 Phrase Searcher", sizeof(info->name) - 1);
    info->name[sizeof(info->name) - 1] = '\0';
    strncpy(info->version, "0.2", sizeof(info->version) - 1);
    info->version[sizeof(info->version) - 1] = '\0';
    info->type = SPINDEX_PLUGIN_TYPE_OTHER; 
    strncpy(info->description, "Performs multi-token phrase search using AVX-512 with case-sensitivity, occurrence, and result limit options.", sizeof(info->description) - 1);
    info->description[sizeof(info->description) - 1] = '\0';
}

int avx512_init() {
    CpuFeatures features;
    if (detect_cpu_features(&features) != 0) {
        LOG_ERROR("AVX-512 Plugin: Failed to detect CPU features.");
        return -1;
    }
    if (!features.has_avx512f || !features.has_avx512bw) {
        LOG_WARN("AVX-512 Plugin: CPU does not support required AVX-512F and AVX-512BW features. Plugin will operate in fallback mode.");
    } else {
        LOG_INFO("AVX-512 Phrase Searcher plugin initialized successfully (AVX-512F & AVX-512BW detected).");
    }
    return 0;
}

void avx512_shutdown() {
    LOG_INFO("AVX-512 Phrase Searcher plugin shutting down.");
}

// Mark this function to use AVX512 instructions
__attribute__((target("avx512f,avx512bw")))
const char* avx512_find_substring(const char* text, size_t text_len, const char* phrase, size_t phrase_len) {
    if (phrase_len == 0) return NULL;

    const __m512i first_char = _mm512_set1_epi8(phrase[0]);
    const __m512i last_char = _mm512_set1_epi8(phrase[phrase_len - 1]);

    for (size_t i = 0; i <= text_len - phrase_len; i += 64) {
        size_t chunk_size = (i + 64 <= text_len) ? 64 : text_len - i;
        if (chunk_size < phrase_len) break;

        __m512i text_chunk_first = _mm512_loadu_si512(text + i);
        __m512i text_chunk_last = _mm512_loadu_si512(text + i + phrase_len - 1);

        __mmask64 first_char_mask = _mm512_cmpeq_epi8_mask(first_char, text_chunk_first);
        __mmask64 last_char_mask = _mm512_cmpeq_epi8_mask(last_char, text_chunk_last);

        __mmask64 combined_mask = first_char_mask & last_char_mask;

        int offset = __builtin_ffsll(combined_mask);
        while (offset > 0) {
            int pos = i + offset - 1;
            if (pos <= text_len - phrase_len) {
                if (memcmp(text + pos, phrase, phrase_len) == 0) {
                    return text + pos;
                }
            }
            combined_mask &= (combined_mask - 1);
            offset = __builtin_ffsll(combined_mask);
        }
    }
    return NULL;
}

ProcessedDataChunk* avx512_process_data(const ProcessedDataChunk *input_chunk) {
    if (!input_chunk || !input_chunk->data) {
        LOG_ERROR("AVX Search: Input chunk or its data is NULL.");
        return NULL;
    }
    const AvxSearchParams* params = (const AvxSearchParams*)input_chunk->data;
    if (!params || !params->text_to_search || !params->text_to_search->data ||
        !params->phrases_to_find || !params->phrases_to_find->data) {
        LOG_ERROR("AVX Search: Invalid search parameters, text, or phrases data.");
        return NULL;
    }

    const char* text_to_search = (const char*)params->text_to_search->data;

    char* phrases_str_copy = strdup((const char*)params->phrases_to_find->data);
    if (!phrases_str_copy) {
        LOG_ERROR("AVX Search: Failed to allocate memory for phrases copy.");
        return NULL;
    }

    DynamicBuffer json_buffer;
    if (!db_init(&json_buffer, 4096)) {
        LOG_ERROR("AVX Search: Failed to initialize JSON dynamic buffer.");
        free(phrases_str_copy);
        return NULL;
    }
    db_append_str(&json_buffer, "{\"search_results\": [");

    bool first_phrase_in_json = true;
    char* saveptr;
    char* current_phrase_str = strtok_r(phrases_str_copy, "\n", &saveptr);

    while (current_phrase_str != NULL) {
        size_t len = strlen(current_phrase_str);
        while (len > 0 && (current_phrase_str[len - 1] == '\n' || current_phrase_str[len - 1] == '\r')) {
            current_phrase_str[--len] = '\0';
        }
        if (len == 0) {
            current_phrase_str = strtok_r(NULL, "\n", &saveptr);
            continue;
        }

        if (!first_phrase_in_json) db_append_char(&json_buffer, ',');
        db_append_str(&json_buffer, "{\"phrase\": \"");
        escape_json_string(current_phrase_str, &json_buffer);
        db_append_str(&json_buffer, "\", \"offsets\": [");

        DynamicBuffer offsets_buffer;
        db_init(&offsets_buffer, 256);
        bool first_offset = true;
        int hits = 0;
        const char* search_start = text_to_search;
        while (true) {
            const char* match;
            CpuFeatures features;
            detect_cpu_features(&features);
            if (params->case_sensitive && features.has_avx512f && features.has_avx512bw) {
                match = avx512_find_substring(search_start, strlen(search_start), current_phrase_str, strlen(current_phrase_str));
            } else if (params->case_sensitive) {
                match = strstr(search_start, current_phrase_str);
            } else {
                match = strcasestr(search_start, current_phrase_str);
            }

            if (!match) break;

            if (!first_offset) db_append_char(&offsets_buffer, ',');
            char offset_buf[21];
            snprintf(offset_buf, sizeof(offset_buf), "%ld", match - text_to_search);
            db_append_str(&offsets_buffer, offset_buf);

            first_offset = false;
            hits++;

            if (!params->find_all_occurrences || (params->max_hits_per_doc > 0 && hits >= params->max_hits_per_doc)) {
                break;
            }
            search_start = match + 1;
        }

        db_append_str(&json_buffer, offsets_buffer.buffer);
        db_free(&offsets_buffer);
        db_append_str(&json_buffer, "]}");
        first_phrase_in_json = false;

        current_phrase_str = strtok_r(NULL, "\n", &saveptr);
    }

    free(phrases_str_copy);
    db_append_str(&json_buffer, "]}");

    ProcessedDataChunk* output_chunk = (ProcessedDataChunk*)malloc(sizeof(ProcessedDataChunk));
    if (!output_chunk) {
        LOG_ERROR("AVX Search: Failed to allocate output chunk.");
        db_free(&json_buffer);
        return NULL;
    }
    output_chunk->data = (unsigned char*)json_buffer.buffer;
    output_chunk->length = json_buffer.length;
    output_chunk->source_id = params->text_to_search->source_id ? strdup(params->text_to_search->source_id) : NULL;
    output_chunk->data_type = CHUNK_TYPE_PARSED_JSON;

    return output_chunk;
}


// --- Helper Implementations ---
static bool db_init(DynamicBuffer *db, size_t initial_capacity) {
    db->buffer = malloc(initial_capacity);
    if (!db->buffer) return false;
    db->buffer[0] = '\0';
    db->length = 0;
    db->capacity = initial_capacity;
    return true;
}

static bool db_ensure_space(DynamicBuffer *db, size_t needed) {
    if (db->length + needed < db->capacity) return true;
    size_t new_capacity = db->capacity * 2;
    while (new_capacity < db->length + needed) new_capacity *= 2;
    char *new_buffer = realloc(db->buffer, new_capacity);
    if (!new_buffer) return false;
    db->buffer = new_buffer;
    db->capacity = new_capacity;
    return true;
}

static void db_append_str(DynamicBuffer *db, const char *str) {
    size_t len = strlen(str);
    if (!db_ensure_space(db, len + 1)) return;
    strcat(db->buffer, str);
    db->length += len;
}

static void db_append_char(DynamicBuffer *db, char c) {
    if (!db_ensure_space(db, 2)) return;
    db->buffer[db->length++] = c;
    db->buffer[db->length] = '\0';
}

static void db_free(DynamicBuffer *db) {
    if (db->buffer) free(db->buffer);
    db->buffer = NULL;
    db->length = db->capacity = 0;
}

static void escape_json_string(const char *input, DynamicBuffer *db) {
    for (const char *p = input; *p; p++) {
        switch (*p) {
            case '"':  db_append_str(db, "\\\""); break;
            case '\\': db_append_str(db, "\\\\"); break;
            case '\b': db_append_str(db, "\\b"); break;
            case '\f': db_append_str(db, "\\f"); break;
            case '\n': db_append_str(db, "\\n"); break;
            case '\r': db_append_str(db, "\\r"); break;
            case '\t': db_append_str(db, "\\t"); break;
            default:
                if ((unsigned char)*p < 32) {
                    char buf[7];
                    snprintf(buf, sizeof(buf), "\\u%04x", (unsigned char)*p);
                    db_append_str(db, buf);
                } else {
                    db_append_char(db, *p);
                }
                break;
        }
    }
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
