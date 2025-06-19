# AEGIS Pipeline: End-to-End (E2E) Test Plan

This document outlines the strategy, test cases, and automation approach for conducting end-to-end (E2E) testing of the AEGIS Data Ingestion Pipeline.

## 1. Objectives

*   Verify the complete data ingestion flow, from file upload to final disposition (promotion or quarantine).
*   Ensure all pipeline stages (security scanning, format detection/conversion, CRS handling, geocoding, complex geometry processing) function correctly as an integrated system.
*   Validate data integrity and metadata accuracy throughout the pipeline.
*   Confirm that error handling and data quarantine mechanisms work as expected.

## 2. Scope

*   Testing the backend API endpoint for data upload (`/upload/data`).
*   Testing the NATS message queue flow from producer to worker.
*   Testing the AEGIS worker (`aegis_worker.py`) and its invocation of `pipeline_logic.py`.
*   Verification of interactions with the database (`IngestionJobDB`).
*   Verification of file system operations (staging, processing, quarantine, promotion areas).

## 3. Test Environment Prerequisites

*   A running instance of the full application stack:
    *   FastAPI backend application.
    *   NATS server.
    *   AEGIS worker (`aegis_worker.py`) instances.
    *   Database (PostgreSQL or SQLite as configured) with schema migrated via Alembic.
*   All necessary configurations set via environment variables (e.g., `NATS_URL`, `DATABASE_URL`, `STAGING_AREA_PATH`, `PROCESSED_AREA_PATH`, `QUARANTINE_AREA_PATH`, security scanner thresholds, etc.).
*   Network access for services like geocoding (if using external Nominatim).
*   A collection of test data files specifically designed for each test case.

## 4. Test Automation Approach

### 4.1. Recommended Tools
*   **Test Runner:** `pytest`
*   **HTTP Client (API):** `httpx` (for file uploads and interacting with any job status APIs)
*   **Database Interaction:** `SQLAlchemy` (using backend models for verification)
*   **Filesystem Operations:** Standard Python `os`, `shutil` modules.

### 4.2. Test Script Structure (Example)
*   **Test Directory:** A new top-level directory, e.g., `e2e_tests/`.
*   **`e2e_tests/conftest.py`:** For `pytest` fixtures (DB sessions, API client, test data paths, helper functions for upload/polling).
*   **`e2e_tests/test_data/`:** To store small, curated input files for test cases.
*   **Test Modules:** Separate Python files for different categories of tests, e.g.:
    *   `e2e_tests/test_successful_ingestion.py`
    *   `e2e_tests/test_pipeline_failures_quarantine.py`
    *   `e2e_tests/test_specific_modules.py` (e.g., for different geocoding or complex geometry scenarios)

### 4.3. Key Test Script Components
*   **File Upload:** Function to upload a test file to the `/upload/data` endpoint and retrieve the `job_id`.
*   **Job Status Polling:** Mechanism to periodically check the job's status in the `IngestionJobDB` until a terminal state (`PROMOTED`, `COMPLETED`, `QUARANTINED`, specific `FAILED_...` states) is reached or a timeout occurs.
*   **Assertions:**
    *   Verify `IngestionJobDB.processing_status`.
    *   Verify paths (`location_of_promoted_data`, `location_of_quarantined_data`, `intermediate_geojson_path`).
    *   Verify content of `validation_summary` and `pipeline_stages_executed_log` for key messages or error details.
    *   Verify file existence/absence in staging, processed, quarantine, and promotion areas.

### 4.4. Helper API Endpoints (Recommended)
For more robust and cleaner test automation, the following API endpoints would be beneficial:
*   `GET /jobs/{job_id}/status`: Returns current job status and key identifying information.
*   `GET /jobs/{job_id}/details`: Returns comprehensive details of the job (e.g., the full database record).

## 5. E2E Test Cases

The following are representative E2E test cases. Each assumes the prerequisites are met.

---
**Test Case ID:** E2E_SCS_001
**Description:** Successful ingestion of a valid GeoJSON file with WGS84 coordinates.
**Input Data:** `valid_geojson_wgs84.geojson` (FeatureCollection with Point, LineString, Polygon features in EPSG:4326, simple valid geometries, no addresses).
**Execution Steps:** Upload file, monitor job.
**Expected Outcome:**
    *   DB Status: `PROMOTED` (or `COMPLETED`).
    *   DB Fields: `detected_format` ("GeoJSON"), `original_crs` (WGS84 WKT), `target_crs` ("EPSG:4326"), `location_of_promoted_data` set.
    *   `validation_summary` and `pipeline_stages_executed_log` show success.
    *   Filesystem: Promoted file exists; staged file removed.

---
**Test Case ID:** E2E_SCS_002
**Description:** Successful ingestion of a Shapefile (zipped) with a projected CRS, requiring reprojection.
**Input Data:** `shapefile_projected.zip` (containing `.shp`, `.shx`, `.dbf`, `.prj`; data in a projected CRS like EPSG:3857).
**Execution Steps:** Upload file, monitor job.
**Expected Outcome:**
    *   DB Status: `PROMOTED` (or `COMPLETED`).
    *   DB Fields: `detected_format` ("ESRI Shapefile"), `original_crs` (input projected CRS WKT), `target_crs` ("EPSG:4326"), `location_of_promoted_data` set, `intermediate_geojson_path` (reprojected GeoJSON) set.
    *   Filesystem: Promoted file exists; intermediate GeoJSON exists; staged file removed.

---
**Test Case ID:** E2E_SCS_003
**Description:** Successful ingestion of CSV with address fields, requiring geocoding.
**Input Data:** `addresses.csv` (CSV with ID, name, and an address field like "address" containing valid, geocodable addresses).
**Execution Steps:** Upload file, monitor job.
**Expected Outcome:**
    *   DB Status: `PROMOTED` (or `COMPLETED`).
    *   DB Fields: `detected_format` ("CSV"), `geocoding_summary` shows `geocoding_successes > 0`. `intermediate_geojson_path` contains GeoJSON with Point geometries.
    *   Filesystem: Promoted file exists.

---
**Test Case ID:** E2E_SCS_004
**Description:** Test `COMPLEX_GEOMETRY_STRATEGY="split_all"` (default).
**Input Data:** `complex_geoms.geojson` (GeoJSON with a `GeometryCollection` and a `MultiPolygon`).
**Execution Steps:** Ensure default strategy, upload file, monitor.
**Expected Outcome:**
    *   DB Status: `PROMOTED` (or `COMPLETED`).
    *   DB Fields: `complex_geometry_summary` shows `collections_exploded > 0`, `multis_split > 0`, `feature_count_after` > `feature_count_before`. `intermediate_geojson_path` contains split features.

---
**Test Case ID:** E2E_FAIL_001
**Description:** Job quarantined due to unrecognized file format.
**Input Data:** `invalid_format.txt` (or text file renamed to `data.geojson`).
**Execution Steps:** Upload file, monitor.
**Expected Outcome:**
    *   DB Status: `QUARANTINED`.
    *   DB Fields: `validation_summary` contains `format_detection_error`. `location_of_quarantined_data` set. `quarantine_timestamp` set.
    *   Filesystem: Input file moved to quarantine location.
    *   `pipeline_stages_executed_log` shows format detection failure and quarantine entry.

---
**Test Case ID:** E2E_FAIL_002
**Description:** Job quarantined due to security scan failure.
**Input Data:** `test_zip_bomb_small.zip` (crafted to trigger configured scanner thresholds).
**Execution Steps:** Configure scanner thresholds for test, upload file, monitor, revert thresholds.
**Expected Outcome:**
    *   DB Status: `QUARANTINED` (or `FAILED_SECURITY_SCAN` then quarantined).
    *   DB Fields: `validation_summary` contains security scan report with `scan_status: "failure_threat_detected"`. `location_of_quarantined_data` set.
    *   Filesystem: File moved to quarantine.
    *   `pipeline_stages_executed_log` shows security scan failure and quarantine entry.

---
**Test Case ID:** E2E_FAIL_003
**Description:** Job quarantined due to `COMPLEX_GEOMETRY_STRATEGY="reject_on_types"`.
**Input Data:** `contains_geometrycollection.geojson` (contains a `GeometryCollection`).
**Execution Steps:** Set env vars `COMPLEX_GEOMETRY_STRATEGY="reject_on_types"` and `CGH_REJECT_IF_TYPES_FOUND='["GeometryCollection"]'`, restart worker, upload file, monitor, revert env vars.
**Expected Outcome:**
    *   DB Status: `QUARANTINED`.
    *   DB Fields: `validation_summary` contains `complex_geometry_error`. `complex_geometry_summary` shows `job_failed_due_to_rejected_type: true`.
    *   Filesystem: File moved to quarantine.

---
*(Additional test cases for other formats, CRS issues, geocoding edge cases, specific module failures, and validation steps should be added as the pipeline evolves.)*

## 6. Test Execution and Reporting

*   Tests should be executable via `pytest` from the project root or `e2e_tests` directory.
*   Test environment (FastAPI app, NATS, worker, DB) must be running independently. Consider Docker Compose for managing this test environment.
*   Pytest will generate a report of pass/fail status for each test case.
*   Detailed logs from the backend, worker, and NATS should be collected, especially for failed tests.

This E2E test plan provides a starting point for ensuring the AEGIS pipeline's reliability and correctness. It should be expanded and maintained as new features are added or existing functionalities change.
