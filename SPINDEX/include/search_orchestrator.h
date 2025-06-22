#ifndef SEARCH_ORCHESTRATOR_H
#define SEARCH_ORCHESTRATOR_H

#include <stdbool.h>
#include <stddef.h>
#include <time.h> // For time_t

// SPINDEX specific headers
#include "plugin_interface.h" // For SpindexPlugin (adjust path if necessary from SPINDEX/include)
#include "config.h"           // For SpindexConfig (adjust path)
#include "data_types.h"       // For ProcessedDataChunk (adjust path)

// Forward declaration for the SearchOrchestrator structure
typedef struct SearchOrchestrator SearchOrchestrator;

// UserQuery structure for submitting search requests
typedef struct {
    const char* main_query_text; // For broader ES search
    const char* avx_phrases;     // Newline-separated phrases for AVX search
    time_t date_start;           // Optional start date for filtering (0 if not used)
    time_t date_end;             // Optional end date for filtering (0 if not used)
    const char* filter_string;   // Optional additional filter criteria (e.g., "type:report AND author:smith")
    // Consider adding pagination parameters: int page_number; int results_per_page;
} UserQuery;

// SearchResultItem structure for individual search hits
typedef struct {
    char* document_id;               // Unique identifier for the document
    char* document_content_preview;  // A snippet or relevant part of the content
    double score;                    // Relevance score (e.g., from Elasticsearch)
    bool avx_match_found;            // True if any of the AVX phrases were found in this item
    char* avx_match_details;         // Detailed string from AVX plugin (e.g., "Found: phrase1\nNot Found: phrase2")
    char* file_path;                 // Original file path or source identifier
    char* document_title;            // Title of the document, if available
    time_t last_modified_date;       // Last modified timestamp of the document (0 if not available)
    char* file_type;                 // Detected file type (e.g., "PDF", "TXT")
    size_t file_size;                // Size of the file in bytes (0 if not available)
    // Consider adding fields like: author, keywords, direct_url_to_hit, etc.
} SearchResultItem;

// SearchResults structure to hold a collection of search results
typedef struct {
    SearchResultItem* items; // Array of search result items
    size_t count;            // Number of items currently in the array
    size_t capacity;         // Allocated capacity of the array
} SearchResults;

/**
 * @brief Creates and initializes the search orchestrator.
 *
 * The orchestrator is responsible for coordinating search operations, including
 * interactions with Elasticsearch and specialized search plugins like AVX search.
 *
 * @param all_loaded_plugins Pointer to the array of all loaded Spindex plugins.
 * @param num_all_plugins Total number of loaded plugins.
 * @param config Pointer to the global Spindex application configuration.
 * @return SearchOrchestrator* A pointer to the created search orchestrator, or NULL on error.
 */
SearchOrchestrator* search_orchestrator_create(
    SpindexPlugin *all_loaded_plugins,
    int num_all_plugins,
    const SpindexConfig *config
);

/**
 * @brief Executes a search query using the configured two-pass search strategy.
 *
 * This typically involves a broad search (e.g., Elasticsearch) followed by a precise
 * secondary search (e.g., AVX) on the results from the first pass.
 *
 * @param orchestrator The search orchestrator instance.
 * @param query The user's search query.
 * @return SearchResults* A pointer to a SearchResults structure containing the findings.
 *                        The caller is responsible for freeing this structure using
 *                        search_results_destroy(). Returns NULL on error.
 */
SearchResults* search_orchestrator_execute_query(
    SearchOrchestrator* orchestrator,
    const UserQuery* query
);

/**
 * @brief Frees the SearchResults structure and all its contents.
 *
 * This includes freeing memory for each SearchResultItem and their string fields.
 *
 * @param results The SearchResults structure to destroy.
 */
void search_results_destroy(SearchResults* results);

/**
 * @brief Destroys the search orchestrator and frees its associated resources.
 *
 * @param orchestrator The search orchestrator to destroy.
 */
void search_orchestrator_destroy(SearchOrchestrator* orchestrator);

#endif // SEARCH_ORCHESTRATOR_H
