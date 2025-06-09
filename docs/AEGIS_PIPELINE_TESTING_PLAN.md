# AEGIS-ECHO Pipeline End-to-End Testing Plan

## 1. Introduction

### Purpose
This document outlines the end-to-end (E2E) testing plan for the AEGIS-ECHO Data Ingestion Pipeline. The goal is to ensure the pipeline functions correctly from the point of data upload through all processing stages, leading to either successful promotion of data or appropriate quarantine in case of failures.

### Scope
The scope of this testing plan covers the AEGIS-ECHO pipeline as implemented in `backend/main.py`. This includes:
*   Secure Upload/Reception Endpoint (`POST /upload/data`).
*   Initial file validation (size, MIME type).
*   Authentication and Authorization checks.
*   Staging of uploaded files.
*   Asynchronous processing via Celery tasks, specifically the `process_staged_file_task`.
*   Currently implemented pipeline modules:
    *   Format Conversion (e.g., to GeoJSON).
    *   Coordinate Reference System (CRS) Transformation (to EPSG:4326).
    *   Geometric Validation (detecting invalid geometries).
*   Metadata updates in the database, reflecting the job's progress and outcome.
*   Promotion of successfully processed data or quarantine of failed/problematic data.

Future modules (e.g., geocoding, content security scanning, schema validation) will require additions to this plan once implemented.

## 2. Prerequisites/Setup

To execute these E2E tests, the following components and conditions must be met:

*   **Running Services:**
    *   FastAPI application server (`uvicorn backend.main:app ...`).
    *   Celery worker(s) subscribed to the relevant queues (`celery -A backend.main.celery_app worker ...`).
    *   Redis server (as Celery broker and backend).
    *   Database (SQLite for default setup) accessible and schema initialized/migrated.
*   **Sample Data Files:** A collection of test files matching the requirements of various test scenarios (see section 4).
*   **Authentication Tokens:** Valid `X-Auth-Token` values for different user roles (e.g., admin, analyst as per `SECURE_PLACEHOLDER_TOKEN_ADMIN`, `SECURE_PLACEHOLDER_TOKEN_ANALYST` in `backend/main.py`).
*   **Clean Test Environment (Recommended):**
    *   **Database:** For reproducible results, consider resetting the database (e.g., deleting the SQLite `spindex_aegis.db` file and letting Alembic re-initialize it) or clearing relevant job entries before a full test suite run.
    *   **File System:** Clear the `staging_area/` and `processed_area/` directories of any residual files from previous test runs to avoid conflicts or unintended behavior.
*   **API Client:** A tool for making HTTP requests (e.g., `curl`, Postman, or a Python script using `requests`).

## 3. Test Scenarios

### AEGIS-E2E-001: Successful Ingestion (Happy Path)
*   **Description:** Tests the successful processing of a valid geospatial file through all implemented pipeline stages, leading to its promotion.
*   **Test Data Requirements:** A valid geospatial file (e.g., a small, simple GeoJSON file with valid geometries, in a common CRS like WGS84/EPSG:4326 or another easily transformable CRS). Example: `valid_geojson_epsg4326.geojson`.
*   **Test Steps:**
    1.  Upload the test file using `POST /upload/data`. Use a valid `X-Auth-Token` for a user with `upload_data` permission (e.g., admin token).
    2.  Note the `job_id` from the API response (status code 202).
    3.  Poll the `GET /metadata/ingestion_jobs/{job_id}` endpoint (with a valid token) periodically until `processing_status` is no longer "staged_and_queued" or "processing_in_progress". This may take a few seconds for Celery to pick up and process.
*   **Expected Results:**
    *   Step 1: API returns HTTP status 202 Accepted. Response body includes `job_id`, `filename`, and a success message.
    *   Step 3:
        *   Final `processing_status` in the job metadata is "validation_successful_ready_for_promotion".
        *   `promotion_timestamp` is populated.
        *   `location_of_promoted_data` is populated with a path to a file in the `processed_area/` directory (e.g., `..._crs_transformed.geojson`).
        *   `detected_format`, `original_crs`, and `target_crs` fields are correctly populated.
        *   `pipeline_stages_executed_log` shows success for format conversion, CRS transformation, and geometric validation.
        *   `validation_summary.geometric_validation.validation_passed` is true.
*   **Verification Points:**
    *   API responses (status codes, body content).
    *   Database: Query the `ingestion_jobs` table for the specific `job_id` and verify all relevant fields.
    *   File System:
        *   The original file exists in `staging_area/` (with a UUID name).
        *   The promoted file exists at the path specified in `location_of_promoted_data`.
        *   Intermediate files (e.g., `_converted.geojson`) may also exist in `processed_area/`.

### AEGIS-E2E-002: Format Conversion Failure
*   **Description:** Tests the pipeline's response to a file that cannot be converted by GDAL into a recognized geospatial format.
*   **Test Data Requirements:** A file that is not a valid geospatial format (e.g., a plain text file `not_geospatial.txt`, an image `image.png`).
*   **Test Steps:**
    1.  Upload the test file using `POST /upload/data` with a valid admin token.
    2.  Note the `job_id`.
    3.  Poll `GET /metadata/ingestion_jobs/{job_id}`.
*   **Expected Results:**
    *   Step 1: API returns HTTP 202.
    *   Step 3:
        *   `processing_status` is "format_conversion_failed".
        *   `quarantine_timestamp` is populated.
        *   `location_of_quarantined_data` is populated with the path to the original file in the `staging_area/`.
        *   `error_log_references` contains an error related to format conversion.
        *   `pipeline_stages_executed_log` shows failure for "format_conversion_error".
*   **Verification Points:** API responses, Database fields (as listed). File system (original staged file exists).

### AEGIS-E2E-003: CRS Transformation Failure
*   **Description:** Tests pipeline's error handling if CRS transformation fails. This can be hard to trigger with specific data if GDAL is robust. An alternative is to check logs for detailed error messages if a truly problematic file (e.g., one with a highly obscure or fundamentally broken CRS definition that GDAL initially accepts but then fails to transform) cannot be easily crafted.
*   **Test Data Requirements:** Ideally, a GeoJSON file that GDAL can read but reports a CRS that it cannot transform from, or a file whose CRS information is so malformed that transformation logic fails. (Acknowledging difficulty: If specific data is hard to craft, this test might rely more on code path analysis and ensuring robust error logging and status updates for any GDAL-raised exception during this stage).
    *   One approach: A GeoJSON file with a manually edited, nonsensical `crs` field that `ogr.Open` might parse but `osr.CoordinateTransformation` fails on.
*   **Test Steps:**
    1.  Upload the test file using `POST /upload/data` with a valid admin token.
    2.  Note the `job_id`.
    3.  Poll `GET /metadata/ingestion_jobs/{job_id}`.
*   **Expected Results:**
    *   Step 1: API returns HTTP 202.
    *   Step 3:
        *   `processing_status` is "crs_transformation_failed".
        *   `quarantine_timestamp` is populated.
        *   `location_of_quarantined_data` points to the output of the format conversion stage (e.g., `..._converted.geojson` in `processed_area/`).
        *   `error_log_references` contains an error related to CRS transformation.
        *   `pipeline_stages_executed_log` shows success for format conversion but failure for "crs_transformation_error".
*   **Verification Points:** API responses, Database fields. File system (converted file exists, but no CRS transformed file, or transformed file is empty/corrupt if GDAL created one before erroring).

### AEGIS-E2E-004: Geometric Validation Failure (Invalid Geometries)
*   **Description:** Tests the pipeline's ability to detect and quarantine files containing invalid geometries (when not attempting repair).
*   **Test Data Requirements:** A GeoJSON file containing at least one invalid geometry (e.g., a self-intersecting polygon, a polygon with incorrect winding order if that causes issues for Shapely/GEOS). Example: `invalid_self_intersecting_polygon.geojson`.
*   **Test Steps:**
    1.  Upload the test file using `POST /upload/data` with a valid admin token.
    2.  Note the `job_id`.
    3.  Poll `GET /metadata/ingestion_jobs/{job_id}`.
*   **Expected Results:**
    *   Step 1: API returns HTTP 202.
    *   Step 3:
        *   `processing_status` is "quarantined_validation_failed" (or similar, depending on how `overall_success` is determined when `geometric_validation_passed_flag` is false).
        *   `quarantine_timestamp` is populated.
        *   `location_of_quarantined_data` points to the CRS-transformed file (e.g., `..._crs_transformed.geojson`).
        *   `validation_summary.geometric_validation.validation_passed` is false.
        *   `validation_summary.geometric_validation.invalid_features` is greater than 0.
        *   `pipeline_stages_executed_log` shows success for format conversion and CRS transform, but "failure_with_invalids" or similar for geometric validation.
*   **Verification Points:** API responses, Database fields.

### AEGIS-E2E-005: File Upload Validation - Too Large
*   **Description:** Tests the initial file validation for size limits.
*   **Test Data Requirements:** A file (any type) whose size exceeds `MAX_FILE_SIZE` (currently 100MB).
*   **Test Steps:**
    1.  Attempt to upload the large file using `POST /upload/data` with a valid admin token.
*   **Expected Results:**
    *   Step 1: API returns HTTP status 413 Request Entity Too Large. Response detail indicates file size issue. No job is created in the database.
*   **Verification Points:** API response. Database (no new job entry). `staging_area` (file should not be saved).

### AEGIS-E2E-006: File Upload Validation - Disallowed MIME Type
*   **Description:** Tests the initial file validation for allowed MIME types.
*   **Test Data Requirements:** A file with a MIME type not in `ALLOWED_MIME_TYPES` (e.g., `application/pdf` if it's not listed).
*   **Test Steps:**
    1.  Attempt to upload the file using `POST /upload/data` with a valid admin token, ensuring the `Content-Type` header of the file part in the multipart request is set to the disallowed type.
*   **Expected Results:**
    *   Step 1: API returns HTTP status 415 Unsupported Media Type. Response detail indicates MIME type issue. No job is created.
*   **Verification Points:** API response. Database (no new job entry). `staging_area` (file should not be saved).

### AEGIS-E2E-007: Authentication Failure (No Token)
*   **Description:** Tests endpoint protection when no authentication token is provided.
*   **Test Data Requirements:** Any small valid file.
*   **Test Steps:**
    1.  Attempt to upload the file using `POST /upload/data` *without* including the `X-Auth-Token` header.
*   **Expected Results:**
    *   Step 1: API returns HTTP status 401 Unauthorized. Detail indicates missing token. `WWW-Authenticate` header might be present.
*   **Verification Points:** API response.

### AEGIS-E2E-008: Authentication Failure (Invalid Token)
*   **Description:** Tests endpoint protection when an invalid/unknown authentication token is provided.
*   **Test Data Requirements:** Any small valid file.
*   **Test Steps:**
    1.  Attempt to upload the file using `POST /upload/data` with an `X-Auth-Token` header containing a value that is not one of the configured valid tokens (e.g., "invalid-dummy-token").
*   **Expected Results:**
    *   Step 1: API returns HTTP status 401 Unauthorized. Detail indicates invalid credentials.
*   **Verification Points:** API response.

### AEGIS-E2E-009: Authorization Failure (Insufficient Permissions)
*   **Description:** Tests endpoint protection when a user has valid authentication but lacks specific permission for data upload.
*   **Test Data Requirements:** Any small valid file. A user/token that is valid but does not have the "upload_data" permission. (e.g., if "viewer" role exists and doesn't have this permission, use `SECURE_PLACEHOLDER_TOKEN_VIEWER`).
*   **Test Steps:**
    1.  Attempt to upload the file using `POST /upload/data` with an `X-Auth-Token` for a user who lacks the "upload_data" permission (e.g., viewer token).
*   **Expected Results:**
    *   Step 1: API returns HTTP status 403 Forbidden. Detail indicates permission denied.
*   **Verification Points:** API response. (Confirm dummy user roles: `dummy_viewer_role` does not have `upload_data` permission, so this is testable with `SECURE_PLACEHOLDER_TOKEN_VIEWER`).

## 4. Notes on Test Data

*   **Valid GeoJSON:** Create a simple GeoJSON file. Example:
    ```json
    {
      "type": "FeatureCollection",
      "features": [
        {
          "type": "Feature",
          "properties": {},
          "geometry": {
            "type": "Point",
            "coordinates": [102.0, 0.5]
          }
        }
      ]
    }
    ```
*   **Invalid GeoJSON (Self-intersection):**
    ```json
    {
      "type": "Feature",
      "properties": {},
      "geometry": {
        "type": "Polygon",
        "coordinates": [
          [ [0,0], [2,0], [1,1], [2,2], [0,2], [1,1], [0,0] ]
        ]
      }
    }
    ```
    (The point `[1,1]` is repeated, making the polygon self-intersecting).
*   **Large File:** Use a utility like `dd` (Linux/macOS) or `fsutil` (Windows) to create a dummy file of a specific size.
    *   `dd if=/dev/zero of=large_file.bin bs=1M count=101` (creates a 101MB file).
*   **Non-Geospatial File:** Any simple `.txt`, `.jpg`, `.docx` file.
*   **Specific CRS Data:** For testing CRS transformations, you might need to find or create data with a known, less common CRS that GDAL supports. Online resources like [epsg.io](https://epsg.io/) can be helpful for finding CRS details. You can use `ogr2ogr` to convert known valid data into a specific CRS for testing purposes.

## 5. Future Testing Considerations

*   **New Pipeline Modules:** As new modules are integrated into the AEGIS pipeline (e.g., conditional geocoding, complex geometry handling, content-level security scanning, schema/value-domain validation), corresponding E2E test scenarios must be designed and added to this plan. Each new module will introduce new success and failure paths that need verification.
*   **Integration with External Services:** If modules rely on external services (e.g., a geocoding API), tests should include scenarios for service availability and failure, potentially using mocks for reliable testing of failure paths.
*   **Complementary Testing:** E2E tests are crucial for verifying the overall pipeline flow but can be slower and more complex to set up. They should be complemented by:
    *   **Unit Tests:** For individual functions and classes within each module.
    *   **Integration Tests:** For interactions between components (e.g., API endpoint to Celery task dispatching, module to database interaction).
*   **Performance and Scalability Testing:** Once the pipeline is feature-complete, performance and scalability tests will be necessary to understand its behavior under load.
*   **Security Penetration Testing:** Specific security-focused testing should be conducted, especially around file handling, input validation, and interactions with external libraries.
```
