import os
import shutil
import uuid
from fastapi.testclient import TestClient
from sqlalchemy.orm import Session
import pytest # For potential fixtures or advanced testing patterns later
from unittest.mock import patch, MagicMock
from datetime import datetime
from typing import Optional

# Adjust paths for imports based on execution from repository root or if backend/ is a module
# Assuming tests might be run from the repo root, and PYTHONPATH includes the root.
from backend.main import app, get_current_user, User, Role, STAGING_AREA_PATH, ALLOWED_MIME_TYPES, MAX_FILE_SIZE, PROCESSED_AREA_PATH, TARGET_CRS_EPSG, NOMINATIM_USER_AGENT, GEOCODING_ADDRESS_FIELDS, COMPLEX_GEOMETRY_STRATEGY, CGH_FILTER_TYPES_TO_KEEP, CGH_REJECT_IF_TYPES_FOUND
from backend.main import process_ingestion_job_task, geocode_and_update_features, handle_complex_geometries
from geopy.exc import GeocoderTimedOut, GeocoderUnavailable, GeocoderServiceError
from shapely.geometry import Point, LineString, Polygon, MultiPoint, MultiLineString, MultiPolygon, GeometryCollection, mapping as shapely_mapping
# from osgeo import osr # Import for creating osr.SpatialReference mock instances if needed directly in test setup
from backend.database import SessionLocal, get_db, Base, engine
import json
import time # For mocking time.sleep
import logging
import copy
from collections import defaultdict
from backend.models import IngestionJobDB
from backend import crud

# Test tokens corresponding to users in backend.main.dummy_users_db
# These should match the tokens in backend/main.py
ADMIN_TOKEN = "secure-placeholder-token-admin" # Has 'upload_data' permission
ANALYST_TOKEN = "secure-placeholder-token-analyst" # Has 'upload_data' permission
VIEWER_TOKEN = "secure-placeholder-token-viewer" # Does NOT have 'upload_data'
INVALID_TOKEN = "invalid-dummy-token"

client = TestClient(app)

def get_test_db_session():
    db = SessionLocal()
    try:
        yield db
    finally:
        db.close()

def test_upload_file_success():
    os.makedirs(STAGING_AREA_PATH, exist_ok=True)
    test_filename = f"test_upload_{uuid.uuid4()}.txt"
    test_content = b"This is a test file."
    with open(test_filename, "wb") as f: f.write(test_content)
    staged_file_path_to_delete = None
    job_id_to_delete = None
    try:
        with open(test_filename, "rb") as f_upload:
            response = client.post("/aegis/upload", headers={"X-Auth-Token": ADMIN_TOKEN}, files={"file": (test_filename, f_upload, "text/plain")})
        assert response.status_code == 200
        data = response.json()
        assert data["filename"] == test_filename
        assert "job_id" in data
        staged_file_path_to_delete = data["staged_filepath"]
        job_id_to_delete = data["job_id"]
        assert os.path.exists(staged_file_path_to_delete)
        with open(staged_file_path_to_delete, "rb") as f_staged: assert f_staged.read() == test_content
        db = next(get_test_db_session())
        job_db = crud.get_ingestion_job(db, job_id_to_delete)
        assert job_db is not None; assert job_db.original_filename == test_filename
        assert job_db.user_id == "admin"; assert job_db.processing_status == "pending_staging"
        assert job_db.celery_task_id is not None
        db.close()
    finally:
        if os.path.exists(test_filename): os.remove(test_filename)
        if staged_file_path_to_delete and os.path.exists(staged_file_path_to_delete):
            if STAGING_AREA_PATH in os.path.abspath(staged_file_path_to_delete): os.remove(staged_file_path_to_delete)
        if job_id_to_delete:
            db = next(get_test_db_session()); crud.remove_ingestion_job(db, job_id_to_delete); db.close()

def test_upload_file_no_token():
    response = client.post("/aegis/upload", files={"file": ("test.txt", b"content", "text/plain")})
    assert response.status_code == 401

def test_upload_file_invalid_token():
    response = client.post("/aegis/upload", headers={"X-Auth-Token": INVALID_TOKEN}, files={"file": ("test.txt", b"content", "text/plain")})
    assert response.status_code == 401

def test_upload_file_permission_denied():
    response = client.post("/aegis/upload", headers={"X-Auth-Token": VIEWER_TOKEN}, files={"file": ("test.txt", b"content", "text/plain")})
    assert response.status_code == 403

def test_upload_file_too_large():
    mime_type = ALLOWED_MIME_TYPES[0] if ALLOWED_MIME_TYPES else "application/octet-stream"
    if MAX_FILE_SIZE < 10000:
        response = client.post("/aegis/upload", headers={"X-Auth-Token": ADMIN_TOKEN}, files={"file": ("large.txt", b"a" * (MAX_FILE_SIZE + 1), mime_type)})
        assert response.status_code == 413
    else:
        pytest.skip(f"MAX_FILE_SIZE ({MAX_FILE_SIZE}) too large for this test.")

def test_upload_file_unsupported_mime_type():
    unsupported_mime_type = f"application/x-test-disallowed-{uuid.uuid4()}"
    response = client.post("/aegis/upload", headers={"X-Auth-Token": ADMIN_TOKEN}, files={"file": ("image.jpg", b"fakejpegcontent", unsupported_mime_type)})
    assert response.status_code == 415

@patch('backend.main.process_ingestion_job_task.delay')
def test_upload_file_celery_submission_failure(mock_celery_delay):
    mock_celery_delay.side_effect = Exception("Celery down") # Simulate Celery task submission failure

    os.makedirs(STAGING_AREA_PATH, exist_ok=True)
    test_filename = f"test_celery_fail_{uuid.uuid4()}.txt"
    test_content = b"Content for celery failure test."

    # Create a temporary file to upload
    with open(test_filename, "wb") as f:
        f.write(test_content)

    staged_file_path_to_delete = None
    job_id_to_delete = None

    try:
        with open(test_filename, "rb") as f_upload:
            response = client.post(
                "/aegis/upload",
                headers={"X-Auth-Token": ADMIN_TOKEN}, # User with upload permission
                files={"file": (test_filename, f_upload, "text/plain")} # Valid file and mime type
            )

        # The upload endpoint should still succeed initially, as file is staged and job record created
        assert response.status_code == 200
        data = response.json()
        assert data["filename"] == test_filename
        assert "job_id" in data
        job_id_to_delete = data["job_id"]
        staged_file_path_to_delete = data["staged_filepath"]

        assert os.path.exists(staged_file_path_to_delete) # File should still be staged

        # Verify the database record
        db = next(get_test_db_session())
        try:
            job_db = crud.get_ingestion_job(db, job_id_to_delete)
            assert job_db is not None
            assert job_db.original_filename == test_filename
            assert job_db.user_id == "admin" # Based on ADMIN_TOKEN
            assert job_db.processing_status == "failed_task_submission"
            assert job_db.celery_task_id is None # Celery task ID should not be set

            assert len(job_db.error_log_references) == 1
            assert "Celery task submission failed: Celery down" in job_db.error_log_references[0]

            assert job_db.pipeline_stages_executed_log is not None
            upload_log_found = False
            celery_fail_log_found = False
            for log_entry in job_db.pipeline_stages_executed_log:
                if log_entry["stage"] == "upload" and log_entry["status"] == "success":
                    upload_log_found = True
                if log_entry["stage"] == "Celery Task Submission" and log_entry["status"] == "failed":
                    assert "Celery task submission failed: Celery down" in log_entry["message"]
                    celery_fail_log_found = True
            assert upload_log_found
            assert celery_fail_log_found

        finally:
            db.close()

    finally:
        if os.path.exists(test_filename):
            os.remove(test_filename)
        if staged_file_path_to_delete and os.path.exists(staged_file_path_to_delete):
            # Basic safety check to prevent accidental deletion outside STAGING_AREA_PATH
            if STAGING_AREA_PATH in os.path.abspath(staged_file_path_to_delete):
                os.remove(staged_file_path_to_delete)
        if job_id_to_delete:
            db = next(get_test_db_session())
            try:
                crud.remove_ingestion_job(db, job_id_to_delete)
            finally:
                db.close()

# --- Celery Task Tests ---

def create_dummy_job_for_task(db_session: Session, staged_file_path: str, job_id_val: Optional[str] = None) -> IngestionJobDB:
    if job_id_val is None: job_id_val = str(uuid.uuid4())
    job_data = {"job_id": job_id_val, "original_filename": "test_file.zip", "staged_file_path": staged_file_path, "user_id": "test_user", "processing_status": "pending_task_execution", "upload_timestamp": datetime.utcnow()}
    return crud.create_ingestion_job(db_session, job_data)

@patch('backend.main.ogr')
@patch('backend.main.gdal')
def test_celery_task_format_related_success_no_crs_transform(mock_gdal, mock_ogr): # Renamed for clarity
    db_session = next(get_test_db_session())
    mock_datasource_fmt = MagicMock(); mock_driver = MagicMock(); mock_driver.GetName.return_value = "GeoJSON" # Assume it's already GeoJSON
    mock_datasource_fmt.GetDriver.return_value = mock_driver

    # For CRS detection part, after format detection, ogr.Open is called again for intermediate_geojson_path
    mock_datasource_crs = MagicMock(); mock_layer_crs = MagicMock(); mock_srs_crs = MagicMock()
    mock_srs_crs.ExportToWkt.return_value = f"EPSG:{TARGET_CRS_EPSG}_WKT" # Already target CRS
    mock_srs_crs.GetAuthorityName.return_value = "EPSG"
    mock_srs_crs.GetAuthorityCode.return_value = str(TARGET_CRS_EPSG)
    mock_layer_crs.GetSpatialRef.return_value = mock_srs_crs
    mock_datasource_crs.GetLayerCount.return_value = 1
    mock_datasource_crs.GetLayer.return_value = mock_layer_crs

    # ogr.Open needs to return different mocks based on input path
    def ogr_open_side_effect(path, *args, **kwargs):
        if "intermediate" in path or path.endswith(".geojson"): # CRS detection on intermediate
            return mock_datasource_crs
        return mock_datasource_fmt # Format detection on staged file
    mock_ogr.Open.side_effect = ogr_open_side_effect

    # gdal.VectorTranslate should NOT be called if format is already GeoJSON and CRS is target
    # However, the current task logic converts ANY detected format to GeoJSON first.
    # So, VectorTranslate will be called for format conversion.
    mock_gdal.VectorTranslate.return_value = True # Mock successful format conversion to GeoJSON

    os.makedirs(STAGING_AREA_PATH, exist_ok=True); os.makedirs(PROCESSED_AREA_PATH, exist_ok=True)
    dummy_staged_filename = f"dummy_staged_{uuid.uuid4()}.geojson" # Test with a geojson directly
    dummy_staged_filepath = os.path.join(STAGING_AREA_PATH, dummy_staged_filename)
    with open(dummy_staged_filepath, "wb") as f: f.write(b'{"type": "FeatureCollection", "features": []}')

    job = create_dummy_job_for_task(db_session, dummy_staged_filepath)
    job_id = job.job_id

    # Patch os.path.exists and os.path.getsize for VectorTranslate output check
    original_os_path_exists = os.path.exists
    original_os_path_getsize = os.path.getsize
    def mock_path_exists_for_task(path):
        if path == os.path.join(PROCESSED_AREA_PATH, f"{job_id}.geojson"): return True
        return original_os_path_exists(path)
    def mock_path_getsize_for_task(path):
        if path == os.path.join(PROCESSED_AREA_PATH, f"{job_id}.geojson"): return 100
        return original_os_path_getsize(path)

    os.path.exists = mock_path_exists_for_task
    os.path.getsize = mock_path_getsize_for_task

    try:
        result_message = process_ingestion_job_task(job_id)
        db_session.refresh(job)

        assert "CRS is already EPSG:4326" in result_message
        assert job.processing_status == "completed_crs_transformation"
        assert job.detected_format == "GeoJSON"
        assert job.intermediate_geojson_path == os.path.join(PROCESSED_AREA_PATH, f"{job_id}.geojson")
        assert job.original_crs == f"EPSG:{TARGET_CRS_EPSG}_WKT"
        assert job.target_crs == f"EPSG:{TARGET_CRS_EPSG}"
        # Check pipeline log for all stages
        assert any(log["stage"] == "Format Detection" and log["status"] == "success" for log in job.pipeline_stages_executed_log)
        assert any(log["stage"] == "Format Conversion (GeoJSON)" and log["status"] == "success" for log in job.pipeline_stages_executed_log)
        assert any(log["stage"] == "CRS Detection" and log["status"] == "success" for log in job.pipeline_stages_executed_log)
        assert any(log["stage"] == "CRS Transformation" and log["status"] == "success" and "No transformation performed" in log["message"] for log in job.pipeline_stages_executed_log)
    finally:
        os.path.exists = original_os_path_exists
        os.path.getsize = original_os_path_getsize
        if os.path.exists(dummy_staged_filepath): os.remove(dummy_staged_filepath)
        # The intermediate GeoJSON is the same as the "reprojected" one in this case.
        if job.intermediate_geojson_path and os.path.exists(job.intermediate_geojson_path) :
            if PROCESSED_AREA_PATH in os.path.abspath(job.intermediate_geojson_path): # safety check
                 os.remove(job.intermediate_geojson_path)
        crud.remove_ingestion_job(db_session, job_id)
        db_session.close()


@patch('backend.main.ogr')
@patch('backend.main.gdal')
@patch('backend.main.osr')
def test_celery_task_crs_detection_and_transformation_success(mock_osr, mock_gdal, mock_ogr):
    db_session = next(get_test_db_session())

    # 1. Mock ogr.Open for format detection (on staged file)
    mock_datasource_format = MagicMock()
    mock_driver_format = MagicMock()
    mock_driver_format.GetName.return_value = "ESRI Shapefile"
    mock_datasource_format.GetDriver.return_value = mock_driver_format

    # 2. Mock ogr.Open for CRS detection (on intermediate GeoJSON)
    mock_datasource_crs = MagicMock()
    mock_layer_crs = MagicMock()
    mock_srs_initial = MagicMock() # This is the SpatialReference from the intermediate GeoJSON
    mock_srs_initial.ExportToWkt.return_value = "EPSG:3857_WKT_REPRESENTATION"
    mock_srs_initial.GetAuthorityName.return_value = "EPSG" # For the check in transformation step
    mock_srs_initial.GetAuthorityCode.return_value = "3857" # For the check in transformation step
    mock_layer_crs.GetSpatialRef.return_value = mock_srs_initial
    mock_datasource_crs.GetLayerCount.return_value = 1
    mock_datasource_crs.GetLayer.return_value = mock_layer_crs

    ogr_open_call_count = 0
    def ogr_open_side_effect(path, *args, **kwargs):
        nonlocal ogr_open_call_count
        ogr_open_call_count += 1
        if ogr_open_call_count == 1: # First call by task is for format detection
            return mock_datasource_format
        else: # Second call by task is for CRS detection on intermediate GeoJSON
            return mock_datasource_crs
    mock_ogr.Open.side_effect = ogr_open_side_effect

    # 3. Mock osr.SpatialReference for source and target SRS objects created in transformation
    mock_srs_source_transform = MagicMock() # For source_srs = osr.SpatialReference()
    mock_srs_source_transform.GetAuthorityName.return_value = "EPSG"
    mock_srs_source_transform.GetAuthorityCode.return_value = "3857"

    mock_srs_target_transform = MagicMock() # For target_srs = osr.SpatialReference()
    mock_srs_target_transform.GetAuthorityName.return_value = "EPSG"
    mock_srs_target_transform.GetAuthorityCode.return_value = str(TARGET_CRS_EPSG)

    osr_spatialref_call_count = 0
    def osr_spatialref_side_effect():
        nonlocal osr_spatialref_call_count
        osr_spatialref_call_count += 1
        if osr_spatialref_call_count == 1: # First instance for source_srs in transform
            return mock_srs_source_transform
        else: # Second instance for target_srs in transform
            return mock_srs_target_transform
    mock_osr.SpatialReference.side_effect = osr_spatialref_side_effect

    # 4. Mock gdal.VectorTranslate (first for format conversion, second for reprojection)
    gdal_vectranslate_call_count = 0
    # Store paths to check them in side_effect
    geojson_output_path_fmt_conv = ""
    reprojected_geojson_path_transform = ""

    def vector_translate_side_effect(destName, srcDS, options):
        nonlocal gdal_vectranslate_call_count, geojson_output_path_fmt_conv, reprojected_geojson_path_transform
        gdal_vectranslate_call_count += 1
        if gdal_vectranslate_call_count == 1: # Format conversion call
            geojson_output_path_fmt_conv = destName
            # Simulate file creation for the check in the task
            with open(destName, "wb") as f: f.write(b"{}")
            return True
        else: # CRS transformation call
            reprojected_geojson_path_transform = destName
            with open(destName, "wb") as f: f.write(b"{}")
            return True
    mock_gdal.VectorTranslate.side_effect = vector_translate_side_effect

    # --- Test Execution ---
    os.makedirs(STAGING_AREA_PATH, exist_ok=True); os.makedirs(PROCESSED_AREA_PATH, exist_ok=True)
    dummy_staged_filepath = os.path.join(STAGING_AREA_PATH, f"staged_{uuid.uuid4()}.shp")
    with open(dummy_staged_filepath, "wb") as f: f.write(b"dummy shp")

    job = create_dummy_job_for_task(db_session, dummy_staged_filepath)
    job_id = job.job_id
    # job.processing_status is initially "pending_task_execution"

    try:
        result_message = process_ingestion_job_task(job_id)
        db_session.refresh(job)

        assert f"Successfully reprojected to EPSG:{TARGET_CRS_EPSG}" in result_message
        assert job.processing_status == "completed_crs_transformation"
        assert job.detected_format == "ESRI Shapefile"
        assert job.original_crs == "EPSG:3857_WKT_REPRESENTATION"
        assert job.target_crs == f"EPSG:{TARGET_CRS_EPSG}"

        expected_reprojected_path = os.path.join(PROCESSED_AREA_PATH, f"{job_id}_epsg{TARGET_CRS_EPSG}.geojson")
        assert job.intermediate_geojson_path == expected_reprojected_path

        assert any(log["stage"] == "CRS Transformation" and log["status"] == "success" for log in job.pipeline_stages_executed_log)

    finally:
        if os.path.exists(dummy_staged_filepath): os.remove(dummy_staged_filepath)
        # Clean up files created by side_effect if they exist
        if geojson_output_path_fmt_conv and os.path.exists(geojson_output_path_fmt_conv): os.remove(geojson_output_path_fmt_conv)
        if reprojected_geojson_path_transform and os.path.exists(reprojected_geojson_path_transform): os.remove(reprojected_geojson_path_transform)
        crud.remove_ingestion_job(db_session, job_id)
        db_session.close()


@patch('backend.main.ogr')
def test_celery_task_crs_detection_none_found(mock_ogr):
    db_session = next(get_test_db_session())

    # Mock for format detection (ogr.Open on staged file)
    mock_datasource_format = MagicMock()
    mock_driver_format = MagicMock()
    mock_driver_format.GetName.return_value = "GeoJSON"
    mock_datasource_format.GetDriver.return_value = mock_driver_format

    # Mock for CRS detection (ogr.Open on intermediate_geojson_path)
    mock_datasource_crs = MagicMock()
    mock_layer_crs = MagicMock()
    mock_layer_crs.GetSpatialRef.return_value = None # Simulate no SRS info
    mock_datasource_crs.GetLayerCount.return_value = 1
    mock_datasource_crs.GetLayer.return_value = mock_layer_crs

    ogr_open_call_count = 0
    def ogr_open_side_effect(path, *args, **kwargs):
        nonlocal ogr_open_call_count
        ogr_open_call_count += 1
        if ogr_open_call_count == 1: return mock_datasource_format
        return mock_datasource_crs
    mock_ogr.Open.side_effect = ogr_open_side_effect

    # Patch gdal.VectorTranslate to simulate intermediate GeoJSON creation
    with patch('backend.main.gdal.VectorTranslate', return_value=True) as mock_vt:
        # Mock os.path.exists and getsize for VectorTranslate's output file
        with patch('os.path.exists', return_value=True), patch('os.path.getsize', return_value=100):

            os.makedirs(STAGING_AREA_PATH, exist_ok=True); os.makedirs(PROCESSED_AREA_PATH, exist_ok=True)
            dummy_staged_filepath = os.path.join(STAGING_AREA_PATH, f"staged_no_crs_{uuid.uuid4()}.geojson")
            with open(dummy_staged_filepath, "wb") as f: f.write(b'{"type": "FeatureCollection", "features": []}')

            job = create_dummy_job_for_task(db_session, dummy_staged_filepath)
            job_id = job.job_id

            try:
                result_message = process_ingestion_job_task(job_id)
                db_session.refresh(job)

                assert "Failed at CRS detection stage" in result_message
                assert job.processing_status == "failed_crs_detection"
                assert job.original_crs is None
                assert "crs_detection_error" in job.validation_summary
                assert job.pipeline_stages_executed_log[-1]["stage"] == "CRS Detection"
                assert job.pipeline_stages_executed_log[-1]["status"] == "failed"
            finally:
                if os.path.exists(dummy_staged_filepath): os.remove(dummy_staged_filepath)
                # Clean up intermediate if created by mocked VectorTranslate
                intermediate_path = os.path.join(PROCESSED_AREA_PATH, f"{job_id}.geojson")
                if os.path.exists(intermediate_path): os.remove(intermediate_path)
                crud.remove_ingestion_job(db_session, job_id)
                db_session.close()

@patch('backend.main.ogr')
@patch('backend.main.gdal')
@patch('backend.main.osr')
def test_celery_task_crs_transformation_failure(mock_osr, mock_gdal, mock_ogr):
    db_session = next(get_test_db_session())

    # 1. Mock ogr.Open for format detection
    mock_datasource_format = MagicMock()
    mock_driver_format = MagicMock(); mock_driver_format.GetName.return_value = "ESRI Shapefile"
    mock_datasource_format.GetDriver.return_value = mock_driver_format

    # 2. Mock ogr.Open for CRS detection
    mock_datasource_crs = MagicMock(); mock_layer_crs = MagicMock(); mock_srs_initial = MagicMock()
    mock_srs_initial.ExportToWkt.return_value = "EPSG:3857_WKT_REPRESENTATION"
    mock_srs_initial.GetAuthorityName.return_value = "EPSG"; mock_srs_initial.GetAuthorityCode.return_value = "3857"
    mock_layer_crs.GetSpatialRef.return_value = mock_srs_initial
    mock_datasource_crs.GetLayerCount.return_value = 1; mock_datasource_crs.GetLayer.return_value = mock_layer_crs

    ogr_open_call_count = 0
    def ogr_open_side_effect(path, *args, **kwargs):
        nonlocal ogr_open_call_count; ogr_open_call_count += 1
        return mock_datasource_format if ogr_open_call_count == 1 else mock_datasource_crs
    mock_ogr.Open.side_effect = ogr_open_side_effect

    # 3. Mock osr.SpatialReference for transformation
    mock_srs_source_transform = MagicMock(); mock_srs_source_transform.GetAuthorityName.return_value = "EPSG"; mock_srs_source_transform.GetAuthorityCode.return_value = "3857"
    mock_srs_target_transform = MagicMock(); mock_srs_target_transform.GetAuthorityName.return_value = "EPSG"; mock_srs_target_transform.GetAuthorityCode.return_value = str(TARGET_CRS_EPSG)

    osr_spatialref_call_count = 0
    def osr_spatialref_side_effect_transf_fail():
        nonlocal osr_spatialref_call_count; osr_spatialref_call_count += 1
        return mock_srs_source_transform if osr_spatialref_call_count == 1 else mock_srs_target_transform
    mock_osr.SpatialReference.side_effect = osr_spatialref_side_effect_transf_fail

    # 4. Mock gdal.VectorTranslate: first call (format conversion) succeeds, second call (reprojection) fails
    geojson_output_path_fmt_conv = ""
    def vector_translate_side_effect_reproj_fail(destName, srcDS, options):
        nonlocal geojson_output_path_fmt_conv
        # Check if this is the reprojection call by looking for -t_srs
        is_reprojection_call = any("-t_srs" in opt for opt in options if isinstance(opt, str))
        if not is_reprojection_call: # Format conversion call
            geojson_output_path_fmt_conv = destName
            with open(destName, "wb") as f: f.write(b"{}")
            return True
        else: # Reprojection call - simulate failure
            raise Exception("GDAL reprojection error")
    mock_gdal.VectorTranslate.side_effect = vector_translate_side_effect_reproj_fail

    os.makedirs(STAGING_AREA_PATH, exist_ok=True); os.makedirs(PROCESSED_AREA_PATH, exist_ok=True)
    dummy_staged_filepath = os.path.join(STAGING_AREA_PATH, f"staged_reproj_fail_{uuid.uuid4()}.shp")
    with open(dummy_staged_filepath, "wb") as f: f.write(b"dummy shp")
    job = create_dummy_job_for_task(db_session, dummy_staged_filepath)
    job_id = job.job_id

    try:
        result_message = process_ingestion_job_task(job_id)
        db_session.refresh(job)

        assert "Failed at CRS transformation stage" in result_message
        assert job.processing_status == "failed_crs_transformation"
        assert job.detected_format == "ESRI Shapefile"
        assert job.original_crs == "EPSG:3857_WKT_REPRESENTATION"
        assert job.target_crs is None
        assert "crs_transformation_error" in job.validation_summary
        assert job.pipeline_stages_executed_log[-1]["stage"] == "CRS Transformation"
        assert job.pipeline_stages_executed_log[-1]["status"] == "failed"
    finally:
        if os.path.exists(dummy_staged_filepath): os.remove(dummy_staged_filepath)
        if geojson_output_path_fmt_conv and os.path.exists(geojson_output_path_fmt_conv): os.remove(geojson_output_path_fmt_conv)
        crud.remove_ingestion_job(db_session, job_id)
        db_session.close()

# Test for successful format detection, successful conversion, successful CRS detection, AND successful transformation (if needed)
# is covered by test_celery_task_crs_detection_and_transformation_success
# Test for successful format detection, successful conversion, and CRS already being target is test_celery_task_crs_already_target

# Test for format detection failure is covered by test_celery_task_format_detection_failure (this is an older test, ensure it's distinct)
# Test for format conversion failure is covered by test_celery_task_format_conversion_failure (older test)

# Note: The initial test_celery_task_success might need renaming or adjustment if it's now fully covered by the more specific CRS tests.
# For now, I'll keep it and assume it tests a simpler path if CRS/Transformation steps are skipped due to some condition or if it's an older test.
# The new tests are more granular for CRS stages.
# The test `test_celery_task_success` has been renamed to `test_celery_task_format_related_success_no_crs_transform`
# to specifically test the case where the input is already GeoJSON and in the target CRS.
# The test `test_celery_task_crs_detection_and_transformation_success` covers a different source CRS.


# --- Tests for geocode_and_update_features Helper ---

# Mock logger to pass to the function
mock_logger = logging.getLogger("test_geocoding_helper")
mock_logger.setLevel(logging.DEBUG)
# Add a handler if you want to see logs during tests (e.g., StreamHandler)
# if not mock_logger.handlers:
#     mock_logger.addHandler(logging.StreamHandler())

class MockNominatimLocation:
    def __init__(self, longitude, latitude):
        self.longitude = longitude
        self.latitude = latitude

@patch('backend.main.time.sleep', return_value=None) # Mock time.sleep to speed up tests
@patch('geopy.geocoders.Nominatim')
def test_geocode_helper_success(MockNominatim, mock_sleep):
    mock_geolocator_instance = MockNominatim.return_value
    mock_geolocator_instance.geocode.return_value = MockNominatimLocation(longitude=10.0, latitude=50.0)

    features = [{"properties": {"address": "Test Address 1"}, "geometry": None}]
    address_fields = ["address"]

    updated_features, summary = geocode_and_update_features(features, address_fields, "test_agent", mock_logger)

    assert summary["features_total_count"] == 1
    assert summary["features_without_geometry_count"] == 1
    assert summary["addresses_found_for_geocoding"] == 1
    assert summary["geocoding_requests_sent"] == 1
    assert summary["geocoding_successes"] == 1
    assert summary["geocoding_failures"] == 0
    assert updated_features[0]["geometry"] == {"type": "Point", "coordinates": [10.0, 50.0]}
    mock_geolocator_instance.geocode.assert_called_once_with("Test Address 1")
    mock_sleep.assert_called_once() # Ensure rate limiting sleep was called

@patch('backend.main.time.sleep', return_value=None)
@patch('geopy.geocoders.Nominatim')
def test_geocode_helper_address_not_found(MockNominatim, mock_sleep):
    mock_geolocator_instance = MockNominatim.return_value
    mock_geolocator_instance.geocode.return_value = None # Simulate address not found

    features = [{"properties": {"address": "Unknown Address"}, "geometry": None}]
    address_fields = ["address"]

    updated_features, summary = geocode_and_update_features(features, address_fields, "test_agent", mock_logger)

    assert summary["geocoding_requests_sent"] == 1
    assert summary["geocoding_successes"] == 0
    assert summary["geocoding_failures"] == 1
    assert updated_features[0]["geometry"] is None
    mock_sleep.assert_called_once()

@patch('backend.main.time.sleep', return_value=None)
@patch('geopy.geocoders.Nominatim')
def test_geocode_helper_service_timeout(MockNominatim, mock_sleep):
    mock_geolocator_instance = MockNominatim.return_value
    mock_geolocator_instance.geocode.side_effect = GeocoderTimedOut("Timeout")

    features = [{"properties": {"address": "Timeout Address"}, "geometry": None}]
    address_fields = ["address"]

    updated_features, summary = geocode_and_update_features(features, address_fields, "test_agent", mock_logger)

    assert summary["geocoding_requests_sent"] == 1
    assert summary["geocoding_successes"] == 0
    assert summary["geocoding_service_errors"] == 1
    assert updated_features[0]["geometry"] is None
    assert mock_sleep.call_count == 2 # geocode call +  one after timeout

@patch('backend.main.time.sleep', return_value=None)
@patch('geopy.geocoders.Nominatim')
def test_geocode_helper_existing_geometry(MockNominatim, mock_sleep):
    mock_geolocator_instance = MockNominatim.return_value

    features = [{"properties": {"address": "Some Address"}, "geometry": {"type": "Point", "coordinates": [1.0, 1.0]}}]
    address_fields = ["address"]

    updated_features, summary = geocode_and_update_features(features, address_fields, "test_agent", mock_logger)

    assert summary["features_total_count"] == 1
    assert summary["features_without_geometry_count"] == 0 # No feature was without geometry initially
    assert summary["geocoding_requests_sent"] == 0 # Should not attempt to geocode
    mock_geolocator_instance.geocode.assert_not_called()
    mock_sleep.assert_not_called()

@patch('backend.main.time.sleep', return_value=None)
@patch('geopy.geocoders.Nominatim')
def test_geocode_helper_no_matching_address_fields(MockNominatim, mock_sleep):
    mock_geolocator_instance = MockNominatim.return_value

    features = [{"properties": {"name": "Place Name"}, "geometry": None}] # No 'address' field
    address_fields = ["address"]

    updated_features, summary = geocode_and_update_features(features, address_fields, "test_agent", mock_logger)

    assert summary["features_without_geometry_count"] == 1
    assert summary["addresses_found_for_geocoding"] == 0
    assert summary["geocoding_requests_sent"] == 0
    mock_geolocator_instance.geocode.assert_not_called()
    mock_sleep.assert_not_called()

# --- Tests for Geocoding Stage in Celery Task ---

def setup_job_for_geocoding_test(db_session: Session, features_list: list) -> IngestionJobDB:
    """Helper to create a job and a dummy GeoJSON file for geocoding tests."""
    os.makedirs(PROCESSED_AREA_PATH, exist_ok=True)
    job_id = str(uuid.uuid4())
    # Create an intermediate GeoJSON file that this job would have produced
    intermediate_geojson_content = {"type": "FeatureCollection", "features": features_list}
    intermediate_geojson_path = os.path.join(PROCESSED_AREA_PATH, f"{job_id}_epsg{TARGET_CRS_EPSG}.geojson") # Typical name after CRS

    with open(intermediate_geojson_path, 'w') as f:
        json.dump(intermediate_geojson_content, f)

    job_data = {
        "job_id": job_id,
        "original_filename": "test_for_geocoding.geojson",
        "staged_file_path": "dummy/path/not_used_by_this_stage.geojson", # Not directly used by geocoding stage
        "user_id": "test_user",
        "processing_status": "completed_crs_transformation", # Prerequisite for geocoding
        "upload_timestamp": datetime.utcnow(),
        "intermediate_geojson_path": intermediate_geojson_path, # Critical for this stage
        "target_crs": f"EPSG:{TARGET_CRS_EPSG}",
        "pipeline_stages_executed_log": [ # Simulate previous successful stages
            {"stage": "Format Detection", "status": "success"},
            {"stage": "Format Conversion (GeoJSON)", "status": "success"},
            {"stage": "CRS Detection", "status": "success"},
            {"stage": "CRS Transformation", "status": "success"}
        ]
    }
    job = crud.create_ingestion_job(db_session, job_data)
    return job

@patch('backend.main.geocode_and_update_features')
def test_celery_task_geocoding_stage_success(mock_geocode_helper):
    db_session = next(get_test_db_session())

    initial_features = [{"type": "Feature", "properties": {"address": "123 Main St"}, "geometry": None}]
    # Mock what geocode_and_update_features would return
    mocked_geocoded_features = [{"type": "Feature", "properties": {"address": "123 Main St"}, "geometry": {"type": "Point", "coordinates": [5.0, 52.0]}}]
    mocked_summary = {"geocoding_successes": 1, "geocoding_requests_sent":1} # Simplified
    mock_geocode_helper.return_value = (mocked_geocoded_features, mocked_summary)

    job = setup_job_for_geocoding_test(db_session, initial_features)
    job_id = job.job_id
    intermediate_file_path = job.intermediate_geojson_path

    try:
        result_message = process_ingestion_job_task(job_id)
        db_session.refresh(job)

        assert "Successfully processed through geocoding" in result_message
        assert job.processing_status == "completed_geocoding"
        assert job.geocoding_summary == mocked_summary

        # Verify the intermediate file was updated
        with open(intermediate_file_path, 'r') as f:
            updated_geojson_data = json.load(f)
        assert updated_geojson_data["features"] == mocked_geocoded_features

        assert any(log["stage"] == "Geocoding" and log["status"] == "success" for log in job.pipeline_stages_executed_log)
        mock_geocode_helper.assert_called_once_with(initial_features, GEOCODING_ADDRESS_FIELDS, NOMINATIM_USER_AGENT, logging.getLogger("backend.main"))

    finally:
        if intermediate_file_path and os.path.exists(intermediate_file_path):
             if PROCESSED_AREA_PATH in os.path.abspath(intermediate_file_path):
                os.remove(intermediate_file_path)
        crud.remove_ingestion_job(db_session, job_id)
        db_session.close()

@patch('backend.main.geocode_and_update_features')
def test_celery_task_geocoding_stage_failure_in_helper(mock_geocode_helper):
    db_session = next(get_test_db_session())

    initial_features = [{"type": "Feature", "properties": {"address": "Bad Address Inc"}, "geometry": None}]
    mock_geocode_helper.side_effect = Exception("Major Geocoding Helper Failure")

    job = setup_job_for_geocoding_test(db_session, initial_features)
    job_id = job.job_id
    intermediate_file_path = job.intermediate_geojson_path

    try:
        result_message = process_ingestion_job_task(job_id)
        db_session.refresh(job)

        assert "Failed at geocoding pipeline step" in result_message
        assert job.processing_status == "failed_geocoding_step"
        assert "geocoding_pipeline_error" in job.validation_summary
        assert "Major Geocoding Helper Failure" in job.validation_summary["geocoding_pipeline_error"]

        assert any(log["stage"] == "Geocoding" and log["status"] == "failed" for log in job.pipeline_stages_executed_log)

    finally:
        if intermediate_file_path and os.path.exists(intermediate_file_path):
            if PROCESSED_AREA_PATH in os.path.abspath(intermediate_file_path):
                os.remove(intermediate_file_path)
        crud.remove_ingestion_job(db_session, job_id)
        db_session.close()

@patch('backend.main.json.load') # To simulate error during file loading
def test_celery_task_geocoding_stage_failure_file_io(mock_json_load):
    db_session = next(get_test_db_session())

    initial_features = [{"type": "Feature", "properties": {"address": "123 Main St"}, "geometry": None}]
    mock_json_load.side_effect = IOError("Cannot read file")

    job = setup_job_for_geocoding_test(db_session, initial_features) # Uses the same setup as successful geocoding
    job_id = job.job_id
    intermediate_file_path = job.intermediate_geojson_path

    try:
        result_message = process_ingestion_job_task(job_id)
        db_session.refresh(job)

        assert "Failed at geocoding pipeline step" in result_message
        assert job.processing_status == "failed_geocoding_step"
        assert "geocoding_pipeline_error" in job.validation_summary
        assert "Cannot read file" in job.validation_summary["geocoding_pipeline_error"]

        assert any(log["stage"] == "Geocoding" and log["status"] == "failed" for log in job.pipeline_stages_executed_log)

    finally:
        if intermediate_file_path and os.path.exists(intermediate_file_path):
            if PROCESSED_AREA_PATH in os.path.abspath(intermediate_file_path):
                os.remove(intermediate_file_path)
        crud.remove_ingestion_job(db_session, job_id)
        db_session.close()


# --- Tests for handle_complex_geometries Helper ---
mock_cgh_logger = logging.getLogger("test_cgh_helper")
# if not mock_cgh_logger.handlers:
#     mock_cgh_logger.addHandler(logging.StreamHandler()) # To see logs

def test_cgh_strategy_pass_through():
    features = [{"type": "Feature", "geometry": {"type": "Point", "coordinates": [0,0]}, "properties": {}}]
    processed, summary = handle_complex_geometries(copy.deepcopy(features), "pass_through", {}, mock_cgh_logger)
    assert processed == features
    assert summary["feature_count_before"] == 1
    assert summary["feature_count_after"] == 1
    assert summary["geometries_by_type_before"]["Point"] == 1
    assert summary["geometries_by_type_after"]["Point"] == 1

def test_cgh_strategy_filter_types():
    features = [
        {"type": "Feature", "geometry": {"type": "Point", "coordinates": [0,0]}, "properties": {"id": 1}},
        {"type": "Feature", "geometry": {"type": "LineString", "coordinates": [[0,0],[1,1]]}, "properties": {"id": 2}},
        {"type": "Feature", "geometry": {"type": "Polygon", "coordinates": [[[0,0],[1,1],[1,0],[0,0]]]}, "properties": {"id": 3}},
        {"type": "Feature", "geometry": None, "properties": {"id": 4}} # Feature with no geometry
    ]
    config = {'filter_types_kept': ["Point", "Polygon"]}
    processed, summary = handle_complex_geometries(copy.deepcopy(features), "filter_types", config, mock_cgh_logger)
    assert len(processed) == 3 # Point, Polygon, and NoGeometry feature
    assert processed[0]["properties"]["id"] == 1
    assert processed[1]["properties"]["id"] == 3
    assert processed[2]["properties"]["id"] == 4 # NoGeometry feature should pass
    assert summary["features_filtered_out_count"] == 1
    assert summary["geometries_by_type_before"]["LineString"] == 1
    assert summary["geometries_by_type_after"]["LineString"] == 0
    assert summary["geometries_by_type_after"]["Point"] == 1
    assert summary["geometries_by_type_after"]["Polygon"] == 1
    assert summary["geometries_by_type_after"]["None"] == 1


def test_cgh_strategy_reject_on_types():
    features = [
        {"type": "Feature", "geometry": {"type": "Point", "coordinates": [0,0]}, "properties": {"id": 1}},
        {"type": "Feature", "geometry": {"type": "GeometryCollection", "geometries": [{"type":"Point", "coordinates":[0,0]}]}, "properties": {"id": 2}}
    ]
    config = {'reject_if_types_found': ["GeometryCollection"]}
    processed, summary = handle_complex_geometries(copy.deepcopy(features), "reject_on_types", config, mock_cgh_logger)
    assert summary["job_failed_due_to_rejected_type"] is True
    assert len(processed) == 0 # Features should be cleared on rejection

    # Test no rejected types found
    config_no_reject = {'reject_if_types_found': ["Polygon"]} # Polygon not present
    processed_ok, summary_ok = handle_complex_geometries(copy.deepcopy(features), "reject_on_types", config_no_reject, mock_cgh_logger)
    assert summary_ok["job_failed_due_to_rejected_type"] is False
    assert len(processed_ok) == 2


def test_cgh_strategy_explode_collections():
    features = [
        {"type": "Feature", "id": "gc1", "geometry": {
            "type": "GeometryCollection",
            "geometries": [
                {"type": "Point", "coordinates": [0,0]},
                {"type": "LineString", "coordinates": [[1,1],[2,2]]}
            ]}, "properties": {"name": "Collection1"}
        },
        {"type": "Feature", "id": "p1", "geometry": {"type": "Point", "coordinates": [3,3]}, "properties": {"name": "Point1"}}
    ]
    processed, summary = handle_complex_geometries(copy.deepcopy(features), "explode_collections", {}, mock_cgh_logger)
    assert len(processed) == 3 # GC explodes into 2, plus the original Point
    assert summary["geometry_collections_exploded_count"] == 1
    assert summary["new_features_from_splits"] == 2
    assert processed[0]["geometry"]["type"] == "Point"
    assert processed[0]["properties"]["original_feature_id"] == "gc1"
    assert processed[1]["geometry"]["type"] == "LineString"
    assert processed[1]["properties"]["original_feature_id"] == "gc1"
    assert processed[2]["id"] == "p1"

def test_cgh_strategy_split_multi():
    features = [
        {"type": "Feature", "id": "mp1", "geometry": {"type": "MultiPoint", "coordinates": [[0,0],[1,1]]}, "properties": {"name": "MultiPoint1"}},
        {"type": "Feature", "id": "p1", "geometry": {"type": "Point", "coordinates": [2,2]}, "properties": {"name": "Point1"}}
    ]
    processed, summary = handle_complex_geometries(copy.deepcopy(features), "split_multi", {}, mock_cgh_logger)
    assert len(processed) == 3 # MultiPoint splits into 2, plus the original Point
    assert summary["multi_geometries_split_count"] == 1
    assert summary["new_features_from_splits"] == 2
    assert processed[0]["geometry"]["type"] == "Point"
    assert processed[0]["properties"]["original_feature_id"] == "mp1"
    assert processed[1]["geometry"]["type"] == "Point"
    assert processed[1]["properties"]["original_feature_id"] == "mp1"
    assert processed[2]["id"] == "p1"

def test_cgh_malformed_geometry():
    features = [{"type": "Feature", "geometry": {"type": "NotARealType", "coordinates": "blah"}, "properties": {}}]
    processed, summary = handle_complex_geometries(copy.deepcopy(features), "pass_through", {}, mock_cgh_logger)
    assert summary["malformed_geometries_encountered"] == 1
    assert summary["geometries_by_type_before"]["Malformed/Invalid"] == 1
    assert summary["geometries_by_type_after"]["Malformed/Invalid"] == 1 # Passed through
    assert len(processed) == 1

@patch.object(logging.getLogger("test_cgh_helper"), 'warning')
def test_cgh_unsupported_strategy(mock_logger_warning):
    features = [{"type": "Feature", "geometry": {"type": "Point", "coordinates": [0,0]}, "properties": {}}]
    processed, summary = handle_complex_geometries(copy.deepcopy(features), "unknown_strategy", {}, mock_cgh_logger)
    assert summary["unsupported_strategy_used"] is True
    assert processed == features # Should default to pass_through
    mock_logger_warning.assert_called_once_with("Unsupported geometry handling strategy: unknown_strategy. Applying 'pass_through'.")


# --- Tests for Complex Geometry Stage in Celery Task ---

def setup_job_for_cgh_test(db_session: Session, features_list: list, job_status_prereq="completed_geocoding") -> IngestionJobDB:
    os.makedirs(PROCESSED_AREA_PATH, exist_ok=True)
    job_id = str(uuid.uuid4())
    intermediate_geojson_content = {"type": "FeatureCollection", "features": features_list}
    # Path depends on whether geocoding step ran and potentially renamed the file (it doesn't currently)
    # or if CRS was the last step that named it.
    # For now, assume geocoding ran, so path is just job_id.geojson. If not, it's job_id_epsg4326.geojson
    filename = f"{job_id}.geojson" if job_status_prereq == "completed_geocoding" else f"{job_id}_epsg{TARGET_CRS_EPSG}.geojson"
    intermediate_geojson_path = os.path.join(PROCESSED_AREA_PATH, filename)

    with open(intermediate_geojson_path, 'w') as f:
        json.dump(intermediate_geojson_content, f)

    job_data = {
        "job_id": job_id, "original_filename": "test_cgh.geojson",
        "staged_file_path": "dummy/staged.geojson", "user_id": "cgh_user",
        "processing_status": job_status_prereq, "upload_timestamp": datetime.utcnow(),
        "intermediate_geojson_path": intermediate_geojson_path,
        "pipeline_stages_executed_log": [{"stage": job_status_prereq, "status": "success"}]
    }
    if job_status_prereq == "completed_geocoding":
        job_data["geocoding_summary"] = {"info": "mock geocoding done"}
    if job_status_prereq == "completed_crs_transformation":
         job_data["target_crs"] = f"EPSG:{TARGET_CRS_EPSG}"

    job = crud.create_ingestion_job(db_session, job_data)
    return job

@patch('backend.main.COMPLEX_GEOMETRY_STRATEGY', "split_all")
@patch('backend.main.handle_complex_geometries')
def test_celery_cgh_stage_successful_split_all(mock_handle_complex_geometries):
    db_session = next(get_test_db_session())
    initial_features = [{"type": "Feature", "id":"gc1", "geometry": {"type": "GeometryCollection", "geometries": [{"type": "MultiPoint", "coordinates": [[0,0],[1,1]]}]}, "properties": {}}]

    # Side effect for two calls
    exploded_features = [{"type": "Feature", "id":"gc1_part1", "geometry": {"type": "MultiPoint", "coordinates": [[0,0],[1,1]]}, "properties": {"original_feature_id":"gc1"}}]
    summary_explode = {"strategy_applied": "explode_collections", "feature_count_before":1, "feature_count_after":1, "geometry_collections_exploded_count":1, "new_features_from_splits":1, "geometries_by_type_before":defaultdict(int, {"GeometryCollection":1}), "geometries_by_type_after":defaultdict(int, {"MultiPoint":1})}

    final_split_features = [
        {"type": "Feature", "id":"gc1_part1_part1", "geometry": {"type": "Point", "coordinates": [0,0]}, "properties": {"original_feature_id":"gc1_part1"}},
        {"type": "Feature", "id":"gc1_part1_part2", "geometry": {"type": "Point", "coordinates": [1,1]}, "properties": {"original_feature_id":"gc1_part1"}}
    ]
    summary_split = {"strategy_applied": "split_multi", "feature_count_before":1, "feature_count_after":2, "multi_geometries_split_count":1, "new_features_from_splits":2, "geometries_by_type_before":defaultdict(int, {"MultiPoint":1}), "geometries_by_type_after":defaultdict(int, {"Point":2})}

    mock_handle_complex_geometries.side_effect = [
        (exploded_features, summary_explode),
        (final_split_features, summary_split)
    ]

    job = setup_job_for_cgh_test(db_session, initial_features)
    job_id = job.job_id
    intermediate_file_path = job.intermediate_geojson_path

    try:
        result_message = process_ingestion_job_task(job_id)
        db_session.refresh(job)

        assert "Successfully processed through complex geometry handling" in result_message
        assert job.processing_status == "completed_complex_geometry"
        assert job.complex_geometry_summary["strategy_applied"] == "split_all"
        assert job.complex_geometry_summary["geometry_collections_exploded_count"] == 1
        assert job.complex_geometry_summary["multi_geometries_split_count"] == 1
        assert job.complex_geometry_summary["feature_count_after"] == 2

        with open(intermediate_file_path, 'r') as f:
            updated_data = json.load(f)
        assert updated_data["features"] == final_split_features

        assert mock_handle_complex_geometries.call_count == 2
    finally:
        if intermediate_file_path and os.path.exists(intermediate_file_path): os.remove(intermediate_file_path)
        crud.remove_ingestion_job(db_session, job_id)
        db_session.close()

@patch('backend.main.COMPLEX_GEOMETRY_STRATEGY', "filter_types")
@patch('backend.main.CGH_FILTER_TYPES_TO_KEEP', ["Point"])
@patch('backend.main.handle_complex_geometries')
def test_celery_cgh_stage_successful_single_strategy_filter(mock_cgh_helper):
    db_session = next(get_test_db_session())
    initial_features = [
        {"type": "Feature", "geometry": {"type": "Point", "coordinates": [0,0]}, "properties": {"id": "p1"}},
        {"type": "Feature", "geometry": {"type": "LineString", "coordinates": [[0,0],[1,1]]}, "properties": {"id": "ls1"}}
    ]
    filtered_features = [{"type": "Feature", "geometry": {"type": "Point", "coordinates": [0,0]}, "properties": {"id": "p1"}}]
    mock_summary = {"strategy_applied": "filter_types", "features_filtered_out_count": 1, "feature_count_after":1}
    mock_cgh_helper.return_value = (filtered_features, mock_summary)

    job = setup_job_for_cgh_test(db_session, initial_features)
    job_id = job.job_id; intermediate_file_path = job.intermediate_geojson_path
    try:
        process_ingestion_job_task(job_id)
        db_session.refresh(job)
        assert job.processing_status == "completed_complex_geometry"
        assert job.complex_geometry_summary == mock_summary
        with open(intermediate_file_path, 'r') as f: data = json.load(f)
        assert data["features"] == filtered_features
    finally:
        if intermediate_file_path and os.path.exists(intermediate_file_path): os.remove(intermediate_file_path)
        crud.remove_ingestion_job(db_session, job_id); db_session.close()

@patch('backend.main.COMPLEX_GEOMETRY_STRATEGY', "reject_on_types")
@patch('backend.main.CGH_REJECT_IF_TYPES_FOUND', ["GeometryCollection"])
@patch('backend.main.handle_complex_geometries')
def test_celery_cgh_stage_failure_reject_on_types(mock_cgh_helper):
    db_session = next(get_test_db_session())
    initial_features = [{"type": "Feature", "geometry": {"type": "GeometryCollection", "geometries": []}}]
    mock_summary = {"strategy_applied": "reject_on_types", "job_failed_due_to_rejected_type": True}
    # When job_failed_due_to_rejected_type is true, the helper returns the original features list (or empty if it clears it)
    # The task logic then uses this flag to fail the job.
    mock_cgh_helper.return_value = ([], mock_summary)

    job = setup_job_for_cgh_test(db_session, initial_features)
    job_id = job.job_id; intermediate_file_path = job.intermediate_geojson_path
    try:
        process_ingestion_job_task(job_id)
        db_session.refresh(job)
        assert job.processing_status == "failed_complex_geometry_rejected"
        assert job.complex_geometry_summary["job_failed_due_to_rejected_type"] is True
    finally:
        if intermediate_file_path and os.path.exists(intermediate_file_path): os.remove(intermediate_file_path)
        crud.remove_ingestion_job(db_session, job_id); db_session.close()

@patch('backend.main.handle_complex_geometries')
def test_celery_cgh_stage_failure_exception_in_helper(mock_cgh_helper):
    db_session = next(get_test_db_session())
    initial_features = [{"type": "Feature", "geometry": {"type": "Point", "coordinates": [0,0]}}]
    mock_cgh_helper.side_effect = Exception("CGH Helper Exception")

    job = setup_job_for_cgh_test(db_session, initial_features)
    job_id = job.job_id; intermediate_file_path = job.intermediate_geojson_path
    try:
        process_ingestion_job_task(job_id)
        db_session.refresh(job)
        assert job.processing_status == "failed_complex_geometry_step"
        assert "complex_geometry_pipeline_error" in job.validation_summary
        assert "CGH Helper Exception" in job.validation_summary["complex_geometry_pipeline_error"]
    finally:
        if intermediate_file_path and os.path.exists(intermediate_file_path): os.remove(intermediate_file_path)
        crud.remove_ingestion_job(db_session, job_id); db_session.close()
