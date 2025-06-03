# Design: AVX-512 Search and Elasticsearch Integration (Task [ES-003])

## 1. Overview

This document outlines the design for integrating the `avx512_phrase_search.so` plugin with Elasticsearch within the SPINDEX application. The primary goal is to leverage the speed of AVX-512 for specialized phrase searching to enhance or refine search results obtained from Elasticsearch.

## 2. Chosen Integration Strategy: Two-Pass Search (Re-ranking/Filtering)

The selected strategy is a two-pass search approach:

1.  **Pass 1 (Elasticsearch - Broad Search):**
    *   SPINDEX receives a search query that may involve specific phrases or patterns suitable for AVX-512 acceleration.
    *   SPINDEX formulates a broader query to Elasticsearch. This query aims to retrieve a candidate set of documents that are likely to contain the desired information (e.g., based on keyword matches, date ranges, or other filterable attributes).
    *   Elasticsearch returns these candidate documents (or relevant text fields) to SPINDEX.

2.  **Pass 2 (AVX-512 Plugin - Precise Search):**
    *   For each candidate document (or its text content) retrieved from Elasticsearch, SPINDEX invokes the `avx512_phrase_search.so` plugin.
    *   The AVX-512 plugin performs a highly precise and fast search for the specific phrases or patterns on the provided text.
    *   The results from the AVX-512 plugin (e.g., boolean found/not_found, count of occurrences, offsets) are used by SPINDEX to re-rank, filter, or annotate the candidate documents.

**Benefits:**
*   Leverages Elasticsearch's strengths in distributed searching and broad filtering.
*   Utilizes the AVX-512 plugin for CPU-intensive, specialized pattern matching that might be less performant or harder to express in standard Elasticsearch queries.
*   Can significantly improve precision and recall for exact phrase matches or complex proximity searches.

## 3. Addressing `process_data` Input Limitation

The `avx512_phrase_search.so` plugin's `process_data` function, as per the current `plugin_interface.h`, accepts only a single `ProcessedDataChunk*` as input. However, phrase searching requires at least two main inputs:
    a. The primary text to search within.
    b. The phrase(s) to search for.

**Chosen Solution: Pack Inputs into One `ProcessedDataChunk`**

To address this without immediate changes to the core `plugin_interface.h`, the `avx512_phrase_search.so` plugin will expect its single `ProcessedDataChunk* input_chunk` to contain a pointer to a custom structure.

**`AvxSearchParams` Structure Definition:**
(This structure should be defined in a new header file, e.g., `SPINDEX/include/avx_search_params.h`, and included by both the calling code in SPINDEX core and the AVX-512 plugin.)

```c
// File: SPINDEX/include/avx_search_params.h (Example)
#ifndef AVX_SEARCH_PARAMS_H
#define AVX_SEARCH_PARAMS_H

#include "data_types.h" // For ProcessedDataChunk

typedef struct {
    const ProcessedDataChunk* text_to_search;  // The main text body
    const ProcessedDataChunk* phrases_to_find; // Contains one or more phrases (e.g., newline-separated, or structured)
    // Future options:
    // bool case_sensitive;
    // int max_hits_per_doc;
} AvxSearchParams;

#endif // AVX_SEARCH_PARAMS_H
```

**Plugin Usage:**
*   The calling module in SPINDEX (e.g., a search orchestrator) will:
    1.  Populate an `AvxSearchParams` struct with pointers to the relevant data chunks.
    2.  Create a "wrapper" `ProcessedDataChunk`.
    3.  Set `wrapper_chunk->data = &my_avx_search_params_instance;`
    4.  Set `wrapper_chunk->length = sizeof(AvxSearchParams);` (or 0 if data pointer is enough)
    5.  Set `wrapper_chunk->data_type` to a custom type (e.g., `CHUNK_TYPE_AVX_SEARCH_REQUEST`) if `data_types.h` is extended, or rely on plugin convention.
*   The `avx512_phrase_search.c` plugin's `process_data` function will:
    1.  Cast `input_chunk->data` to `const AvxSearchParams*`.
    2.  Access `params->text_to_search` and `params->phrases_to_find` to get the actual data.

## 4. Data Flow for a Search Query (Illustrative)

1.  User submits a query (potentially indicating a desire for precise phrase matching) to SPINDEX.
2.  SPINDEX's search module determines that the AVX-512 path is appropriate.
3.  SPINDEX sends a query to Elasticsearch to fetch candidate documents.
4.  Elasticsearch returns a list of documents.
5.  For each document:
    a.  SPINDEX prepares the document's text content as `text_to_search_chunk`.
    b.  SPINDEX prepares the target phrases as `phrases_to_find_chunk`.
    c.  An `AvxSearchParams` instance is created and populated.
    d.  A wrapper `ProcessedDataChunk` is created, pointing to the `AvxSearchParams`.
    e.  `avx512_phrase_search.so->process_data()` is called with the wrapper chunk.
    f.  The result (e.g., a new `ProcessedDataChunk` indicating matches) is processed.
6.  SPINDEX aggregates results from the AVX-512 plugin (e.g., filters the document list, adds match details).
7.  Final, refined results are presented to the user.

## 5. Output of the AVX-512 Plugin

The `avx512_phrase_search.so` plugin's `process_data` function should return a `ProcessedDataChunk`. The `data` field of this output chunk could contain:
*   A simple boolean status (match found / not found).
*   A count of matches.
*   A structured list of match details (e.g., offsets, lengths).
The exact format will be defined during the detailed implementation of the AVX-512 plugin's search logic.

## 6. Future Considerations

*   **Pre-computation and Metadata Indexing:** For frequently searched critical phrases, the AVX-512 plugin could be used during ingestion (as described in initial brainstorming) to pre-calculate metadata and index it into Elasticsearch. This would allow faster filtering in ES for those specific phrases.
*   **Plugin Interface Modification:** If more plugins require multiple distinct input data chunks, the `plugin_interface.h` could be revisited to add a more flexible processing function signature.
*   **Configuration for AVX Plugin:** The AVX plugin might need its own configuration (e.g., for default search parameters, list of predefined phrases if used in an ingestion-time scenario).

```
