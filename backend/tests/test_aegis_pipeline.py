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
from backend.main import app, get_current_user, User, Role, STAGING_AREA_PATH, ALLOWED_MIME_TYPES, MAX_FILE_SIZE, PROCESSED_AREA_PATH
from backend.main import process_ingestion_job_task # Import the Celery task
from backend.database import SessionLocal, get_db, Base, engine
from backend.models import IngestionJobDB
from backend import crud

# Test tokens corresponding to users in backend.main.dummy_users_db
# These should match the tokens in backend/main.py
ADMIN_TOKEN = "secure-placeholder-token-admin" # Has 'upload_data' permission
ANALYST_TOKEN = "secure-placeholder-token-analyst" # Has 'upload_data' permission
VIEWER_TOKEN = "secure-placeholder-token-viewer" # Does NOT have 'upload_data'
INVALID_TOKEN = "invalid-dummy-token"

client = TestClient(app)

# Create tables for an in-memory SQLite or ensure test DB is clean.
# For simplicity, let's use the existing DB but be mindful of test data.
# If using a real DB, ensure cleanup. For now, we'll assume existing DB.
# A better approach would be to override get_db for tests to use a test DB.
# Base.metadata.create_all(bind=engine) # Ensure tables exist if using a fresh test DB

def get_test_db_session():
    db = SessionLocal()
    try:
        yield db
    finally:
        db.close()

def test_upload_file_success():
    # Ensure STAGING_AREA_PATH exists for the test
    # main.py creates this, but good practice for tests to be self-sufficient if needed
    os.makedirs(STAGING_AREA_PATH, exist_ok=True)

    test_filename = f"test_upload_{uuid.uuid4()}.txt"
    test_content = b"This is a test file."

    # Create a dummy file to upload locally for the test client
    with open(test_filename, "wb") as f:
        f.write(test_content)

    staged_file_path_to_delete = None
    job_id_to_delete = None

    try:
        with open(test_filename, "rb") as f_upload:
            response = client.post(
                "/aegis/upload",
                headers={"X-Auth-Token": ADMIN_TOKEN},
                files={"file": (test_filename, f_upload, "text/plain")}
            )

        assert response.status_code == 200
        data = response.json()
        assert data["filename"] == test_filename
        assert "job_id" in data
        assert "staged_filepath" in data
        staged_file_path_to_delete = data["staged_filepath"]
        job_id_to_delete = data["job_id"]

        # Verify file exists in staging area relative to where main.py would create it
        assert os.path.exists(staged_file_path_to_delete)
        with open(staged_file_path_to_delete, "rb") as f_staged:
            assert f_staged.read() == test_content

        # Verify database record
        # Using a new session for verification
        db_gen = get_test_db_session()
        db = next(db_gen)
        job_db = crud.get_ingestion_job(db, job_id_to_delete)
        assert job_db is not None
        assert job_db.original_filename == test_filename
        assert job_db.staged_file_path == staged_file_path_to_delete
        assert job_db.user_id == "admin" # From ADMIN_TOKEN mapping in main.py
        assert job_db.processing_status == "pending_staging"
        assert job_db.celery_task_id is not None # Check if celery task ID was set

    finally:
        # Clean up created file and staged file
        if os.path.exists(test_filename):
            os.remove(test_filename)
        if staged_file_path_to_delete and os.path.exists(staged_file_path_to_delete):
            # Check if staged_file_path_to_delete is within STAGING_AREA_PATH to be safe
            if STAGING_AREA_PATH in os.path.abspath(staged_file_path_to_delete):
                 os.remove(staged_file_path_to_delete)
            else:
                print(f"Warning: Attempted to delete staged file outside staging area: {staged_file_path_to_delete}")

        # Clean up DB record
        if job_id_to_delete:
            db_gen = get_test_db_session()
            db = next(db_gen) # Get a new session for cleanup
            crud.remove_ingestion_job(db, job_id_to_delete)
            db.close() # Close session used for cleanup

def test_upload_file_no_token():
    response = client.post("/aegis/upload", files={"file": ("test.txt", b"content", "text/plain")})
    assert response.status_code == 401 # Expect 401 if token is missing

def test_upload_file_invalid_token():
    response = client.post(
        "/aegis/upload",
        headers={"X-Auth-Token": INVALID_TOKEN},
        files={"file": ("test.txt", b"content", "text/plain")}
    )
    assert response.status_code == 401

def test_upload_file_permission_denied():
    response = client.post(
        "/aegis/upload",
        headers={"X-Auth-Token": VIEWER_TOKEN}, # Viewer role lacks 'upload_data'
        files={"file": ("test.txt", b"content", "text/plain")}
    )
    assert response.status_code == 403

def test_upload_file_too_large():
    mime_type_for_large_file = "application/octet-stream"
    if ALLOWED_MIME_TYPES and mime_type_for_large_file not in ALLOWED_MIME_TYPES:
        mime_type_for_large_file = ALLOWED_MIME_TYPES[0]

    if MAX_FILE_SIZE < 10000:
        large_content = b"a" * (MAX_FILE_SIZE + 1)
        response = client.post(
            "/aegis/upload",
            headers={"X-Auth-Token": ADMIN_TOKEN},
            files={"file": ("large.txt", large_content, mime_type_for_large_file)}
        )
        assert response.status_code == 413
    else:
        print(f"Skipping file_too_large test as MAX_FILE_SIZE ({MAX_FILE_SIZE}) is large.")
        pytest.skip(f"Skipping file_too_large test as MAX_FILE_SIZE ({MAX_FILE_SIZE}) is too large for practical testing here.")


def test_upload_file_unsupported_mime_type():
    unsupported_mime_type = "application/x-definitely-not-allowed"
    if unsupported_mime_type in ALLOWED_MIME_TYPES:
        unsupported_mime_type = f"application/x-test-disallowed-{uuid.uuid4()}"

    response = client.post(
        "/aegis/upload",
        headers={"X-Auth-Token": ADMIN_TOKEN},
        files={"file": ("image.jpg", b"fakejpegcontent", unsupported_mime_type)}
    )
    assert response.status_code == 415

# --- Celery Task Tests ---

def create_dummy_job_for_task(db_session: Session, staged_file_path: str, job_id_val: Optional[str] = None) -> IngestionJobDB:
    if job_id_val is None:
        job_id_val = str(uuid.uuid4())

    job_data = {
        "job_id": job_id_val,
        "original_filename": "test_file.zip", # Example
        "staged_file_path": staged_file_path,
        "user_id": "test_user",
        "processing_status": "pending_task_execution", # Initial status before task runs
        "upload_timestamp": datetime.utcnow()
    }
    return crud.create_ingestion_job(db_session, job_data)

@patch('backend.main.ogr') # Mock ogr in the module where the task uses it
@patch('backend.main.gdal') # Mock gdal in the module where the task uses it
def test_celery_task_success(mock_gdal, mock_ogr):
    db_session = next(get_test_db_session())

    mock_datasource = MagicMock()
    mock_driver = MagicMock()
    mock_driver.GetName.return_value = "ESRI Shapefile"
    mock_datasource.GetDriver.return_value = mock_driver
    mock_ogr.Open.return_value = mock_datasource

    mock_gdal.VectorTranslate.return_value = True

    os.makedirs(PROCESSED_AREA_PATH, exist_ok=True)
    os.makedirs(STAGING_AREA_PATH, exist_ok=True) # Ensure staging also exists

    dummy_staged_filename = f"dummy_staged_{uuid.uuid4()}.shp"
    dummy_staged_filepath = os.path.join(STAGING_AREA_PATH, dummy_staged_filename)
    with open(dummy_staged_filepath, "wb") as f:
        f.write(b"dummy shapefile content")

    job = create_dummy_job_for_task(db_session, dummy_staged_filepath)
    job_id = job.job_id

    try:
        result_message = process_ingestion_job_task(job_id)
        db_session.refresh(job)

        assert "File successfully converted to GeoJSON" in result_message
        assert job.processing_status == "completed_format_conversion"
        assert job.detected_format == "ESRI Shapefile"
        expected_geojson_path = os.path.join(PROCESSED_AREA_PATH, f"{job_id}.geojson")
        assert job.intermediate_geojson_path == expected_geojson_path

        assert len(job.pipeline_stages_executed_log) >= 2
        assert job.pipeline_stages_executed_log[-1]["stage"] == "Format Conversion (GeoJSON)"
        assert job.pipeline_stages_executed_log[-1]["status"] == "success"
        assert job.pipeline_stages_executed_log[-1]["output_path"] == expected_geojson_path
        assert job.pipeline_stages_executed_log[-2]["stage"] == "Format Detection"
        assert job.pipeline_stages_executed_log[-2]["status"] == "success"

    finally:
        if os.path.exists(dummy_staged_filepath):
            os.remove(dummy_staged_filepath)
        # No real GeoJSON file created by mock, so no need to delete job.intermediate_geojson_path
        crud.remove_ingestion_job(db_session, job_id)
        db_session.close()

@patch('backend.main.ogr')
def test_celery_task_format_detection_failure(mock_ogr):
    db_session = next(get_test_db_session())

    mock_ogr.Open.return_value = None

    os.makedirs(STAGING_AREA_PATH, exist_ok=True)
    dummy_staged_filename = f"dummy_unrecog_{uuid.uuid4()}.dat"
    dummy_staged_filepath = os.path.join(STAGING_AREA_PATH, dummy_staged_filename)
    with open(dummy_staged_filepath, "wb") as f:
        f.write(b"unknown content")

    job = create_dummy_job_for_task(db_session, dummy_staged_filepath)
    job_id = job.job_id

    try:
        result_message = process_ingestion_job_task(job_id)
        db_session.refresh(job)

        assert "Format detection failed" in result_message
        assert job.processing_status == "failed_format_detection"
        assert job.detected_format is None
        assert job.intermediate_geojson_path is None
        assert "format_detection_error" in job.validation_summary
        assert job.pipeline_stages_executed_log[-1]["stage"] == "Format Detection"
        assert job.pipeline_stages_executed_log[-1]["status"] == "failed"

    finally:
        if os.path.exists(dummy_staged_filepath):
            os.remove(dummy_staged_filepath)
        crud.remove_ingestion_job(db_session, job_id)
        db_session.close()

@patch('backend.main.ogr')
@patch('backend.main.gdal')
def test_celery_task_format_conversion_failure(mock_gdal, mock_ogr):
    db_session = next(get_test_db_session())

    mock_datasource = MagicMock()
    mock_driver = MagicMock()
    mock_driver.GetName.return_value = "ESRI Shapefile"
    mock_datasource.GetDriver.return_value = mock_driver
    mock_ogr.Open.return_value = mock_datasource

    mock_gdal.VectorTranslate.side_effect = Exception("GDAL conversion error")

    os.makedirs(STAGING_AREA_PATH, exist_ok=True)
    os.makedirs(PROCESSED_AREA_PATH, exist_ok=True)

    dummy_staged_filename = f"dummy_staged_badconv_{uuid.uuid4()}.shp"
    dummy_staged_filepath = os.path.join(STAGING_AREA_PATH, dummy_staged_filename)
    with open(dummy_staged_filepath, "wb") as f:
        f.write(b"dummy shapefile content for bad conversion")

    job = create_dummy_job_for_task(db_session, dummy_staged_filepath)
    job_id = job.job_id

    try:
        result_message = process_ingestion_job_task(job_id)
        db_session.refresh(job)

        assert "Failed at GeoJSON conversion stage" in result_message
        assert job.processing_status == "failed_format_conversion"
        assert job.detected_format == "ESRI Shapefile"
        assert job.intermediate_geojson_path is None
        assert "format_conversion_error" in job.validation_summary
        assert job.pipeline_stages_executed_log[-1]["stage"] == "Format Conversion (GeoJSON)"
        assert job.pipeline_stages_executed_log[-1]["status"] == "failed"

    finally:
        if os.path.exists(dummy_staged_filepath):
            os.remove(dummy_staged_filepath)
        crud.remove_ingestion_job(db_session, job_id)
        db_session.close()
