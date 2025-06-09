# AEGIS-ECHO Data Ingestion Pipeline Design

## 1. Overview

*   **Purpose:** This document details the architecture and design of the AEGIS-ECHO Data Ingestion Pipeline for SPINDEX. The primary goal of this pipeline is to provide a secure, resilient, and extensible mechanism for ingesting heterogeneous, unstructured, and potentially malicious geospatial data from various sources into the SPINDEX system. It aims to abstract the complexities of data handling, ensuring that only validated and standardized data is made available for indexing.
*   **Scope:** Covers the pipeline from data reception to its promotion for indexing or quarantine.
*   **Guiding Principles:** Security by design, resilience, modularity, extensibility, detailed metadata tracking.

## 2. Pipeline Architecture

**(Placeholder for a diagram illustrating the stages and data flow that can be added later)**

### 2.1. Pipeline Stages

*   **Stage 1: Secure Upload/Reception Endpoint**
    *   **Responsibilities:** Securely accept incoming data.
    *   **Input:** Raw data (file uploads via HTTP POST, potentially API streams, or connection parameters for database sources in later phases).
    *   **Process:**
        *   Authentication & Authorization: Verify user identity and permissions.
        *   Initial validation (e.g., file size limits, basic MIME type checks).
        *   Immediate transfer to Staging Area.
    *   **Output:** Data written to Staging Area; metadata entry for the Ingestion Task.
*   **Stage 2: Staging Area**
    *   **Responsibilities:** Provide temporary, isolated storage for uploaded raw data before processing.
    *   **Characteristics:**
        *   Isolated from core system and other data.
        *   Unique, traceable identifiers for each staged item.
        *   Permissions restricted to pipeline components.
*   **Stage 3: Ingestion Task Trigger**
    *   **Responsibilities:** Initiate asynchronous processing of staged data.
    *   **Process:**
        *   Upon successful staging of data, a message is published to the Ingestion Task Queue (e.g., RabbitMQ, Redis).
        *   Message content: Reference to staged data (e.g., path/URI), original filename, user ID, initial metadata.
*   **Stage 4: Pipeline Worker Execution**
    *   **Responsibilities:** Orchestrate and execute a sequence of configurable processing and validation modules on the data.
    *   **Process:**
        *   Workers subscribe to the Ingestion Task Queue.
        *   On receiving a task:
            1.  Retrieve data from Staging Area.
            2.  Execute a defined sequence of pluggable modules.
            3.  Collect logs, results, and error information.
            4.  Update the Metadata Catalogue at each significant step.
*   **Stage 5: Data Promotion**
    *   **Responsibilities:** Move validated and processed data to a trusted location for subsequent indexing by SPINDEX.
    *   **Process:**
        *   If all mandatory pipeline modules execute successfully and pass all validation checks:
            1.  The final, processed data is considered ready for promotion.
            2.  Metadata status updated (e.g., `validation_successful_ready_for_promotion`).
            3.  Path to promoted data recorded in metadata.
*   **Stage 6: Data Quarantine (Dead-Letter Queue - DLQ)**
    *   **Responsibilities:** Isolate data that fails processing or validation, along with detailed error information, for manual review.
    *   **Process:**
        *   If any critical pipeline module fails or data does not pass validation:
            1.  Metadata status updated (e.g., `quarantined_validation_failed`).
            2.  Detailed error log stored/referenced in metadata.
            3.  Path to problematic data recorded.
            4.  (Optionally) Notifications sent to administrators.
*   **Stage 7: Metadata Cataloguing**
    *   **Responsibilities:** Track the lineage and processing history of all ingested data.
    *   **Information to Capture:** job_id, original_filename, upload_timestamp, user_id, source_type, detected_format, original_crs, target_crs, processing_status, promotion_timestamp, quarantine_timestamp, location_of_promoted_data, location_of_quarantined_data, pipeline_stages_executed, validation_results, error_log_references, data_hashes (original/processed).
    *   **Storage:** Persistent database.

## 3. Core Pluggable Modules

*   **3.1. Format Conversion & Abstraction (GDAL/OGR Integration)**
    *   **Purpose:** Convert various input vector formats into a common internal representation.
    *   **Library:** GDAL/OGR.
    *   **Process:** Detect input format, read vector data, transform to a standard interchange format (e.g., GeoJSON).
    *   **Configuration:** Supported input formats, output interchange format.
    *   **Error Handling:** Failure on unsupported format or read errors.
*   **3.2. Coordinate Reference System (CRS) Transformation**
    *   **Purpose:** Ensure all data is in a standard internal CRS.
    *   **Standard Internal CRS:** EPSG:4326 - WGS 84 (configurable).
    *   **Process:** Auto-detect source CRS. Re-project geometries to the standard internal CRS.
    *   **Error Handling:** Failure if CRS cannot be determined or re-projection fails.
*   **3.3. Geocoding Service Integration**
    *   **Purpose:** Generate point geometries for features with address information but lacking explicit geometry.
    *   **Process:** Identify address fields. Send data to a geocoding service (e.g., self-hosted Nominatim, external API). Update/create geometries.
    *   **Configuration:** Address field name, geocoding service endpoint/API key, timeout.
    *   **Error Handling:** Log failures, continue if possible.
*   **3.4. Complex Geometry Handling**
    *   **Purpose:** Manage datasets with mixed geometry types or ensure specific geometry types.
    *   **Process Strategies (User-defined or configurable):**
        1.  **Split:** Create multiple outputs, one for each geometry type.
        2.  **Filter:** Select only one geometry type to import, discard others.
        3.  **Reject:** Fail ingestion if mixed types are present and not desired.
    *   **Error Handling:** Based on chosen strategy.
*   **3.5. Unstructured Text Parsing (NLP/NER)**
    *   **Purpose:** Extract location names from text documents or fields to create spatial features.
    *   **Process:** Use Named Entity Recognition (NER) to scan text, identify location names, pass to geocoding module.
    *   **Configuration:** Target text fields/document types, NLP model selection, entity types to extract.
    *   **Error Handling:** Log failures in NER or subsequent geocoding.

## 4. Security: Validation & Sanitization Mandates

*   **4.1. Geometric Validation**
    *   **Purpose:** Check for and optionally repair invalid geometries.
    *   **Process:** Use standard functions (e.g., `ST_IsValid` in PostGIS, Shapely's `is_valid`). Attempt repair for common issues (`MakeValid`).
    *   **Configuration:** Strict (reject invalid) vs. Attempt Repair.
    *   **Error Handling:** Quarantine data with irreparable invalid geometries. Log details.
*   **4.2. Content-Level Security Scanning**
    *   **Purpose:** Mitigate risks from malicious file structures.
    *   **Process:**
        *   XML-based formats (KML, GML): Disable XXE processing, limit entity expansion.
        *   Compressed files (ZIP): Check for "zip bomb" characteristics.
    *   **Error Handling:** Quarantine suspicious files. Log details.
*   **4.3. Value-Domain & Schema Validation**
    *   **Purpose:** Validate data against expected structure and value constraints.
    *   **Process:** Check for required columns/attributes, data types, logical coordinate domains (lat/lon ranges). (Future: user-defined schemas).
    *   **Error Handling:** Quarantine data failing validation. Log specific errors.

## 5. UI Integration

*   **5.1. Ingestion Dashboard**
    *   **Functionality:** New top-level GUI view. Lists status of current/past ingestion jobs. Provides access to quarantined items.
*   **5.2. Quarantine Review**
    *   **Functionality:** Tab/section within Ingestion Dashboard. Displays quarantined jobs with source filename, error summary.
    *   **Actions:** [Discard Job], [View Error Log], [Retry Job].
*   **5.3. Enhanced Index Creation Wizard Integration**
    *   **Step 2 (Data Source):** After file upload, backend pipeline runs "dry-run" analysis.
    *   **Step 3 (Validation & Configuration):** UI presents analysis results (detected format, CRS, proposed changes). Data preview highlights validation warnings.

## 6. Metadata Cataloguing (Reiteration from Stage 7)

*   **Purpose:** Track lineage and processing history.
*   **Key Information:** job_id, original_filename, timestamps (upload, promotion, quarantine), user_id, source_type, detected_format, original_crs, target_crs, processing_status, paths (promoted, quarantined), pipeline_stages_executed, validation_summary, error_logs, data_hashes.
*   **Storage:** Persistent database, accessible via API.
