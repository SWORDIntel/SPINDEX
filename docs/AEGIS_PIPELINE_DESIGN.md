# AEGIS-ECHO Data Ingestion Pipeline Design

## 1. Overview

*   **Purpose:** This document details the architecture and design of the AEGIS-ECHO Data Ingestion Pipeline for SPINDEX. The primary goal of this pipeline is to provide a secure, resilient, and extensible mechanism for ingesting heterogeneous, unstructured, and potentially malicious geospatial data from various sources into the SPINDEX system. It aims to abstract the complexities of data handling, ensuring that only validated and standardized data is made available for indexing.
*   **Scope:** Covers the pipeline from data reception to its promotion for indexing or quarantine.
*   **Guiding Principles:** Security by design, resilience, modularity, extensibility, detailed metadata tracking.

## 2. Pipeline Architecture

**(Placeholder for a diagram illustrating the stages and data flow that can be added later)**

### 2.1. Pipeline Stages

(Pipeline Stages 1-4, 5, 7 remain as previously defined)
*   **Stage 1: Secure Upload/Reception Endpoint**
*   **Stage 2: Staging Area**
*   **Stage 3: Ingestion Task Trigger**
*   **Stage 4: Pipeline Worker Execution**
*   **Stage 5: Data Promotion**
    *   (As previously defined)
*   **Stage 6: Data Quarantine (DLQ)**
    *   **Responsibilities:** Isolate data that fails processing or validation, along with detailed error information, for manual review and potential reprocessing.
    *   **Process (Criteria for Quarantine):**
        *   Quarantine is triggered if any critical pipeline module/function (e.g., Format Conversion, CRS Transformation, Geometric Validation) encounters a fatal error during its execution. In such cases:
            *   The failing module immediately updates the `IngestionJobDB.processing_status` to a specific status indicating the point of failure (e.g., `"format_conversion_failed"`, `"crs_transformation_failed"`, `"geometric_validation_error"`).
            *   `IngestionJobDB.quarantine_timestamp` is set.
            *   `IngestionJobDB.location_of_quarantined_data` is set to the path of the data file at its last known state (which might be the original staged file or an intermediate processed file).
            *   Detailed error messages from the failing module are logged into `IngestionJobDB.error_log_references` and `IngestionJobDB.pipeline_stages_executed_log`.
            *   The Celery task (`process_staged_file_task`) terminates processing for that job.
        *   Additionally, if all explicitly implemented modules complete but one of the placeholder validation flags (e.g., for future content security or schema validation) is determined to be false, the final `overall_success` check at the end of `process_staged_file_task` will fail. This also leads to quarantine:
            *   `IngestionJobDB.processing_status` is typically set to a general status like `"quarantined_validation_failed"` (if not already set to a more specific failure status by a module).
            *   `IngestionJobDB.quarantine_timestamp` is set.
            *   `IngestionJobDB.location_of_quarantined_data` is updated.
            *   A summary of which validation(s) caused the `overall_success` to be false is logged in `IngestionJobDB.error_log_references`.
    *   **Outcome:** Quarantined data is not eligible for promotion to SPINDEX indexing until the issues are addressed (e.g., manual correction and re-ingestion, or pipeline adjustment). The GUI's Ingestion Dashboard will provide a view into quarantined items.
*   **Stage 7: Metadata Cataloguing**
    *   (As previously defined)

## 3. Core Pluggable Modules (Executed within `process_staged_file_task`)

*   **3.1. Format Conversion & Abstraction (GDAL/OGR Integration)**
    *   (As previously defined)
*   **3.2. Coordinate Reference System (CRS) Transformation**
    *   (As previously defined)
*   **3.3. Geometric Validation**
    *   (As previously defined)
*   **3.4. Geocoding Service Integration**
    *   (As previously defined)
*   **3.5. Complex Geometry Handling**
    *   (As previously defined)
*   **3.6. Unstructured Text Parsing (NLP/NER)**
    *   (As previously defined)

## 4. Security: Validation & Sanitization Mandates

*   **4.1. Geometric Validation:** (Covered in section 3.3)
*   **4.2. Content-Level Security Scanning**
    *   (As previously defined)
*   **4.3. Value-Domain & Schema Validation**
    *   (As previously defined)

## 5. UI Integration

(As previously stated)

## 6. Metadata Cataloguing (Reiteration from Stage 7)

(As previously stated)
