import os
import shutil
import uuid
from fastapi.testclient import TestClient
from sqlalchemy.orm import Session
import pytest # For potential fixtures or advanced testing patterns later

# Adjust paths for imports based on execution from repository root or if backend/ is a module
# Assuming tests might be run from the repo root, and PYTHONPATH includes the root.
from backend.main import app, get_current_user, User, Role, STAGING_AREA_PATH, ALLOWED_MIME_TYPES, MAX_FILE_SIZE
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
    # Create a dummy file larger than MAX_FILE_SIZE
    # This assumes MAX_FILE_SIZE is small enough for a memory/local disk test file
    # If MAX_FILE_SIZE is very large, this test might need adjustment or mocking `MAX_FILE_SIZE`

    # Check if 'application/octet-stream' is an allowed type, if not, use one that is.
    # For this test, the content itself matters more than strict type adherence if type is not main variable.
    mime_type_for_large_file = "application/octet-stream"
    if ALLOWED_MIME_TYPES and mime_type_for_large_file not in ALLOWED_MIME_TYPES:
        mime_type_for_large_file = ALLOWED_MIME_TYPES[0] # Use the first allowed type

    # MAX_FILE_SIZE could be large, so creating a file that size could be slow/problematic.
    # For unit tests, it's common to test the boundary condition logic,
    # possibly by temporarily patching MAX_FILE_SIZE or ensuring it's set to a small value for tests.
    # The prompt's version checks MAX_FILE_SIZE < 10000. Let's honor that.
    if MAX_FILE_SIZE < 10000:
        large_content = b"a" * (MAX_FILE_SIZE + 1)
        response = client.post(
            "/aegis/upload",
            headers={"X-Auth-Token": ADMIN_TOKEN},
            files={"file": ("large.txt", large_content, mime_type_for_large_file)}
        )
        assert response.status_code == 413
    else:
        # Consider using pytest.skip or adjusting how large files are tested
        # For now, just printing a message as per original prompt's logic
        print(f"Skipping file_too_large test as MAX_FILE_SIZE ({MAX_FILE_SIZE}) is large.")
        pytest.skip(f"Skipping file_too_large test as MAX_FILE_SIZE ({MAX_FILE_SIZE}) is too large for practical testing here.")


def test_upload_file_unsupported_mime_type():
    # Ensure we pick a MIME type that is definitely not in ALLOWED_MIME_TYPES
    unsupported_mime_type = "application/x-definitely-not-allowed"
    if unsupported_mime_type in ALLOWED_MIME_TYPES:
        # If by some chance it is allowed, find one that isn't or make one up
        unsupported_mime_type = f"application/x-test-disallowed-{uuid.uuid4()}"

    response = client.post(
        "/aegis/upload",
        headers={"X-Auth-Token": ADMIN_TOKEN},
        files={"file": ("image.jpg", b"fakejpegcontent", unsupported_mime_type)}
    )
    assert response.status_code == 415
