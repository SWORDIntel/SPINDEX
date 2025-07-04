# AEGIS-ECHO Data Ingestion Pipeline: Developer Guide

## 1. Introduction

The AEGIS-ECHO Data Ingestion Pipeline is a crucial component of the SPINDEX system, responsible for the robust, scalable, and secure processing of incoming geospatial and other data formats. This guide provides developers with a deeper understanding of its architecture, data flow, key components, and how to extend or interact with it.

This document complements the higher-level design documents:
*   `docs/AEGIS_PIPELINE_DESIGN.md` (Overall conceptual design)
*   `docs/AEGIS_MQ_PIPELINE_ARCHITECTURE.md` (Message queue architecture specifics)

## 2. Architecture Overview

The AEGIS pipeline is designed as an asynchronous, message-driven system to handle potentially long-running data processing tasks without blocking user interactions.

*   **Key Components:**
    *   **API Endpoint (`/aegis/upload` in `backend/main.py`):** The entry point for data. Handles initial validation (file size, MIME type), authentication, and authorization. Upon successful validation, it stages the raw file and creates an ingestion job record.
    *   **Message Queue (NATS):** Used for decoupling the API from the processing workers. The API publishes a message containing the `job_id` when a new file is ready for processing. (Refer to `AEGIS_MQ_PIPELINE_ARCHITECTURE.md` for NATS stream/subject details).
        *   The `JobOutbox` table in PostgreSQL and `message_relay.py` ensure reliable "at-least-once" publishing of these messages to NATS.
    *   **Celery Workers (`backend/aegis_worker.py`):** Python-based workers that consume messages from the NATS queue. Each message triggers the processing of a specific ingestion job.
    *   **Pipeline Logic (`backend/pipeline_logic.py`):** Contains the sequence of Python functions that implement the various processing stages for a given job. This is orchestrated by the Celery task in `aegis_worker.py`.
    *   **Database (PostgreSQL):**
        *   `IngestionJobDB` (`backend/models.py`): Persistently stores metadata for each ingestion job, including its status, paths to data at various stages, logs, and summaries.
        *   `JobModuleStatus` (`backend/models.py`): Tracks the status and details of individual modules executed within the pipeline for a given job.
    *   **File Storage:**
        *   **Staging Area:** Temporary storage for raw uploaded files (`STAGING_AREA_PATH` in `backend/main.py`).
        *   **Processed Area:** Storage for intermediate files generated during pipeline execution (e.g., converted GeoJSON files) (`PROCESSED_AREA_PATH` in `backend/pipeline_logic.py`).
        *   **Trusted Area:** Final location for successfully processed and validated data suitable for indexing (`TRUSTED_AREA_PATH` in `backend/pipeline_logic.py`).
        *   **Quarantine Area:** Location for files that fail critical validation or processing steps (`QUARANTINE_AREA_PATH` in `backend/aegis_worker.py`).

*   **Data Flow Diagram (Conceptual):**
    ```
    User --(Upload File)--> FastAPI Endpoint (`/aegis/upload`)
                                 |
                                 v
                        (Save to Staging Area)
                        (Create IngestionJobDB entry - status: PENDING_PUBLISH)
                        (Create JobOutbox entry)
                                 |
                                 v
    Message Relay (`message_relay.py`) --(Publish to NATS)--> NATS Stream (e.g., AEGIS_JOB_STREAM)
                                                                 |
                                                                 v
    AEGIS Celery Worker (`aegis_worker.py`) --(Consume from NATS)--> (Fetch Job Details from DB)
                                                                         |
                                                                         v
                                                            Execute `pipeline_logic.py`
                                                                 (Multiple Stages)
                                                                         |
                                                                         v
                                            (Update IngestionJobDB: status, paths, logs, summaries)
                                            (Update JobModuleStatus for each stage)
                                            (Move data to Trusted Area or Quarantine Area)
    ```

## 3. Ingestion Job Lifecycle

The `processing_status` field in the `IngestionJobDB` model tracks the state of an ingestion job. Key statuses include:

*   `PENDING_PUBLISH`: Initial state after upload, before the job message is published to NATS.
*   `PUBLISHED_PENDING_WORKER`: Message published to NATS, awaiting worker pickup. (This status might be set by `message_relay.py`).
*   `WORKER_RECEIVED_PENDING_IDEMPOTENCY`: Worker picked up the message, validating job.
*   `PROCESSING_SECURITY_SCAN`: Security scan in progress.
*   `FAILED_SECURITY_SCAN` / `ERROR_SECURITY_SCAN`: Security scan failed.
*   `PROCESSING_FORMAT_DETECTION`: Format detection in progress.
*   `FAILED_FORMAT_DETECTION`: Format detection failed.
*   `FORMAT_DETECTED_PENDING_CONVERSION`: Format detected, awaiting conversion.
*   `COMPLETED_FORMAT_CONVERSION`: File converted (e.g., to GeoJSON).
*   `PROCESSING_CRS_DETECTION` / `FAILED_CRS_DETECTION`
*   `CRS_DETECTED_PENDING_TRANSFORMATION`
*   `COMPLETED_CRS_TRANSFORMATION` / `FAILED_CRS_TRANSFORMATION`
*   `PROCESSING_GEOCODING` / `FAILED_GEOCODING_STEP`
*   `COMPLETED_GEOCODING`
*   `PROCESSING_COMPLEX_GEOMETRY` / `FAILED_COMPLEX_GEOMETRY_STEP`
*   `COMPLETED_COMPLEX_GEOMETRY`
    *   *(Other module-specific processing statuses as they are implemented)*
*   `PROCESSING_PROMOTION`: Data promotion to trusted area in progress.
*   `FAILED_PROMOTION` / `FAILED_PROMOTION_CHECKSUM_MISMATCH`: Promotion failed.
*   `PROMOTED`: Data successfully processed and moved to the trusted area, ready for indexing.
*   `QUARANTINED`: Job failed critical checks and data moved to quarantine.
*   `ERROR_PIPELINE_UNKNOWN`: An unexpected error occurred in the pipeline.

## 4. Pipeline Stages (Implemented in `backend/pipeline_logic.py`)

The `execute_aegis_pipeline` function in `pipeline_logic.py` orchestrates the following stages for each job. Each stage updates the `IngestionJobDB` and potentially `JobModuleStatus`.

1.  **Security Scan:**
    *   **Module:** `backend.security_scanner.scan_file()`
    *   **Purpose:** Scans the uploaded file for threats like zip bombs or (conceptually) XXE in XMLs.
    *   **Inputs:** Staged file path, original filename, content type, configuration for scanner limits.
    *   **Outputs:** Scan report (JSON), updates `IngestionJobDB.validation_summary` and `processing_status`.
    *   **Failure:** If threats are detected or a critical scanning error occurs, the pipeline stops, and the job status is set accordingly (e.g., `FAILED_SECURITY_SCAN`).

2.  **Format Detection:**
    *   **Tool:** GDAL/OGR (`ogr.Open`, `dataSource.GetDriver`)
    *   **Purpose:** Detects the geospatial format of the (intermediate) file.
    *   **Inputs:** File path (initially staged file, then intermediate GeoJSON).
    *   **Outputs:** Detected GDAL driver name, updates `IngestionJobDB.detected_format`.
    *   **Failure:** If GDAL cannot open or recognize the file, status set to `FAILED_FORMAT_DETECTION`.

3.  **Format Conversion (to GeoJSON):**
    *   **Tool:** GDAL (`gdal.VectorTranslate`)
    *   **Purpose:** Converts the input vector data to a standardized GeoJSON format.
    *   **Inputs:** Current file path, target format ("GeoJSON").
    *   **Outputs:** A new GeoJSON file in the `PROCESSED_AREA_PATH`. Updates `IngestionJobDB.intermediate_geojson_path`.
    *   **Failure:** If conversion fails, status set to `FAILED_FORMAT_CONVERSION`.

4.  **CRS (Coordinate Reference System) Detection:**
    *   **Tool:** GDAL/OGR (`layer.GetSpatialRef().ExportToWkt()`)
    *   **Purpose:** Detects the CRS of the current GeoJSON file.
    *   **Inputs:** Path to the (intermediate) GeoJSON file.
    *   **Outputs:** CRS information in WKT format, updates `IngestionJobDB.original_crs`.
    *   **Failure:** If CRS cannot be determined, status set to `FAILED_CRS_DETECTION`.

5.  **CRS Transformation:**
    *   **Tool:** GDAL (`gdal.VectorTranslate` with `-t_srs`)
    *   **Purpose:** Reprojects the GeoJSON data to a standard internal CRS (e.g., EPSG:4326).
    *   **Inputs:** Current GeoJSON path, source CRS (from `IngestionJobDB.original_crs`), target CRS (e.g., `EPSG:4326`).
    *   **Outputs:** A new reprojected GeoJSON file, overwriting `IngestionJobDB.intermediate_geojson_path`. Updates `IngestionJobDB.target_crs`.
    *   **Failure:** If transformation fails, status set to `FAILED_CRS_TRANSFORMATION`.

6.  **Geocoding (Feature Enrichment):**
    *   **Module:** `pipeline_logic.geocode_and_update_features()` (uses `geopy.Nominatim`)
    *   **Purpose:** For features lacking geometry, attempts to geocode them using address information found in specified properties.
    *   **Inputs:** GeoJSON features list, configured address fields to search, Nominatim user agent.
    *   **Outputs:** Updates GeoJSON features in-place with point geometries. Populates `IngestionJobDB.geocoding_summary`.
    *   **Failure:** Errors during geocoding (service errors, address not found) are logged in the summary. A step failure (`FAILED_GEOCODING_STEP`) occurs if the process itself has an unhandled exception.

7.  **Complex Geometry Handling:**
    *   **Module:** `pipeline_logic.handle_complex_geometries()` (uses `shapely`)
    *   **Purpose:** Manages datasets with mixed or complex geometry types (e.g., GeometryCollection, MultiPoint/Line/Polygon) based on a configured strategy (e.g., `split_all`, `filter_types`, `explode_collections`).
    *   **Inputs:** GeoJSON features list, configured strategy, strategy parameters (e.g., types to keep/reject).
    *   **Outputs:** Modified GeoJSON features list. Populates `IngestionJobDB.complex_geometry_summary`.
    *   **Failure:** If a feature causes a rejection based on strategy, or an unhandled exception occurs, sets status to `FAILED_COMPLEX_GEOMETRY_STEP`.

8.  **Data Promotion:**
    *   **Purpose:** If all prior stages are successful, moves the final processed (and validated) GeoJSON file from the `PROCESSED_AREA_PATH` to the `TRUSTED_AREA_PATH`.
    *   **Process:**
        *   Calculates SHA256 checksum of the source processed file.
        *   Copies the file to the trusted area.
        *   Calculates SHA256 checksum of the destination file.
        *   Compares checksums. If they match, updates `IngestionJobDB` with `location_of_promoted_data`, `promotion_timestamp`, `data_hash_processed`, and status `PROMOTED`.
        *   Removes the source file from `PROCESSED_AREA_PATH` after successful DB commit.
    *   **Failure:** If file operations fail or checksums mismatch, status is set to `FAILED_PROMOTION` or `FAILED_PROMOTION_CHECKSUM_MISMATCH`. The corrupted destination file (if any) is removed.

## 5. Error Handling and Quarantine

*   **Stage Failures:** Each pipeline stage logs its execution status and any errors encountered into `IngestionJobDB.pipeline_stages_executed_log`. Critical failures in a stage typically stop the pipeline for that job, and the job's overall `processing_status` is updated (e.g., `FAILED_FORMAT_CONVERSION`).
*   **Quarantine Logic (in `aegis_worker.py`):**
    *   If `execute_aegis_pipeline` returns `False` (indicating a failure), the `aegis_worker.py` attempts to move the original staged file (and potentially the last intermediate GeoJSON) to a quarantine area (`QUARANTINE_AREA_PATH/<job_id>/`).
    *   The `IngestionJobDB` record is updated with `processing_status = "QUARANTINED"`, `location_of_quarantined_data`, and `quarantine_timestamp`.
    *   The NATS message for the job is then ACKed to prevent redelivery for that specific failure.
*   **Idempotency:** The `aegis_worker.py` performs an idempotency check before processing a job. If the job is already in a terminal state (e.g., `PROMOTED`, `QUARANTINED`, or certain `FAILED_` states), it ACKs the message without reprocessing.

## 6. Configuration

Key aspects of the AEGIS pipeline are configurable via environment variables (see `backend/main.py` and `backend/pipeline_logic.py` for defaults and env var names):

*   `STAGING_AREA_PATH`, `PROCESSED_AREA_PATH`, `TRUSTED_AREA_PATH`, `QUARANTINE_AREA_PATH`
*   `NATS_URL`, `NATS_AEGIS_JOB_STREAM_NAME`, `NATS_AEGIS_WORKER_DURABLE_NAME`
*   `TARGET_CRS_EPSG` (for CRS transformation)
*   `NOMINATIM_USER_AGENT`
*   `GEOCODING_ADDRESS_FIELDS` (JSON string list)
*   `COMPLEX_GEOMETRY_STRATEGY` and related filter lists (`CGH_FILTER_TYPES_TO_KEEP`, `CGH_REJECT_IF_TYPES_FOUND`)
*   Security Scanner thresholds (e.g., `SCANNER_MAX_TOTAL_UNCOMPRESSED_SIZE`)

## 7. Extending the Pipeline

To add a new processing module/stage to the AEGIS pipeline:

1.  **Write the Processing Logic:**
    *   Create a new Python function, typically in `backend/pipeline_logic.py` or a new dedicated module.
    *   This function should accept `job_id`, `db_session`, `current_file_path`, `logger_instance`, and any other necessary parameters or configuration.
    *   It should perform its processing, update the `IngestionJobDB` (status, logs, relevant summary fields), and manage intermediate files.
    *   Return a boolean indicating success/failure or the path to the new output file.
2.  **Integrate into `execute_aegis_pipeline`:**
    *   Call your new function at the appropriate point in the sequence within `execute_aegis_pipeline` in `pipeline_logic.py`.
    *   Handle its success/failure: update job status, log appropriately, and decide if a failure is critical enough to stop the pipeline.
    *   Ensure new `processing_status` values are defined and handled.
3.  **Update `JobModuleStatus` (Optional but Recommended):**
    *   Consider adding entries to the `JobModuleStatus` table at the start and end of your new module's execution to provide granular tracking.
4.  **Configuration:**
    *   If your new module requires configuration, add environment variables or integrate with a broader configuration management system.
5.  **Testing:**
    *   Write unit tests for your new processing function.
    *   Update or add integration tests that cover the pipeline including your new stage. (Refer to `docs/AEGIS_PIPELINE_TESTING_PLAN.md`).

## 8. Testing the Pipeline

Refer to `docs/AEGIS_PIPELINE_TESTING_PLAN.md` for comprehensive strategies on testing the pipeline, including end-to-end tests, individual module tests, and handling of various data types and error conditions.
The `backend/tests/test_aegis_pipeline.py` contains automated tests for pipeline logic.
```
