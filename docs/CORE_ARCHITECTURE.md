# SPINDEX Core Architecture and Design Principles

## 1. Introduction

SPINDEX (Spatial Indexer) is a modular system designed for ingesting, processing, indexing, and searching geospatial and unstructured data. Its core architecture emphasizes flexibility through a plugin system, robust data handling via the AEGIS-ECHO pipeline, and efficient search capabilities leveraging advanced CPU features where available.

This document provides a high-level overview of the SPINDEX architecture, its main components, and the key design principles guiding its development.

## 2. High-Level Architecture

The SPINDEX system can be visualized as several key interacting components:

*   **C Core Engine:** A high-performance engine written in C, responsible for low-level data processing, plugin management, and search orchestration. This includes the `spindex_app` command-line utility and underlying libraries.
*   **Plugin System:** Allows dynamic loading of modules (plugins) to extend functionality for parsing, preprocessing, indexing, analysis, and search algorithms.
*   **AEGIS-ECHO Data Ingestion Pipeline:** A message-queue driven, multi-stage pipeline for robust and scalable data ingestion, validation, transformation, and security scanning of incoming data before it's made available for indexing.
*   **Backend API (Python/FastAPI):** Provides a RESTful interface for user interaction, data upload, index management, job control, and search queries. It often acts as a bridge to the C Core Engine for performance-critical operations.
*   **Graphical User Interface (GUI):** (Future/In Development) A web-based interface for users to interact with the SPINDEX system.
*   **External Services:**
    *   **Elasticsearch/OpenSearch:** Primary datastore for indexed data and search.
    *   **NATS/Kafka:** Message queue for the AEGIS-ECHO pipeline.
    *   **PostgreSQL:** Database for persistent metadata (ingestion jobs, index metadata, query history).
    *   **Redis:** Cache and Celery message broker for the AEGIS pipeline.

*(A textual diagram placeholder: Consider a flow where User/GUI interacts with Backend API. Backend API triggers AEGIS Pipeline via Message Queue. AEGIS Worker (Celery) consumes from MQ, uses C-Core/Plugins for some processing stages, and writes to PostgreSQL/Elasticsearch. Backend API also uses C-Core/Plugins for search queries against Elasticsearch.)*

## 3. C Core Engine (`spindex_app` & Core Libraries)

The C Core Engine forms the backbone for efficient data processing and search.

*   **Purpose:**
    *   Manages the lifecycle of plugins (loading, unloading).
    *   Orchestrates search queries, potentially combining results from different sources or applying advanced filtering (e.g., AVX-accelerated phrase search).
    *   Provides utility functions for configuration, hardware detection, logging, and data type management.
*   **Key Modules:**
    *   **`config_manager` (`config_manager.c/.h`):** Loads and provides access to system configuration (e.g., from `.conf` files).
    *   **`plugin_loader` (`plugin_loader.c/.h`):** Discovers, loads, initializes, and unloads plugins from shared object (`.so`) files. Includes versioning and dependency resolution ([CORE-009]).
    *   **`search_orchestrator` (`search_orchestrator.c/.h`):** Manages the execution of search queries, potentially involving multiple plugins or search backends.
    *   **`hardware_detection` (`hardware_detection.c/.h`):** Detects CPU features (e.g., AVX support) to enable optimized code paths.
    *   **`utils` (`utils.c/.h`):** Common utility functions, including logging macros.
    *   **`threading` (`threading.c/.h`):** Provides a thread pool and task queue for concurrent execution of tasks within the C core ([CORE-008]).
    *   **`data_types` (`data_types.h`):** Defines core data structures like `ProcessedDataChunk` and `PluginInfo`.
*   **`spindex_app` (`main.c`):** The main executable for the C core. Its role can be multifaceted:
    *   A command-line tool for direct interaction, testing, or specific batch processing tasks.
    *   Potentially, a long-running worker process if designed to listen to queues or perform continuous tasks (this aspect is evolving).
    *   The C components can also be compiled as a library (e.g., `libspindex_core_cffi.so`) to be called from other languages like Python (via CFFI), as seen in the backend's AVX search integration.

## 4. Plugin System

The plugin system is central to SPINDEX's extensibility.

*   **Plugin Types:**
    *   **Parsers:** Understand specific file formats (e.g., JSON, CSV, PDF, DOC).
    *   **Preprocessors:** Perform initial transformations or cleaning on data before parsing or further processing.
    *   **Indexers:** Interface with search backends (e.g., Elasticsearch) to write data to an index.
    *   **Analyzers:** Perform content analysis (e.g., identifying content types, metadata extraction).
    *   **Classifiers:** Categorize data using machine learning models (e.g., OpenVINO-accelerated classifiers).
    *   **Search Algorithms:** Implement specialized search logic or post-processing filters (e.g., AVX-512 phrase search).
    *   *Other types as needed (Decompressors, Optimizers, Security Scanners, etc.).*
*   **Interface (`plugin_interface.h`):** Defines a standard contract for all plugins, including functions for:
    *   `spindex_plugin_get_info()`: Provides metadata about the plugin (name, version).
    *   `spindex_plugin_get_dependencies()`: Declares dependencies on other plugins and their versions.
    *   `spindex_plugin_free_dependencies_list()`: Allows the plugin to manage memory for its dependency list.
    *   `spindex_plugin_init()`: One-time initialization.
    *   `spindex_plugin_process_data()`: The core data processing function.
    *   `spindex_plugin_shutdown()`: Cleanup on unload.
*   **Loading & Lifecycle:** Managed by `plugin_loader.c`. Plugins are dynamically loaded from `.so` files. The loader resolves dependencies and versions before initializing plugins.

## 5. Backend API (Python/FastAPI)

The Backend API serves as the primary interaction point for users and the GUI.

*   **Role:**
    *   Provides RESTful HTTP endpoints for all system functionalities.
    *   Handles user authentication and authorization (RBAC).
    *   Manages ingestion jobs, index metadata, and query history via PostgreSQL.
    *   Triggers the AEGIS-ECHO pipeline for new data.
    *   Interfaces with the C Core Engine (e.g., via CFFI) for performance-sensitive search operations like AVX post-filtering.
    *   Communicates with Elasticsearch for indexing and standard search operations.
*   **Key Modules/Components (`SPINDEX/backend/`):**
    *   `main.py`: FastAPI application setup, endpoint definitions, exception handlers.
    *   `crud.py`: Database Create, Read, Update, Delete operations using SQLAlchemy.
    *   `models.py`: SQLAlchemy ORM models for database tables (e.g., `IngestionJobDB`, `IndexMetadataDB`, `QueryHistoryDB`).
    *   `database.py`: SQLAlchemy engine and session setup.
    *   `exceptions.py`: Custom exception classes for standardized error handling.
    *   `pipeline_logic.py`: Contains the Python-based stages of the AEGIS pipeline.
    *   `aegis_worker.py`: Celery worker that consumes jobs from NATS and executes `pipeline_logic.py`.
    *   `security_scanner.py`: Module for file security checks (zip bombs, XXE).

## 6. AEGIS-ECHO Data Ingestion Pipeline

A robust, asynchronous pipeline for processing incoming data. (Detailed documentation in `docs/AEGIS_PIPELINE_DEVELOPER_GUIDE.md` - [DEV-DOC-002]).

*   **Key Features:** Message-queue driven (NATS), Celery for task execution, persistent job metadata in PostgreSQL, modular stages.
*   **High-Level Flow:** Upload -> Staging -> NATS Message -> Celery Worker (`aegis_worker.py`) -> `pipeline_logic.py` (Security Scan, Format Detection/Conversion, CRS Handling, Geocoding, Complex Geometry, Validation) -> Promotion to Trusted Area / Quarantine -> Indexing (separate step triggered after promotion).

## 7. Build System

*   **CMake:** The primary build system for C components ([BUILD-ARC-001]).
*   **Conan:** C/C++ package manager used to manage external dependencies (Jansson, libcurl, GEOS, OpenVINO, etc.) and ensure pinned versions ([BUILD-ARC-002]).
*   The combination provides a cross-platform, reproducible build environment for the C core and plugins.

## 8. Key Design Principles

*   **Modularity & Extensibility:**
    *   Achieved through the plugin system for C components.
    *   Microservice-oriented principles for backend services where appropriate (e.g., separation of API, worker, message queue).
*   **Performance:**
    *   C for performance-critical core logic and data processing.
    *   Leveraging CPU-specific optimizations (e.g., AVX-512) via plugins.
    *   Asynchronous processing for data ingestion (AEGIS pipeline).
    *   Efficient data structures and algorithms.
*   **Robustness & Reliability:**
    *   Standardized error handling in the backend API ([BE-006]).
    *   Transactional outbox pattern for reliable messaging from API to NATS.
    *   Data validation and security scanning in the AEGIS pipeline.
    *   Persistent job and metadata tracking.
*   **Configuration-Driven:**
    *   System behavior (paths, parameters, feature flags) configurable via files (e.g., `spindex.conf`) and environment variables.
*   **Testability:**
    *   Unit and integration tests for backend Python components (pytest).
    *   Tests for C components (custom test executables or a C test framework).
*   **Security:**
    *   Authentication and authorization for API endpoints.
    *   Security scanning of uploaded files.
    *   (Future: E2E encryption considerations for data in transit/at rest where applicable).
*   **Maintainability:**
    *   Clear separation of concerns between components.
    *   Consistent coding style (to be enforced).
    *   Comprehensive documentation.
```
