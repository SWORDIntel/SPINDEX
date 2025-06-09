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
