**Assumptions:**
*   You're using `make` for the build system.
*   Tasks are for C development primarily.
*   "Implement" means create the `.c` and corresponding `.h` file, compile it as a shared object (`.so`) if it's a plugin, or link it into the core if it's core functionality.

---

## SPINDEX - Jules Task List (Starting from `json_preprocessor.c`)

**Phase 1: Core System & Plugin Infrastructure**
Status: **DONE ✅**
*(No changes in this phase)*

1.  **Task: [CORE-001] Implement `main.c` Entry Point**
    *   Priority: High
2.  **Task: [CORE-002] Define Core Data Structures & Types**
    *   Priority: High
3.  **Task: [CORE-003] Define Plugin Interface (`plugin_interface.h`)**
    *   Priority: High
4.  **Task: [CORE-004] Implement Plugin Loader (`plugin_loader.c`)**
    *   Priority: High
5.  **Task: [CORE-005] Implement Configuration Manager (`config_manager.c`)**
    *   Priority: High
6.  **Task: [BUILD-001] Create Initial Makefile**
    *   Priority: High
7.  **Task: [CORE-006] Implement Basic Logging Utility**
    *   Priority: Medium
8.  **Task: [CORE-007] Implement Hardware Detection (`hardware_detection.c`)**
    *   Priority: Medium

**Phase 2: Basic Ingestion Pipeline & First Parser/Preprocessor Pair**
Status: **DONE ✅**
*(No changes in this phase)*

1.  **Task: [INGEST-001] Implement Ingestion Pipeline Manager (`ingestion_pipeline.c`)**
    *   Priority: High
2.  **Task: [JSON-001] Adapt `json_preprocessor.c` to Plugin Interface**
    *   Priority: High
3.  **Task: [JSON-002] Implement `json_parser.c`**
    *   Priority: High
4.  **Task: [BUILD-002] Extend Makefile for JSON Plugins**
    *   Priority: High
5.  **Task: [TEST-001] Test Core JSON Ingestion Path**
    *   Priority: High

**Phase 3: Elasticsearch Integration & Indexing**
Status: **DONE ✅**
*(No changes in this phase)*

1.  **Task: [ES-001] Implement Elasticsearch Client (`elasticsearch_client.c`)**
    *   Priority: High
2.  **Task: [ES-002] Implement Elasticsearch Indexer Plugin (`elasticsearch_indexer.so`)**
    *   Priority: High
3.  **Task: [BUILD-003] Extend Makefile for Elasticsearch Indexer**
    *   Priority: High
4.  **Task: [TEST-002] End-to-End JSON Indexing Test**
    *   Priority: High

**Phase 4: Expanding Parsers & Preprocessors (Iterative)**
Status: **DONE ✅**
*(No changes in this phase)*

*   Tasks [TXT-001] to [SQLITE-001], [BUILD-004], [ENC-001] to [ENC-003] remain as is with their existing priorities.

**Phase 5: Decompressors**
Status: **DONE ✅**
*(No changes in this phase)*

1.  Tasks [DECOMP-001] to [BUILD-005] remain as is with their existing priorities.

**Phase 6: AEGIS-ECHO Data Ingestion Pipeline**
Status: Pending
(Based on docs/AEGIS_PIPELINE_DESIGN.md)

1.  **Task: [AEGIS-001] Design and Implement Secure Upload/Reception Endpoint**
    *   Description: Establish the API endpoint for secure file uploads, including initial validation (size, MIME type) and robust authentication/authorization.
    *   Priority: High
    *   Status: DONE ✅
2.  **Task: [AEGIS-002] Implement Staging Area for raw data**
    *   Description: Set up a secure, isolated temporary storage location for uploaded raw data before processing.
    *   Priority: High
    *   Status: DONE ✅
3.  **Task: [AEGIS-003] Implement Ingestion Task Trigger mechanism**
    *   Description: Integrate a message queue (e.g., RabbitMQ, Redis) to publish tasks for asynchronous processing upon successful data staging.
    *   Priority: High
    *   Status: DONE ✅
4.  **Task: [AEGIS-004] Develop Pipeline Worker Execution framework**
    *   Description: Create the Celery worker (or similar) infrastructure capable of subscribing to the task queue and executing a sequence of processing modules.
    *   Priority: High
    *   Status: To Do
5.  **Task: [AEGIS-005] Implement Format Conversion & Abstraction module (GDAL/OGR)**
    *   Description: Develop the pipeline module to detect input vector formats and convert them to a standard interchange format like GeoJSON using GDAL/OGR.
    *   Priority: Medium
    *   Status: To Do
6.  **Task: [AEGIS-006] Implement CRS Transformation module**
    *   Description: Develop the pipeline module to auto-detect source CRS and re-project geometries to a standard internal CRS (e.g., EPSG:4326).
    *   Priority: Medium
    *   Status: To Do
7.  **Task: [AEGIS-007] Implement Geocoding Service Integration module**
    *   Description: Develop the module to identify address fields and integrate with a geocoding service to generate point geometries.
    *   Priority: Medium
    *   Status: To Do
8.  **Task: [AEGIS-008] Implement Complex Geometry Handling module**
    *   Description: Develop the module to manage datasets with mixed geometry types based on configurable strategies (split, filter, reject).
    *   Priority: Medium
    *   Status: To Do
9.  **Task: [AEGIS-009] Implement Unstructured Text Parsing (NLP/NER) module**
    *   Description: Develop the module to use NER for extracting location names from text, integrating with the geocoding module.
    *   Priority: Medium
    *   Status: To Do
10. **Task: [AEGIS-010] Implement Geometric Validation module**
    *   Description: Develop the pipeline module to check for and optionally repair invalid geometries (e.g., using Shapely).
    *   Priority: Medium
    *   Status: To Do
11. **Task: [AEGIS-011] Implement Content-Level Security Scanning module**
    *   Description: Develop the module for security scanning (e.g., XXE prevention, zip bomb detection).
    *   Priority: Medium
    *   Status: To Do
12. **Task: [AEGIS-012] Implement Value-Domain & Schema Validation module**
    *   Description: Develop the module to validate data against expected structures, attribute types, and value constraints.
    *   Priority: Medium
    *   Status: To Do
13. **Task: [AEGIS-013] Implement Data Promotion logic**
    *   Description: Develop the logic to move validated, processed data to a trusted location and update metadata status accordingly.
    *   Priority: High
    *   Status: To Do
14. **Task: [AEGIS-014] Implement Data Quarantine (DLQ) logic**
    *   Description: Develop the logic to isolate data that fails processing/validation, update metadata, and log errors.
    *   Priority: High
    *   Status: To Do
15. **Task: [AEGIS-015] Implement Persistent Metadata Cataloguing**
    *   Description: Integrate a persistent database (e.g., PostgreSQL) for storing all ingestion job metadata, replacing any in-memory solutions.
    *   Priority: High
    *   Status: To Do
16. **Task: [AEGIS-016] Test AEGIS-ECHO pipeline end-to-end**
    *   Description: Conduct comprehensive testing of the full AEGIS-ECHO pipeline with various data types, sizes, and potential error conditions.
    *   Priority: High
    *   Status: To Do

**Phase 7: Advanced C Plugin Features - AVX-512 & Machine Learning** (Previously Phase 6)
Status: In Progress

1.  **Task: [AVX-001] Implement full AVX-512 phrase search algorithm**
    *   Priority: High
    *   Status: To Do
2.  **Task: [ES-003] Integrate AVX-512 Search with Elasticsearch Queries**
    *   Priority: Medium
    *   Status: Design Complete
3.  **Task: [ML-001] Define Machine Learning Interface (`ml_interface.h`)**
    *   Priority: Medium
    *   Status: **DONE ✅**
4.  **Task: [ML-002] Implement ML Classifier Plugin (`ml_classifier.so`)**
    *   Priority: Medium
    *   Status: **DONE ✅**
5.  **Task: [ML-003] Implement Content Analyzer Plugin (`content_analyzer.so`)**
    *   Priority: Medium
    *   Status: **DONE ✅**
6.  **Task: [INGEST-003] Integrate ML Classification into Ingestion Pipeline**
    *   Priority: Medium
    *   Status: **DONE ✅**
7.  **Task: [BUILD-006] Extend Makefile for AVX & ML Plugins**
    *   Priority: Medium
    *   Status: **DONE ✅**
*   **Sub-Phase: ML Inference Acceleration with OpenVINO**
    *   Tasks [ML-OV-001] to [ML-OV-004] (DONE ✅, Medium-High priorities)
    5.  **Task: [ML-OV-005] Execute hardware-specific testing and validation for OpenVINO FP16 Inference on Meteor Lake 165h**
        *   Priority: High
        *   Status: To Do

**Phase 8: Backend Finalization** (Previously Phase 7 - "Backend Finalization and AEGIS Pipeline Completion", AEGIS part moved to new Phase 6)
Status: Pending

1.  **Task: [BE-001] Implement persistent storage for IndexMetadata** (Refined from original BE-001)
    *   Description: Replace current in-memory list for `IndexMetadata` with a persistent storage solution.
    *   Priority: High
    *   Status: To Do
2.  **Task: [BE-002] Implement persistent storage for Query History**
    *   Priority: High
    *   Status: To Do
3.  **Task: [BE-006] Implement robust error handling and user notification for general backend operations** (Refined from original BE-006)
    *   Description: Enhance error handling for general backend API calls, provide clearer error messages, and establish user notifications where applicable (beyond AEGIS pipeline).
    *   Priority: Medium
    *   Status: To Do
4.  **Task: [BE-007] Testing and Refinement of Backend APIs and Services** (Refined from original BE-007)
    *   Description: Conduct thorough testing of all backend endpoints and services (excluding AEGIS pipeline which is tested in [AEGIS-016]).
    *   Priority: High
    *   Status: To Do
    *   (Original BE-003, BE-004, BE-005 are now covered by AEGIS tasks [AEGIS-011], [AEGIS-012], [AEGIS-009] respectively)

**Phase 9: Comprehensive GUI Implementation** (Previously Phase 8 - "GUI Development")
Status: Pending
(Based on GUI_DESIGN.md and gui/GUI_MOCKUPS.md)

*   **Foundational & Architectural GUI Tasks:**
    1.  **Task: [GUI-FND-001] Implement RBAC checks in UI based on user roles.**
        *   Description: Ensure UI elements and actions are enabled/disabled based on authenticated user's role and permissions.
        *   Priority: High
        *   Status: To Do
    2.  **Task: [GUI-FND-002] Ensure UI interactions with backend respect E2E encryption and authentication model.**
        *   Description: Verify client-side handling of tokens, mTLS requirements, and secure API communication.
        *   Priority: High
        *   Status: To Do
    3.  **Task: [GUI-FND-003] Design UI for Index Versioning & Integrity display.**
        *   Description: Implement UI elements to show index versions, associated hashes, and verification status.
        *   Priority: High
        *   Status: To Do

*   **User Experience (UX) Enhancements:**
    4.  **Task: [GUI-UX-001] Implement multi-step Index Creation Wizard.**
        *   Description: Develop the full wizard flow as per `GUI_DESIGN.md` and `gui/GUI_MOCKUPS.md`.
        *   Priority: High
        *   Status: To Do
    5.  **Task: [GUI-UX-002] Add Data Preview & Validation display within the wizard.**
        *   Description: Integrate preview of AEGIS-processed data and its validation summary in the Index Creation Wizard.
        *   Priority: Medium
        *   Status: To Do
    6.  **Task: [GUI-UX-003] Implement Breadcrumb Navigation.**
        *   Description: Add breadcrumbs for easier navigation in nested views.
        *   Priority: Medium
        *   Status: To Do
    7.  **Task: [GUI-UX-004] Integrate Contextual Help & Tooltips throughout the GUI.**
        *   Description: Add "?" icons and informative tooltips for complex UI elements.
        *   Priority: Medium
        *   Status: To Do
    8.  **Task: [GUI-UX-005] Implement Progressive Disclosure in tables.**
        *   Description: Use pagination/load-more for tables like Index Management.
        *   Priority: Medium
        *   Status: To Do
    9.  **Task: [GUI-UX-006] Implement Loading States & Skeleton Screens.**
        *   Description: Add visual feedback during data loading and asynchronous operations.
        *   Priority: High
        *   Status: To Do
    10. **Task: [GUI-UX-007] Implement Query History & Favorites feature in Query Interface.**
        *   Description: Allow users to view past queries and save favorites.
        *   Priority: High
        *   Status: To Do
    11. **Task: [GUI-UX-008] Refactor Query Interface for a Unified Map (draw/results modes).**
        *   Description: Implement the single map component that switches between geometry drawing and results display.
        *   Priority: Medium
        *   Status: To Do

*   **Data Management GUI Tasks:**
    12. **Task: [GUI-DMGT-001] Display Query Performance Metrics after query execution.**
        *   Description: Show query time and result count.
        *   Priority: High
        *   Status: To Do
    13. **Task: [GUI-DMGT-002] Implement Bulk Actions in Index Management.**
        *   Description: Add support for batch delete/rebuild of indexes.
        *   Priority: Medium
        *   Status: To Do
    14. **Task: [GUI-DMGT-003] Implement Saved & Sharable Queries.**
        *   Description: Allow users to save query definitions and share them.
        *   Priority: Medium
        *   Status: To Do
    15. **Task: [GUI-DMGT-004] Add Drag-and-Drop File Upload capability.**
        *   Description: Implement drag-and-drop for relevant file inputs (e.g., query geometry files).
        *   Priority: Medium
        *   Status: To Do

*   **Core View Implementations (incorporating above enhancements):**
    16. **Task: [GUI-001] Implement Dashboard View.** (Priority: Medium, Status: To Do)
    17. **Task: [GUI-002] Implement Index Management View.** (Priority: Medium, Status: To Do, Depends on [GUI-UX-001], [GUI-UX-005], [GUI-DMGT-002])
    18. **Task: [GUI-003] Implement Query Interface View.** (Priority: Medium, Status: To Do, Depends on [GUI-UX-007], [GUI-UX-008], [GUI-DMGT-001], [GUI-DMGT-003])
    19. **Task: [GUI-004] Implement Data Visualization View.** (Priority: Medium, Status: To Do)
    20. **Task: [GUI-005] Implement Settings View.** (Priority: Medium, Status: To Do)
    21. **Task: [GUI-006] Implement Ingestion Dashboard View.** (Priority: Medium, Status: To Do, Relies on AEGIS Pipeline)
    22. **Task: [GUI-007] Refine Enhanced Index Creation Wizard (GUI Part).** (Effectively covered by [GUI-UX-001], [GUI-UX-002], this task can be considered merged or a placeholder for final wizard integration)
        *   Priority: Medium
        *   Status: To Do
    23. **Task: [GUI-008] Implement GUI Backend API Interaction.** (Remains High Priority, Status: To Do - Ongoing)
    24. **Task: [GUI-009] Refine UI feedback mechanisms.** (Effectively covered by [GUI-UX-004], [GUI-UX-006], this task can be considered merged or a placeholder for final polish)
        *   Priority: Medium
        *   Status: To Do
    25. **Task: [GUI-010] Testing and Refinement of GUI components.** (Remains High Priority, Status: To Do)


**Phase 10: System Optimizations, Security, and Final Touches** (Previously Phase 9)
Status: Pending

1.  **Task: [OPT-001] Implement general hardware optimization plugins**
    *   Priority: Low
    *   Status: To Do
2.  **Task: [SEC-001] Implement C-level security analysis plugins (e.g., hash_separator.so, password_analyzer.so)**
    *   Priority: Low
    *   Status: To Do
3.  **Task: [CORE-008] Implement Threading Model (`threading.c`)**
    *   Priority: Medium
    *   Status: To Do
4.  **Task: [DOCS-001] Write `README.md` and Basic Usage Documentation**
    *   Priority: Medium (ongoing)
    *   Status: In Progress
5.  **Task: [TEST-003] Execute comprehensive system and performance testing**
    *   Priority: Medium
    *   Status: To Do

**Phase 11: Developer Documentation**
Status: Pending

1.  **Task: [DEV-DOC-001] Document Core Architecture and Design Principles**
    *   Description: Create a high-level overview of the SPINDEX architecture, including the core engine, plugin system, AEGIS-ECHO pipeline, backend API, and GUI. Document key design principles and decisions.
    *   Priority: High
    *   Status: To Do
2.  **Task: [DEV-DOC-002] Document AEGIS-ECHO Data Ingestion Pipeline**
    *   Description: Detail the pipeline's stages, modules, data flow, configuration options, and how to extend it with new processing or validation modules. Reference `docs/AEGIS_PIPELINE_DESIGN.md` but provide more developer-specific details.
    *   Priority: High
    *   Status: To Do
3.  **Task: [DEV-DOC-003] Document Backend API**
    *   Description: Provide detailed documentation for all backend API endpoints, including request/response schemas, authentication requirements, and usage examples. Leverage FastAPI's auto-generated OpenAPI docs and supplement as needed.
    *   Priority: High
    *   Status: To Do
4.  **Task: [DEV-DOC-004] Document SPINDEX Core and Plugin Development**
    *   Description: Explain how to develop new plugins for the SPINDEX core (parsers, preprocessors, indexers, etc.). Detail the plugin interface, build process, and testing best practices.
    *   Priority: Medium
    *   Status: To Do
5.  **Task: [DEV-DOC-005] Document GUI Components and Frontend Development**
    *   Description: Describe the GUI architecture, key components, state management, and how to interact with the backend API from the frontend. Document the build process for the frontend.
    *   Priority: Medium
    *   Status: To Do
6.  **Task: [DEV-DOC-006] Create Contribution Guidelines**
    *   Description: Outline the process for contributing to the project, including coding standards, branch management, pull request process, and testing requirements.
    *   Priority: Medium
    *   Status: To Do
7.  **Task: [DEV-DOC-007] Setup and Deployment Guide**
    *   Description: Provide instructions for setting up a development environment and deploying SPINDEX (including backend, core, and GUI components).
    *   Priority: Medium
    *   Status: To Do
8.  **Task: [DEV-DOC-008] Review and Publish Developer Documentation**
    *   Description: Collate all developer documentation into a coherent structure (e.g., a documentation website or a set of markdown files in a `/devdocs` directory). Review for clarity, accuracy, and completeness.
    *   Priority: Medium
    *   Status: To Do

---

**Chunk Reorganization (Reflecting New Phases and Task Updates)**

This section provides a high-level view, grouping phases into larger chunks.

**Chunk 1: Core System & Plugin Foundation** (Phase 1) - Status: **DONE ✅**
**Chunk 2: Basic Ingestion & ES Integration** (Phases 2, 3) - Status: **DONE ✅**
**Chunk 3: Core Parsers & Decompressors** (Phases 4, 5) - Status: **DONE ✅**

**Chunk 4: AEGIS-ECHO Data Ingestion Pipeline** (Phase 6)
Status: **Pending**
*   Key Tasks: [AEGIS-001] Endpoint (High), [AEGIS-004] Worker Framework (High), [AEGIS-015] Metadata DB (High), [AEGIS-016] Pipeline Testing (High). Core modules ([AEGIS-005] to [AEGIS-012]) are Medium.

**Chunk 5: Advanced C Plugin Features** (Phase 7)
Status: **In Progress**
*   Key Pending Tasks: [AVX-001] Full AVX Implementation (High), [ML-OV-005] Meteor Lake Testing (High).

**Chunk 6: Backend Finalization** (Phase 8)
Status: **Pending**
*   Key Tasks: [BE-001] IndexMetadata DB (High), [BE-002] Query History DB (High), [BE-007] Backend Testing (High).

**Chunk 7: Comprehensive GUI Implementation** (Phase 9)
Status: **Pending**
*   Key Tasks: Foundational GUI Security/Arch ([GUI-FND-001]-[GUI-FND-003]) (High). Core UX ([GUI-UX-001] Wizard, [GUI-UX-006] Loading States, [GUI-UX-007] Query History, [GUI-DMGT-001] Perf. Metrics) (High). Other UX/DMGT/View tasks (Medium). GUI Testing ([GUI-010]) (High).

**Chunk 8: System Optimizations & Final Touches** (Phase 10)
Status: **Pending**
*   Key Tasks: [CORE-008] Threading (Medium), [TEST-003] System Testing (Medium). Optimizations ([OPT-001], [SEC-001]) (Low). Documentation ([DOCS-001]) (Medium).

**Chunk 9: Developer Documentation** (Phase 11 - New)
Status: **Pending**
*   Key Tasks: [DEV-DOC-001] Core Arch (High), [DEV-DOC-002] AEGIS Pipeline Doc (High), [DEV-DOC-003] Backend API Doc (High). Others (Medium).

---
This breakdown should provide smaller, more manageable sets of tasks for Jules. Each chunk aims for a tangible step forward.
(Note: Phase and Chunk numbering has been significantly updated.)
