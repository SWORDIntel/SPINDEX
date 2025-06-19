# SPINDEX Backend

This directory contains the Python FastAPI backend application for SPINDEX. It includes features for asynchronous task processing using Celery and Redis, a file upload endpoint for data ingestion, and a persistent metadata catalogue for tracking ingestion jobs using SQLAlchemy and SQLite.

## Prerequisites

*   Docker (for containerized deployment)
*   Python 3.11+ (for local development)
*   `pip` (Python package installer)
*   Redis server (for local development)
*   **GDAL (Geospatial Data Abstraction Library):** System libraries and Python bindings.
*   **GEOS:** System library required by Shapely.
*   **SQLAlchemy, Alembic:** For database ORM and migrations.

## Configuration

The backend application and worker behavior can be configured using environment variables. Key variables include:

*   `NATS_URL`: URL for the NATS server (default: "nats://localhost:4222").
*   `AEGIS_JOB_STREAM_NAME`: Name of the NATS JetStream stream for jobs (default: "AEGIS_JOB_STREAM").
*   `NATS_AEGIS_WORKER_DURABLE_NAME`: Durable name for the NATS consumer (default: "aegis_job_worker").
*   `STAGING_AREA_PATH`: Filesystem path for temporary storage of uploaded files before processing (e.g., default used by API might be "staging_area/"). The worker expects job records to contain the full path to the staged file.
*   `PROCESSED_AREA_PATH`: Filesystem path for storing intermediate processed files by `pipeline_logic.py` (default: "processed_area").
*   `QUARANTINE_AREA_PATH`: Base filesystem path where quarantined data from failed ingestion jobs will be stored by the worker (default: "quarantined_data" if not set in worker environment). Each failed job's data will typically be placed in a subdirectory named after its `job_id`.
*   `SCANNER_MAX_TOTAL_UNCOMPRESSED_SIZE`: Security scanner - max total uncompressed size for archives (bytes, default: 1GB).
*   `SCANNER_MAX_FILE_COUNT_IN_ZIP`: Security scanner - max number of files in an archive (default: 10000).
*   `SCANNER_MAX_COMPRESSION_RATIO_SINGLE_FILE`: Security scanner - max compression ratio for any single file in an archive (default: 200.0).
*   `SCANNER_MAX_COMPRESSION_RATIO_TOTAL`: Security scanner - max overall compression ratio for an archive (default: 100.0).
*   `COMPLEX_GEOMETRY_STRATEGY`: Strategy for handling complex geometries like "split_all", "filter_types", "explode_collections" (default: "split_all").
*   `CGH_FILTER_TYPES_TO_KEEP`: JSON formatted list of geometry types to retain when using "filter_types" strategy (default: '["Point", "LineString", "Polygon", "MultiPoint", "MultiLineString", "MultiPolygon"]').
*   `CGH_REJECT_IF_TYPES_FOUND`: JSON formatted list of geometry types that will cause job rejection if found, when using "reject_on_types" strategy (default: "[]").
*   `GEOCODING_ADDRESS_FIELDS`: JSON formatted list of property field names to check for addresses when attempting geocoding (default: `["address", "Address", ..., "שדה כתובת"]`).
*   `NOMINATIM_USER_AGENT`: User agent string for Nominatim geocoding requests (default: "spindex-aegis-pipeline/0.1 (developer.contact@example.org)").

Refer to the source code (e.g., `aegis_worker.py`, `pipeline_logic.py`) for defaults if an environment variable is not set.

## Application Structure

*   `main.py`: The main FastAPI application file. Contains API endpoints, Pydantic models, Celery app configuration, and task definitions including the AEGIS pipeline logic.
*   `database.py`: Configures the SQLAlchemy engine and session management.
*   `models.py`: Defines SQLAlchemy ORM models (e.g., `IngestionJobDB`).
*   `crud.py`: Contains CRUD functions for database model interactions.
*   `requirements.txt`: Python dependencies.
*   `Dockerfile`, `entrypoint.sh`: For Docker containerization.
*   `alembic.ini`, `alembic/`: Alembic migration configuration and scripts.
*   `spindex_aegis.db`: SQLite database file (created by Alembic).
*   `staging_area/`: Temporary storage for raw uploaded files.
*   `processed_area/`: Stores files processed by the AEGIS pipeline (e.g., initial GeoJSON conversion, CRS transformed GeoJSON).

**Note on Directory Permissions:** (As previously stated)

## Database Setup (SQLite, SQLAlchemy, Alembic)

(As previously stated)

## Asynchronous Task Processing (Celery & Redis)

(As previously stated)

### Ingestion Task Triggering & AEGIS Pipeline Execution Flow

The `process_staged_file_task` Celery task orchestrates the AEGIS-ECHO pipeline:

1.  **Initial Status:** Metadata status set to `"processing_in_progress"`.
2.  **Format Conversion to GeoJSON:** (As previously described)
3.  **Coordinate Reference System (CRS) Transformation:** (As previously described)
4.  **Geometric Validation:** (As previously described)
5.  **Subsequent Pipeline Stages (currently placeholders or simplified):**
    *   Conditional Geocoding.
    *   Complex Geometry Handling.
    *   Placeholders for Content-Level Security Scanning and Schema & Value-Domain Validation. These currently default to `True` for the purpose of the `overall_success` check.
6.  **Metadata Updates:** Each step logs its outcome and updates relevant fields in the `IngestionJobDB` record.
7.  **Final Status (Data Promotion or Quarantine Logic):**
    *   An `overall_success` flag is determined by the success of all critical preceding stages (Format Conversion, CRS Transformation, Geometric Validation) and placeholder flags for future checks (Security, Schema).
    *   **Data Promotion (if `overall_success` is true):**
        *   `IngestionJobDB.processing_status` is set to `"validation_successful_ready_for_promotion"`.
        *   `IngestionJobDB.location_of_promoted_data` is updated with the path to the final, validated data file.
        *   `IngestionJobDB.promotion_timestamp` is set.
    *   **Data Quarantine (if `overall_success` is false or a critical stage fails earlier):**
        *   If a critical stage (like Format Conversion, CRS Transformation, or a fatal error during Geometric Validation) fails, it immediately updates the job's `processing_status` to a stage-specific failure (e.g., `"format_conversion_failed"`, `"crs_transformation_failed"`, `"geometric_validation_error"`), sets the `quarantine_timestamp`, logs detailed errors to `error_log_references` and `pipeline_stages_executed_log`, and terminates processing for that job. The `location_of_quarantined_data` will point to the file at its last successfully processed state (or the original staged file if failure was very early).
        *   If all explicitly implemented stages pass but a placeholder check (like security or schema validation, which currently default to true) were to be set to false, the final `overall_success` check would fail. In this case:
            *   `IngestionJobDB.processing_status` is set to `"quarantined_validation_failed"`.
            *   `IngestionJobDB.quarantine_timestamp` is set.
            *   `IngestionJobDB.location_of_quarantined_data` is updated.
            *   A summary of which validation step(s) led to this outcome is added to `error_log_references`.

This approach ensures that failures are caught as early as possible, with specific status indicators, and that all unsuccessful processing attempts result in quarantine with appropriate logging.

## Development

(As previously stated)

### Docker Deployment

(As previously stated)

## API Endpoints

(As previously stated)

### Data Upload Endpoint:

*   **POST `/upload/data`**:
    *   (Details as previously documented.)

(Rest of the README remains the same)
