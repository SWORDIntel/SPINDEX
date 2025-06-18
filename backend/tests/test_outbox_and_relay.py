# backend/tests/test_outbox_and_relay.py
import os
import shutil
import uuid
from fastapi.testclient import TestClient
from sqlalchemy.orm import Session
import pytest
from unittest.mock import patch, MagicMock, AsyncMock
from datetime import datetime
import json # For NATS payload assertion

from backend.main import app, STAGING_AREA_PATH, NATS_JOB_SUBJECT # Import app and config
from backend.database import SessionLocal, get_db, Base, engine # For DB interaction
from backend.models import IngestionJobDB, JobOutbox # Import models
from backend import crud

# Imports for relay testing
import asyncio
from backend.message_relay import relay_cycle, MAX_OUTBOX_FETCH_LIMIT # Import the function to test and config
from nats.errors import TimeoutError as NatsTimeoutError # For simulating NATS errors


# Test tokens
ADMIN_TOKEN = "secure-placeholder-token-admin" # Has 'upload_data' permission

client = TestClient(app)

# Setup a temporary, isolated database for tests if possible, or use test session management.
# For now, follow existing pattern of creating/cleaning up specific records.

def get_test_db_session_for_outbox_tests(): # Copied from test_aegis_pipeline
    db = SessionLocal()
    try:
        yield db
    finally:
        db.close()

@patch('backend.main.process_ingestion_job_task.delay') # Mock the old Celery task
def test_upload_creates_job_and_outbox_event(mock_celery_delay):
    os.makedirs(STAGING_AREA_PATH, exist_ok=True)
    test_filename = f"test_outbox_upload_{uuid.uuid4()}.txt"
    test_content = b"Test content for outbox."

    # Create a temporary file to upload
    with open(test_filename, "wb") as f:
        f.write(test_content)

    staged_file_path_to_delete = None
    job_id_to_delete = None
    outbox_event_id_to_delete = None # To store outbox event id for cleanup

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
        job_id_to_delete = data["job_id"]
        staged_file_path_to_delete = data["staged_filepath"]

        assert os.path.exists(staged_file_path_to_delete)

        # Verify database records
        db = next(get_test_db_session_for_outbox_tests())
        try:
            # Verify IngestionJobDB record
            job_db = crud.get_ingestion_job(db, job_id_to_delete)
            assert job_db is not None
            assert job_db.original_filename == test_filename
            assert job_db.user_id == "admin" # From ADMIN_TOKEN
            assert job_db.processing_status == "PENDING_PUBLISH" # New status
            assert job_db.celery_task_id is None # Celery task ID should NOT be set

            # Verify JobOutbox record
            # Assuming job_id is unique enough to find the outbox event for this test
            outbox_event = db.query(JobOutbox).filter(JobOutbox.job_id == job_id_to_delete).first()
            assert outbox_event is not None
            outbox_event_id_to_delete = outbox_event.event_id # Store for cleanup
            assert outbox_event.nats_subject == NATS_JOB_SUBJECT # Check configured subject
            assert outbox_event.published_status == "UNPUBLISHED"
            assert outbox_event.nats_message_id == job_id_to_delete

            payload = outbox_event.nats_message_payload
            assert payload["job_id"] == job_id_to_delete
            assert payload["staged_file_path"] == staged_file_path_to_delete

            # Verify Celery task was NOT called
            mock_celery_delay.assert_not_called()

        finally:
            db.close()

    finally:
        # Cleanup
        if os.path.exists(test_filename):
            os.remove(test_filename)
        if staged_file_path_to_delete and os.path.exists(staged_file_path_to_delete):
            # Ensure the path is within the staging area before deleting
            abs_staging_area = os.path.abspath(STAGING_AREA_PATH)
            abs_staged_file = os.path.abspath(staged_file_path_to_delete)
            if os.path.commonpath([abs_staging_area]) == os.path.commonpath([abs_staging_area, abs_staged_file]):
                 os.remove(staged_file_path_to_delete)
            else:
                print(f"Warning: Staged file {staged_file_path_to_delete} is outside staging area {STAGING_AREA_PATH}. Skipping delete for safety.")


        db_cleanup = next(get_test_db_session_for_outbox_tests())
        try:
            if outbox_event_id_to_delete:
                event_to_del = db_cleanup.query(JobOutbox).filter(JobOutbox.event_id == outbox_event_id_to_delete).first()
                if event_to_del:
                    db_cleanup.delete(event_to_del)
            if job_id_to_delete:
                # Assuming remove_ingestion_job handles its own commit for the job record
                # and cascades or that outbox event is already deleted.
                crud.remove_ingestion_job(db_cleanup, job_id_to_delete)
            db_cleanup.commit() # Commit deletion of outbox event if job was not deleted or cascade didn't happen
        finally:
            db_cleanup.close()


# --- Tests for Message Relay Cycle ---

@pytest.mark.asyncio
@patch('backend.message_relay.SessionLocal')
@patch('backend.message_relay.get_unpublished_outbox_events')
@patch('backend.message_relay.update_outbox_event_status')
@patch('backend.message_relay.publish_to_nats', new_callable=AsyncMock) # Use AsyncMock for async function
async def test_relay_cycle_no_events(mock_publish_nats, mock_update_status, mock_get_events, mock_session_local):
    # Arrange
    mock_get_events.return_value = [] # No events
    mock_nc = MagicMock()
    mock_nc.is_connected = True
    mock_db_session = MagicMock(spec=Session) # Use spec for Session methods
    mock_session_local.return_value = mock_db_session

    # Act
    published_count = await relay_cycle(mock_nc)

    # Assert
    assert published_count == 0
    mock_get_events.assert_called_once_with(mock_db_session, MAX_OUTBOX_FETCH_LIMIT)
    mock_publish_nats.assert_not_called()
    mock_update_status.assert_not_called()
    mock_db_session.commit.assert_not_called()
    mock_db_session.rollback.assert_not_called()
    mock_db_session.close.assert_called_once()

@pytest.mark.asyncio
@patch('backend.message_relay.SessionLocal')
@patch('backend.message_relay.get_unpublished_outbox_events')
@patch('backend.message_relay.update_outbox_event_status')
@patch('backend.message_relay.publish_to_nats', new_callable=AsyncMock)
async def test_relay_cycle_publish_success_one_event(mock_publish_nats, mock_update_status, mock_get_events, mock_session_local):
    # Arrange
    mock_event = MagicMock(spec=JobOutbox)
    mock_event.event_id = 1
    mock_event.job_id = "job_123"
    mock_event.nats_subject = "test.subject"
    mock_event.nats_message_payload = {"data": "test_payload"}
    mock_event.nats_message_id = "msg_123"

    mock_get_events.return_value = [mock_event]
    mock_publish_nats.return_value = True # Simulate successful NATS publish
    mock_update_status.return_value = mock_event # Simulate successful DB update

    mock_nc = MagicMock()
    mock_nc.is_connected = True
    mock_db_session = MagicMock(spec=Session)
    mock_session_local.return_value = mock_db_session

    # Act
    published_count = await relay_cycle(mock_nc)

    # Assert
    assert published_count == 1
    mock_get_events.assert_called_once_with(mock_db_session, MAX_OUTBOX_FETCH_LIMIT)
    mock_publish_nats.assert_called_once_with(
        mock_nc,
        "test.subject",
        json.dumps({"data": "test_payload"}).encode('utf-8'),
        "msg_123"
    )
    mock_update_status.assert_called_once()
    # Check specific args of update_outbox_event_status more carefully
    args, kwargs = mock_update_status.call_args
    assert args[0] == mock_db_session
    assert args[1] == mock_event.event_id
    assert args[2] == "PUBLISHED"
    assert 'published_at' in kwargs and isinstance(kwargs['published_at'], datetime)

    mock_db_session.commit.assert_called_once()
    mock_db_session.rollback.assert_not_called()
    mock_db_session.close.assert_called_once()

@pytest.mark.asyncio
@patch('backend.message_relay.SessionLocal')
@patch('backend.message_relay.get_unpublished_outbox_events')
@patch('backend.message_relay.update_outbox_event_status')
@patch('backend.message_relay.publish_to_nats', new_callable=AsyncMock)
async def test_relay_cycle_publish_failure_nats_timeout(mock_publish_nats, mock_update_status, mock_get_events, mock_session_local):
    # Arrange
    mock_event = MagicMock(spec=JobOutbox)
    mock_event.event_id = 2
    mock_event.job_id = "job_456"
    # ... (set other attributes for mock_event as in previous test)
    mock_event.nats_subject = "test.subject.fail"
    mock_event.nats_message_payload = {"data": "fail_payload"}
    mock_event.nats_message_id = "msg_456"

    mock_get_events.return_value = [mock_event]
    mock_publish_nats.return_value = False # Simulate NATS publish failure (e.g. due to timeout handled in publish_to_nats)

    mock_nc = MagicMock()
    mock_nc.is_connected = True
    mock_db_session = MagicMock(spec=Session)
    mock_session_local.return_value = mock_db_session

    # Act
    published_count = await relay_cycle(mock_nc)

    # Assert
    assert published_count == 0
    mock_get_events.assert_called_once_with(mock_db_session, MAX_OUTBOX_FETCH_LIMIT)
    mock_publish_nats.assert_called_once() # Attempted to publish
    mock_update_status.assert_not_called() # Should not update status if NATS publish failed
    mock_db_session.commit.assert_not_called() # No successful DB updates to commit
    mock_db_session.rollback.assert_not_called() # No exception directly in relay_cycle's try block to cause rollback
    mock_db_session.close.assert_called_once()

@pytest.mark.asyncio
@patch('backend.message_relay.SessionLocal')
@patch('backend.message_relay.get_unpublished_outbox_events')
@patch('backend.message_relay.update_outbox_event_status')
@patch('backend.message_relay.publish_to_nats', new_callable=AsyncMock)
async def test_relay_cycle_db_update_failure(mock_publish_nats, mock_update_status, mock_get_events, mock_session_local):
    # Arrange
    mock_event = MagicMock(spec=JobOutbox)
    mock_event.event_id = 3
    mock_event.job_id = "job_789"
    mock_event.nats_subject = "test.subject.dbfail"
    mock_event.nats_message_payload = {"data": "dbfail_payload"}
    mock_event.nats_message_id = "msg_789"

    mock_get_events.return_value = [mock_event]
    mock_publish_nats.return_value = True # NATS publish succeeds
    mock_update_status.return_value = None # Simulate DB update failure (event not found or other issue)

    mock_nc = MagicMock()
    mock_nc.is_connected = True
    mock_db_session = MagicMock(spec=Session)
    mock_session_local.return_value = mock_db_session

    # Act
    published_count = await relay_cycle(mock_nc)

    # Assert
    assert published_count == 0 # Because DB update failed, it's not counted as fully published
    mock_get_events.assert_called_once_with(mock_db_session, MAX_OUTBOX_FETCH_LIMIT)
    mock_publish_nats.assert_called_once()
    mock_update_status.assert_called_once() # Attempted to update status
    mock_db_session.commit.assert_not_called() # Commit should not be called as no event was successfully marked as PUBLISHED in DB for this cycle
    mock_db_session.rollback.assert_not_called() # No exception directly in relay_cycle's try block
    mock_db_session.close.assert_called_once()

@pytest.mark.asyncio
@patch('backend.message_relay.SessionLocal')
@patch('backend.message_relay.get_unpublished_outbox_events')
@patch('backend.message_relay.update_outbox_event_status') # Keep this to ensure it's not called if get_unpublished throws error
@patch('backend.message_relay.publish_to_nats', new_callable=AsyncMock)
async def test_relay_cycle_db_read_exception(mock_publish_nats, mock_update_status, mock_get_events, mock_session_local):
    # Arrange
    mock_get_events.side_effect = Exception("DB read error") # Simulate an exception during DB read

    mock_nc = MagicMock()
    mock_nc.is_connected = True
    mock_db_session = MagicMock(spec=Session)
    mock_session_local.return_value = mock_db_session

    # Act
    published_count = await relay_cycle(mock_nc)

    # Assert
    assert published_count == 0
    mock_get_events.assert_called_once_with(mock_db_session, MAX_OUTBOX_FETCH_LIMIT)
    mock_publish_nats.assert_not_called()
    mock_update_status.assert_not_called()
    mock_db_session.commit.assert_not_called()
    mock_db_session.rollback.assert_called_once() # Exception in try block should trigger rollback
    mock_db_session.close.assert_called_once()
