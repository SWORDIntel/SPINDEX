**Assumptions:**
*   You're using `make` for the build system.
*   Tasks are for C development primarily.
*   "Implement" means create the `.c` and corresponding `.h` file, compile it as a shared object (`.so`) if it's a plugin, or link it into the core if it's core functionality.

---

## SPINDEX - Jules Task List (Starting from `json_preprocessor.c`)

**Phase 1: Core System & Plugin Infrastructure**
Status: **DONE ✅**

This phase focuses on getting the absolute basics running: the core application, plugin loading, and configuration.

1.  **Task: [CORE-001] Implement `main.c` Entry Point**
    *   Description: Create the main application entry point. Basic argument parsing (e.g., config file path).
    *   Files: `core/main.c`
    *   Priority: High
2.  **Task: [CORE-002] Define Core Data Structures & Types**
    *   Description: Define common data structures (e.g., for processed data chunks, plugin info) in `data_types.h`.
    *   Files: `include/data_types.h`
    *   Priority: High
3.  **Task: [CORE-003] Define Plugin Interface (`plugin_interface.h`)**
    *   Description: Define the C function pointer interfaces for all plugin types (parsers, preprocessors, indexers, etc.). This is crucial for modularity.
    *   Files: `include/plugin_interface.h`
    *   Priority: High
4.  **Task: [CORE-004] Implement Plugin Loader (`plugin_loader.c`)**
    *   Description: Develop the module to scan `plugins/` subdirectories, dynamically load `.so` files (`dlopen`, `dlsym`), and register them based on the `plugin_interface.h`.
    *   Files: `core/plugin_loader.c`, `core/plugin_loader.h`
    *   Priority: High
5.  **Task: [CORE-005] Implement Configuration Manager (`config_manager.c`)**
    *   Description: Develop the module to parse configuration files (e.g., `plugins.cfg`, `pipeline.cfg`, `elasticsearch.cfg`). Define config structures in `config.h`.
    *   Files: `core/config_manager.c`, `core/config_manager.h`, `include/config.h`
    *   Priority: High
6.  **Task: [BUILD-001] Create Initial Makefile**
    *   Description: Set up a basic `Makefile` to compile the core components and link them. Add rules for compiling plugins into `.so` files later.
    *   Files: `Makefile`
    *   Priority: High
7.  **Task: [CORE-006] Implement Basic Logging Utility**
    *   Description: A simple logging mechanism for debug, info, error messages. Could be part of `utils.c`.
    *   Files: `core/utils.c`, `core/utils.h` (or dedicated logging module)
    *   Priority: Medium
8.  **Task: [CORE-007] Implement Hardware Detection (`hardware_detection.c`)**
    *   Description: Basic module to detect CPU capabilities (e.g., AVX-512 support using CPUID).
    *   Files: `core/hardware_detection.c`, `core/hardware_detection.h`
    *   Priority: Medium

**Phase 2: Basic Ingestion Pipeline & First Parser/Preprocessor Pair**
Status: **DONE ✅**

Focus on getting a single file type processed end-to-end (conceptually, actual indexing comes later). We'll leverage the existing `json_preprocessor.c`.

1.  **Task: [INGEST-001] Implement Ingestion Pipeline Manager (`ingestion_pipeline.c`)**
    *   Description: Core logic to manage the data flow: identify file type (initially basic extension), call preprocessor, call parser, prepare for indexer.
    *   Files: `core/ingestion_pipeline.c`, `core/ingestion_pipeline.h`
    *   Priority: High
2.  **Task: [JSON-001] Adapt `json_preprocessor.c` to Plugin Interface**
    *   Description: Modify the existing `json_preprocessor.c` to conform to `plugin_interface.h`. Compile it as `plugins/preprocessors/json_preprocessor.so`.
    *   Files: `plugins/preprocessors/json_preprocessor.c` (and a minimal `.h` if needed for its own functions)
    *   Priority: High
    *   *Status: `json_preprocessor.c` exists, needs adaptation.*
3.  **Task: [JSON-002] Implement `json_parser.c`**
    *   Description: Create the JSON parser plugin. It should take preprocessed data (if any) and parse the JSON structure into a format defined in `data_types.h`.
    *   Files: `plugins/parsers/json_parser.c` (and a minimal `.h`)
    *   Priority: High
    *   Dependencies: [CORE-002], [CORE-003]
4.  **Task: [BUILD-002] Extend Makefile for JSON Plugins**
    *   Description: Add rules to `Makefile` to compile `json_preprocessor.so` and `json_parser.so`.
    *   Files: `Makefile`
    *   Priority: High
5.  **Task: [TEST-001] Test Core JSON Ingestion Path**
    *   Description: Create a test harness or extend `main.c` to load a JSON file, pass it through the `ingestion_pipeline` using the JSON preprocessor and parser plugins, and log/verify the output.
    *   Priority: High

**Phase 3: Elasticsearch Integration & Indexing**
Status: **DONE ✅**

1.  **Task: [ES-001] Implement Elasticsearch Client (`elasticsearch_client.c`)**
    *   Description: Module to handle communication with Elasticsearch (HTTP requests, connection management, sending bulk data). Likely using `libcurl`.
    *   Files: `core/elasticsearch_client.c`, `core/elasticsearch_client.h`
    *   Priority: High
2.  **Task: [ES-002] Implement Elasticsearch Indexer Plugin (`elasticsearch_indexer.so`)**
    *   Description: Plugin that takes parsed data (from `data_types.h` format) and sends it to Elasticsearch via `elasticsearch_client.c` for indexing.
    *   Files: `plugins/indexers/elasticsearch_indexer.c`
    *   Priority: High
    *   Dependencies: [ES-001], [CORE-002], [CORE-003]
3.  **Task: [BUILD-003] Extend Makefile for Elasticsearch Indexer**
    *   Description: Add rules to `Makefile` to compile `elasticsearch_indexer.so`.
    *   Files: `Makefile`
    *   Priority: High
4.  **Task: [TEST-002] End-to-End JSON Indexing Test**
    *   Description: Test indexing a JSON file into a local Elasticsearch instance. Verify data in Elasticsearch/Kibana.
    *   Priority: High

**Phase 4: Expanding Parsers & Preprocessors (Iterative)**
Status: **DONE ✅**

This phase will be repeated for each file type.

*   **Text-Based Files (`.txt`)**
    1.  **Task: [TXT-001] Implement `txt_preprocessor.c`**
        *   Description: Preprocessor for TXT files. Initially simple, later to integrate with ML classification for ambiguous content.
        *   Files: `plugins/preprocessors/txt_preprocessor.c`
        *   Priority: Medium
    2.  **Task: [TXT-002] Implement `txt_parser.c`**
        *   Description: Parser for TXT files. Strategies might vary based on (future) ML classification.
        *   Files: `plugins/parsers/txt_parser.c`
        *   Priority: Medium

*   **CSV Files (`.csv`)**
    1.  **Task: [CSV-001] Implement `csv_preprocessor.c`**
        *   Files: `plugins/preprocessors/csv_preprocessor.c`
        *   Priority: Medium
    2.  **Task: [CSV-002] Implement `csv_parser.c`**
        *   Files: `plugins/parsers/csv_parser.c`
        *   Priority: Medium

*   **SQL Files (`.sql`)**
    1.  **Task: [SQL-001] Implement `sql_preprocessor.c`**
        *   Files: `plugins/preprocessors/sql_preprocessor.c`
        *   Priority: Medium
    2.  **Task: [SQL-002] Implement `sql_parser.c`** (might focus on extracting DDL, DML, comments, etc.)
        *   Files: `plugins/parsers/sql_parser.c`
        *   Priority: Medium

*   **XML Files (`.xml`)**
    1.  **Task: [XML-001] Implement `xml_preprocessor.c`**
        *   Files: `plugins/preprocessors/xml_preprocessor.c`
        *   Priority: Medium
    2.  **Task: [XML-002] Implement `xml_parser.c`** (consider using a library like `libxml2`)
        *   Files: `plugins/parsers/xml_parser.c`
        *   Priority: Medium

*   **YAML Files (`.yml`)**
    1.  **Task: [YML-001] Implement `yml_preprocessor.c`**
        *   Files: `plugins/preprocessors/yml_preprocessor.c`
        *   Priority: Medium
    2.  **Task: [YML-002] Implement `yml_parser.c`** (consider using a library like `libyaml`)
        *   Files: `plugins/parsers/yml_parser.c`
        *   Priority: Medium

*   **HTML Files (`.html`, `.htm`)**
    1.  **Task: [HTML-001] Implement `html_preprocessor.c`**
        *   Files: `plugins/preprocessors/html_preprocessor.c`
        *   Priority: Medium
    2.  **Task: [HTML-002] Implement `html_parser.c`** (extract text content, metadata, links; consider `libxml2` in HTML mode)
        *   Files: `plugins/parsers/html_parser.c`
        *   Priority: Medium

*   **Document Files (DOC/DOCX, XLS/XLSX, PDF, RTF)** - These are more complex and may require external libraries or command-line tools initially.
    1.  **Task: [DOC-001] Research & Implement `doc_parser.c` & `doc_preprocessor.c`** (e.g., using `libdoc`, `antiword`, or shelling out to `pandoc`/`libreoffice`)
    2.  **Task: [XLS-001] Research & Implement `xls_parser.c` & `xls_preprocessor.c`** (e.g., using `libxls`, `libxlsxwriter`, or shelling out)
    3.  **Task: [PDF-001] Research & Implement `pdf_parser.c` & `pdf_preprocessor.c`** (e.g., using `poppler`, `mupdf`, or shelling out to `pdftotext`)
    4.  **Task: [RTF-001] Research & Implement `rtf_parser.c` & `rtf_preprocessor.c`** (e.g., `librtf` or shelling out)
    *   Priority: Low to Medium (due to complexity/external dependencies)

*   **Specialized Formats (`.dic`, `.sqlite/.db`)**
    1.  **Task: [DIC-001] Research & Implement `dic_parser.c` & `dic_preprocessor.c`** (for DICOM; e.g., using `dcmtk` or `Grassroots DICOM (GDCM)`)
        *   Priority: Medium (if medical data is key)
    2.  **Task: [SQLITE-001] Implement `sqlite_parser.c` & `sqlite_preprocessor.c`** (using `sqlite3` library to read schema and data)
        *   Priority: Medium

*   **Task: [BUILD-004] Extend Makefile for All New Parsers/Preprocessors**
    *   Description: Iteratively update Makefile as new plugins are added.
    *   Priority: Medium (ongoing)

*   **General Enhancement: File Encoding Support**
    1.  **Task: [ENC-001] Research and Implement Encoding Detection**
        *   Description: Investigate and integrate a robust library or method for detecting file encodings (e.g., UTF-8, UTF-16, ISO-8859-1, etc.). This might involve updating `ingestion_pipeline.c` or individual preprocessors.
        *   Priority: Medium
    2.  **Task: [ENC-002] Update Parsers/Preprocessors for Encoding Handling**
        *   Description: Modify existing and future text-based parsers and preprocessors to utilize detected encoding information, ensuring correct character processing. This includes `txt_parser/preprocessor`, `csv_parser/preprocessor`, `json_parser/preprocessor`, `xml_parser/preprocessor`, etc.
        *   Priority: Medium (ongoing as new parsers are developed)
    3.  **Task: [ENC-003] Test Encoding Support**
        *   Description: Create test cases with files using various common encodings to verify proper handling.
        *   Priority: Medium

**Phase 5: Decompressors**
Status: **DONE ✅**

1.  **Task: [DECOMP-001] Implement `zip_decompressor.c`** (e.g., using `libzip` or `miniz`) - **DONE ✅**
    *   Files: `plugins/decompressors/zip_decompressor.c`
    *   Priority: Medium
2.  **Task: [DECOMP-002] Implement `gzip_decompressor.c`** (e.g., using `zlib`) - **DONE ✅**
    *   Files: `plugins/decompressors/gzip_decompressor.c`
    *   Priority: Medium
3.  **Task: [DECOMP-003] Implement `zstd_decompressor.c`** (e.g., using `libzstd`) - **DONE ✅**
    *   Files: `plugins/decompressors/zstd_decompressor.c` (Corrected from .so to .c as per actual implementation)
    *   Priority: Medium
4.  **Task: [INGEST-002] Integrate Decompressors into Ingestion Pipeline** - **DONE ✅**
    *   Description: Modify `ingestion_pipeline.c` to detect compressed files and invoke decompressor plugins before preprocessors/parsers.
    *   Priority: Medium
5.  **Task: [BUILD-005] Extend Makefile for Decompressors** - **DONE ✅**
    *   Priority: Medium

**Phase 6: Advanced Features - AVX-512 & Machine Learning**
Status: In Progress

These are significant undertakings.

1.  **Task: [AVX-001] Implement full AVX-512 phrase search algorithm**
    *   Description: Develop the C-based AVX-512 optimized phrase search algorithm as a plugin. This is a highly specialized task.
    *   Files: `plugins/search_algorithms/avx512_phrase_search.c`
    *   Priority: High
    *   Dependencies: [CORE-007]
    *   Status: To Do
2.  **Task: [ES-003] Integrate AVX-512 Search with Elasticsearch Queries**
    *   Description: Design how SPINDEX's AVX-512 search interacts with or enhances Elasticsearch vector search. This might involve custom ES plugins or specific query formations.
    *   Priority: Medium
    *   Status: Design Complete. See `docs/avx_elasticsearch_integration_design.md`.
3.  **Task: [ML-001] Define Machine Learning Interface (`ml_interface.h`)**
    *   Description: Specify C interfaces for loading ML models and performing inference (classification).
    *   Files: `include/ml_interface.h`
    *   Priority: Medium
    *   Status: **DONE ✅**
4.  **Task: [ML-002] Implement ML Classifier Plugin (`ml_classifier.so`)**
    *   Description: A plugin to load and run pre-trained ML models for content classification (e.g., distinguishing types of data within `.txt` files). May use libraries like `oneDNN` or ONNX Runtime C API.
    *   Files: `plugins/classifiers/ml_classifier.c` (Path corrected from preprocessors)
    *   Priority: Medium
    *   Status: **DONE ✅**
5.  **Task: [ML-003] Implement Content Analyzer Plugin (`content_analyzer.so`)**
    *   Description: A plugin for heuristic-based content analysis, potentially as a fallback or supplement to ML classification.
    *   Files: `plugins/analyzers/content_analyzer.c` (Path corrected from preprocessors)
    *   Priority: Medium
    *   Status: **DONE ✅**
6.  **Task: [INGEST-003] Integrate ML Classification into Ingestion Pipeline**
    *   Description: Modify `ingestion_pipeline.c` and relevant preprocessors (e.g., `txt_preprocessor.c`) to use `ml_classifier.so` and `content_analyzer.so` for ambiguous data handling.
    *   Priority: Medium
    *   Status: **DONE ✅**
7.  **Task: [BUILD-006] Extend Makefile for AVX & ML Plugins**
    *   Priority: Medium
    *   Status: **DONE ✅**

*   **Sub-Phase: ML Inference Acceleration with OpenVINO**
    *   Description: Enhance the ML capabilities by integrating Intel's OpenVINO toolkit for optimized inference. The primary goal is to accelerate ML classification tasks by leveraging FP16 precision models on modern hardware, including CPUs, integrated GPUs, and specialized NPUs (e.g., on Intel Meteor Lake 165h processors). The `openvino_classifier.c` plugin implements this using the OpenVINO C API. It expects models pre-converted to FP16 OpenVINO Intermediate Representation (IR) format. Device selection for inference can be automatic (`"AUTO"`) or explicitly specified (e.g., `"NPU"`), with further optimization possible via performance hints. See FP16.MD for detailed analysis and model recommendations.
    *   Priority: Medium-High (Strategic Feature)
    *   Note: Successful deployment and testing of these tasks will be dependent on the availability of target hardware (e.g., Intel Meteor Lake 165h) and corresponding OpenVINO SDK, drivers, and toolchains.

    1.  **Task: [ML-OV-001] Research OpenVINO Integration for SPINDEX**
        *   Description: Investigate the OpenVINO toolkit, its C/C++ APIs for inference, and best practices for integrating it into the SPINDEX plugin architecture. Identify any necessary changes to `ml_interface.h` or the plugin loading mechanism to support OpenVINO-accelerated models alongside other ML backends.
        *   Files: Potentially `include/ml_interface.h`, design documents.
        *   Priority: Medium-High
        *   Status: DONE ✅
    2.  **Task: [ML-OV-002] Implement OpenVINO-enabled ML Classifier Plugin**
        *   Description: Develop a new ML classifier plugin (e.g., `openvino_classifier.c`) or significantly refactor `ml_classifier.c` to support loading and executing models (especially FP16 quantized models) using the OpenVINO Inference Engine. This includes model loading, input preprocessing specific to OpenVINO, and output postprocessing. (Note: Consider DistilBERT or similar efficient transformer models, fine-tuned and converted to FP16 OpenVINO IR format, as detailed in FP16.MD analysis.)
        *   Files: `plugins/classifiers/openvino_classifier.c`, `plugins/classifiers/openvino_classifier.h` (if new), or modifications to existing ML plugin files.
        *   Priority: Medium-High
        *   Dependencies: [ML-OV-001], [ML-001]
        *   Status: DONE ✅
    3.  **Task: [ML-OV-003] Add Build System Support for OpenVINO**
        *   Description: Update the Makefile or build system to correctly detect OpenVINO installations, link against necessary OpenVINO libraries, and compile the OpenVINO-enabled plugin.
        *   Files: `Makefile`, build scripts.
        *   Priority: Medium
        *   Status: DONE ✅
    4.  **Task: [ML-OV-004] Implement FP16 Model Handling and Meteor Lake 165h Optimizations**
        *   Description: Ensure the OpenVINO plugin can correctly load and utilize FP16 precision models. Investigate and implement any specific optimizations or configurations available through OpenVINO for Intel Meteor Lake 165h processors, potentially including NPU offloading if supported and exposed via the OpenVINO C/C++ API.
        *   Files: OpenVINO plugin implementation.
        *   Priority: Medium
        *   Dependencies: [ML-OV-002]
        *   Status: DONE ✅ (Code complete, FP16 model usage integrated; NPU specific tuning is hardware-dependent)
    5.  **Task: [ML-OV-005] Execute hardware-specific testing and validation for OpenVINO FP16 Inference on Meteor Lake 165h**
        *   Description: Conduct thorough testing of the OpenVINO-enabled plugin with suitable FP16 models on a system equipped with an Intel Meteor Lake 165h processor. Verify functional correctness, performance gains, and stability. Compare against non-accelerated inference if possible.
        *   Priority: High
        *   Dependencies: [ML-OV-004], Access to target hardware.
        *   Status: To Do

**Phase 7: Backend Finalization and AEGIS Pipeline Completion**
Status: Pending
(Based on backend/README.md and docs/AEGIS_PIPELINE_DESIGN.md)

1.  **Task: [BE-001] Implement persistent storage for Metadata Catalogue (IngestionJobMetadata and IndexMetadata)**
    *   Description: Replace current in-memory lists for `IngestionJobMetadata` and `IndexMetadata` with a persistent storage solution (e.g., SQLite, PostgreSQL).
    *   Priority: High
    *   Status: To Do
2.  **Task: [BE-002] Implement persistent storage for Query History**
    *   Description: Replace current in-memory list for `QueryHistory` with a persistent storage solution.
    *   Priority: High
    *   Status: To Do
3.  **Task: [BE-003] Complete Content-Level Security Scanning module in AEGIS pipeline**
    *   Description: Implement robust content-level security scanning (Stage 3.4 in AEGIS_PIPELINE_DESIGN.md) for various file types, including checks for XXE in XML-based formats and "zip bomb" characteristics.
    *   Priority: High
    *   Status: To Do
4.  **Task: [BE-004] Complete Schema & Value-Domain Validation module in AEGIS pipeline**
    *   Description: Implement comprehensive schema and value-domain validation (Stage 3.5 in AEGIS_PIPELINE_DESIGN.md), including checks for required attributes, data types, and coordinate value domains. Allow for user-defined schemas.
    *   Priority: High
    *   Status: To Do
5.  **Task: [BE-005] Implement Unstructured Text Parsing (NLP/NER) for Location Extraction in AEGIS pipeline**
    *   Description: Implement Stage 3.8 from AEGIS_PIPELINE_DESIGN.md, using NLP/NER (e.g., spaCy) to extract location names from text and integrate with the geocoding module.
    *   Priority: High
    *   Status: To Do
6.  **Task: [BE-006] Implement robust error handling and user notification for AEGIS pipeline failures**
    *   Description: Enhance error handling within the AEGIS pipeline, provide clearer error messages in `IngestionJobMetadata`, and establish a user notification system for failures (e.g., via GUI alerts or email).
    *   Priority: Medium
    *   Status: To Do
7.  **Task: [BE-007] Testing and Refinement of Backend and AEGIS pipeline**
    *   Description: Conduct thorough testing of all backend endpoints, Celery tasks, and AEGIS pipeline stages with diverse datasets and error conditions. Refine based on test outcomes.
    *   Priority: High
    *   Status: To Do

**Phase 8: GUI Development**
Status: Pending
(Based on GUI_DESIGN.md and gui/GUI_MOCKUPS.md)

1.  **Task: [GUI-001] Implement Dashboard View**
    *   Description: Develop the main dashboard UI components as per `gui/GUI_MOCKUPS.md`.
    *   Priority: Medium
    *   Status: To Do
2.  **Task: [GUI-002] Implement Index Management View (including wizard integration)**
    *   Description: Develop the Index Management UI, including the table for listing indexes and integrating the "Enhanced Index Creation Wizard" modal. Backend interaction for `GET /indexes`, `DELETE /indexes/{index_id}`.
    *   Priority: Medium
    *   Status: To Do
3.  **Task: [GUI-003] Implement Query Interface View**
    *   Description: Develop the Query Interface UI, including index selection, query type selection, dynamic parameter inputs, results display, and query history. Backend interaction for `POST /query/execute`, `GET /query/history`, and `GET /indexes?status=ready`.
    *   Priority: Medium
    *   Status: To Do
4.  **Task: [GUI-004] Implement Data Visualization View (basic map and layer controls)**
    *   Description: Develop the Data Visualization UI with a map display and basic layer/style controls.
    *   Priority: Medium
    *   Status: To Do
5.  **Task: [GUI-005] Implement Settings View (basic structure and user preferences)**
    *   Description: Develop the Settings UI with initial tabs for Data Sources and User Preferences.
    *   Priority: Medium
    *   Status: To Do
6.  **Task: [GUI-006] Implement Ingestion Dashboard View (file upload, job listing)**
    *   Description: Develop the Ingestion Dashboard UI, including file upload functionality (`POST /upload/data`), and tables for Ingestion Jobs and Quarantined Jobs (`GET /metadata/ingestion_jobs`).
    *   Priority: Medium
    *   Status: To Do
7.  **Task: [GUI-007] Implement Enhanced Index Creation Wizard (all steps)**
    *   Description: Fully implement the multi-step "Enhanced Index Creation Wizard" modal, including fetching processed data (`GET /metadata/ingestion_jobs?processing_status=validation_successful_ready_for_promotion`), displaying validation summaries, collecting index parameters, and submitting for creation (`POST /indexes`).
    *   Priority: Medium
    *   Dependencies: [GUI-002]
    *   Status: To Do
8.  **Task: [GUI-008] Implement GUI Backend API Interaction (connecting UI elements to backend endpoints in `script.js`)**
    *   Description: Ensure all JavaScript functions in `script.js` correctly call backend API endpoints, handle responses, and update the UI accordingly for all implemented views.
    *   Priority: High
    *   Status: To Do (Ongoing with each view)
9.  **Task: [GUI-009] Implement UI feedback mechanisms (loading indicators, error messages, help tooltips)**
    *   Description: Add loading spinners/messages for API calls, clear error messages for users, and helpful tooltips for complex UI elements as described in mockups.
    *   Priority: Medium
    *   Status: To Do (Ongoing with each view)
10. **Task: [GUI-010] Testing and Refinement of GUI components.**
    *   Description: Conduct usability testing, cross-browser checks, and functional testing of all GUI views and interactions. Refine based on feedback.
    *   Priority: High
    *   Status: To Do

**Phase 9: Optimizations, Security, and Final System Touches**
Status: Pending

1.  **Task: [OPT-001] Implement general hardware optimization plugins**
    *   Description: If specific CPU-bound tasks in parsers/preprocessors can be optimized with SIMD (beyond the dedicated AVX-512 search), create plugins for these.
    *   Files: `plugins/optimizations/`
    *   Priority: Low
    *   Status: To Do
2.  **Task: [SEC-001] Implement security analysis plugins (e.g., hash_separator.so, password_analyzer.so)**
    *   Description: Develop plugins for hash separation, password analysis, etc.
    *   Files: `plugins/security/`
    *   Priority: Low
    *   Status: To Do
3.  **Task: [CORE-008] Implement Threading Model (`threading.c`)**
    *   Description: Implement multi-threading for parallel processing of files or chunks within the ingestion pipeline.
    *   Files: `core/threading.c`, `core/threading.h`
    *   Priority: Medium
    *   Status: To Do
4.  **Task: [DOCS-001] Write `README.md` and Basic Usage Documentation**
    *   Files: `README.md`, `docs/`
    *   Priority: Medium (ongoing)
    *   Status: In Progress
5.  **Task: [TEST-003] Execute comprehensive system and performance testing**
    *   Description: Test with large datasets, various file types, measure throughput, and identify bottlenecks.
    *   Priority: Medium
    *   Status: To Do

---

**Chunk Reorganization (Reflecting New Phases and Task Updates)**

This section provides a high-level view, grouping phases into larger chunks.

**Chunk 1: Core System & Plugin Foundation**
Status: **DONE ✅**
*(Corresponds to Phase 1)*

**Chunk 2: Basic Ingestion Pipeline & JSON Processing**
Status: **DONE ✅**
*(Corresponds to Phase 2)*

**Chunk 3: Elasticsearch Integration**
Status: **DONE ✅**
*(Corresponds to Phase 3)*

**Chunk 4: Expanding Parsers & Preprocessors**
Status: **DONE ✅**
*(Corresponds to Phase 4)*

**Chunk 5: Decompressor Plugins**
Status: **DONE ✅**
*(Corresponds to Phase 5)*

**Chunk 6: Advanced Features - AVX-512 & Machine Learning**
Status: **In Progress**
*(Corresponds to Phase 6)*
*   Key Pending Tasks: [AVX-001] Full Implementation (High), [ML-OV-005] Meteor Lake Testing (High).

**Chunk 7: Backend Finalization and AEGIS Pipeline Completion**
Status: **Pending**
*(Corresponds to Phase 7 - New)*
*   Key Tasks: [BE-001] Persistent Storage (High), [BE-002] Persistent Query History (High), [BE-003] Security Scanning (High), [BE-004] Schema Validation (High), [BE-005] NLP/NER Implementation (High), [BE-006] Error Handling (Medium), [BE-007] Backend Testing (High).

**Chunk 8: GUI Development**
Status: **Pending**
*(Corresponds to Phase 8 - New)*
*   Key Tasks: [GUI-001] Dashboard (Medium), [GUI-002] Index Mgt (Medium), [GUI-003] Query Interface (Medium), [GUI-006] Ingestion (Medium), [GUI-007] Index Wizard (Medium), [GUI-008] API Interaction (High), [GUI-010] GUI Testing (High).

**Chunk 9: System Enhancements & Finalization**
Status: **Pending**
*(Corresponds to Phase 9 - Previously Phase 7 / Chunk 10, now incorporating remaining and updated tasks)*
*   Key Tasks: [OPT-001] Hardware Opt. Plugins (Low), [SEC-001] Security Analysis Plugins (Low), [CORE-008] Threading (Medium), [DOCS-001] Documentation (Medium), [TEST-003] System Testing (Medium).

---

This breakdown should provide smaller, more manageable sets of tasks for Jules. Each chunk aims for a tangible step forward. You can tackle these chunks sequentially or, where dependencies allow, some in parallel.
(Note: Original Chunk 7-10 have been re-mapped to align with the new phase structure and updates.)
