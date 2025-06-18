# backend/tests/test_outbox_and_relay.py
import os
import shutil
import uuid
from fastapi.testclient import TestClient
from sqlalchemy.orm import Session
import pytest

from unittest.mock import patch, MagicMock, AsyncMock, ANY
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


# Imports for worker testing
import nats # Required for nats.aio.client.Msg if directly instantiating
from backend.aegis_worker import run_worker # shutdown_event can be mocked if needed
from backend import pipeline_logic # To mock execute_aegis_pipeline
# Note: aegis_worker.logger will be the logger instance used by the worker


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

        job_id_to_delete = data["job_id"]
        staged_file_path_to_delete = data["staged_filepath"]
        assert os.path.exists(staged_file_path_to_delete)

        db = next(get_test_db_session_for_outbox_tests())
        try:
            job_db = crud.get_ingestion_job(db, job_id_to_delete)
            assert job_db is not None
            assert job_db.original_filename == test_filename
            assert job_db.user_id == "admin"
            assert job_db.processing_status == "PENDING_PUBLISH"
            assert job_db.celery_task_id is None

            outbox_event = db.query(JobOutbox).filter(JobOutbox.job_id == job_id_to_delete).first()
            assert outbox_event is not None
            outbox_event_id_to_delete = outbox_event.event_id
            assert outbox_event.nats_subject == NATS_JOB_SUBJECT
            assert outbox_event.published_status == "UNPUBLISHED"
            assert outbox_event.nats_message_id == job_id_to_delete
            payload = outbox_event.nats_message_payload
            assert payload["job_id"] == job_id_to_delete
            assert payload["staged_file_path"] == staged_file_path_to_delete
            mock_celery_delay.assert_not_called()
        finally:
            db.close()
    finally:
        if os.path.exists(test_filename): os.remove(test_filename)
        if staged_file_path_to_delete and os.path.exists(staged_file_path_to_delete):

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

                if event_to_del: db_cleanup.delete(event_to_del)
            if job_id_to_delete:
                crud.remove_ingestion_job(db_cleanup, job_id_to_delete)
            db_cleanup.commit()
        finally:
            db_cleanup.close()

# --- Tests for Message Relay Cycle ---
# (Existing relay tests remain here)

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

@patch('backend.message_relay.publish_to_nats', new_callable=AsyncMock)
async def test_relay_cycle_no_events(mock_publish_nats, mock_update_status, mock_get_events, mock_session_local):
    mock_get_events.return_value = []
    mock_nc = MagicMock(); mock_nc.is_connected = True
    mock_db_session = MagicMock(spec=Session)
    mock_session_local.return_value = mock_db_session
    published_count = await relay_cycle(mock_nc)

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

    mock_event = MagicMock(spec=JobOutbox)
    mock_event.event_id = 1; mock_event.job_id = "job_123"
    mock_event.nats_subject = "test.subject"; mock_event.nats_message_payload = {"data": "test_payload"}
    mock_event.nats_message_id = "msg_123"
    mock_get_events.return_value = [mock_event]
    mock_publish_nats.return_value = True
    mock_update_status.return_value = mock_event
    mock_nc = MagicMock(); mock_nc.is_connected = True
    mock_db_session = MagicMock(spec=Session)
    mock_session_local.return_value = mock_db_session
    published_count = await relay_cycle(mock_nc)
    assert published_count == 1
    mock_publish_nats.assert_called_once_with(mock_nc, "test.subject", json.dumps({"data": "test_payload"}).encode('utf-8'), "msg_123")
    args, kwargs = mock_update_status.call_args
    assert args[1] == mock_event.event_id; assert args[2] == "PUBLISHED"; assert isinstance(kwargs['published_at'], datetime)
    mock_db_session.commit.assert_called_once()

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

    mock_event = MagicMock(spec=JobOutbox)
    mock_event.event_id = 2; mock_event.job_id = "job_456"
    mock_event.nats_subject = "test.subject.fail"; mock_event.nats_message_payload = {"data": "fail_payload"}
    mock_event.nats_message_id = "msg_456"
    mock_get_events.return_value = [mock_event]
    mock_publish_nats.return_value = False
    mock_nc = MagicMock(); mock_nc.is_connected = True
    mock_db_session = MagicMock(spec=Session)
    mock_session_local.return_value = mock_db_session
    published_count = await relay_cycle(mock_nc)
    assert published_count == 0
    mock_publish_nats.assert_called_once()
    mock_update_status.assert_not_called()
    mock_db_session.commit.assert_not_called()

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

    mock_event = MagicMock(spec=JobOutbox)
    mock_event.event_id = 3; mock_event.job_id = "job_789"
    mock_event.nats_subject = "test.subject.dbfail"; mock_event.nats_message_payload = {"data": "dbfail_payload"}
    mock_event.nats_message_id = "msg_789"
    mock_get_events.return_value = [mock_event]
    mock_publish_nats.return_value = True
    mock_update_status.return_value = None
    mock_nc = MagicMock(); mock_nc.is_connected = True
    mock_db_session = MagicMock(spec=Session)
    mock_session_local.return_value = mock_db_session
    published_count = await relay_cycle(mock_nc)
    assert published_count == 0
    mock_update_status.assert_called_once()
    mock_db_session.commit.assert_not_called()

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

@patch('backend.message_relay.update_outbox_event_status')
@patch('backend.message_relay.publish_to_nats', new_callable=AsyncMock)
async def test_relay_cycle_db_read_exception(mock_publish_nats, mock_update_status, mock_get_events, mock_session_local):
    mock_get_events.side_effect = Exception("DB read error")
    mock_nc = MagicMock(); mock_nc.is_connected = True
    mock_db_session = MagicMock(spec=Session)
    mock_session_local.return_value = mock_db_session
    published_count = await relay_cycle(mock_nc)
    assert published_count == 0
    mock_db_session.rollback.assert_called_once()

# --- Tests for AEGIS Worker ---

# Helper to create a mock NATS message
def create_mock_nats_msg(data: dict):
    msg = MagicMock(spec=nats.aio.client.Msg)
    msg.data = json.dumps(data).encode('utf-8')
    msg.ack = AsyncMock()
    msg.term = AsyncMock()
    msg.nak = AsyncMock()
    return msg

# Custom exception to break worker loop in tests
class TestWorkerStopException(Exception):
    pass

@pytest.mark.asyncio
@patch('backend.aegis_worker.nats.connect', new_callable=AsyncMock)
@patch('backend.aegis_worker.SessionLocal')
@patch('backend.aegis_worker.get_job_status')
@patch('backend.pipeline_logic.execute_aegis_pipeline')
@patch('backend.aegis_worker.asyncio.sleep', new_callable=AsyncMock)
async def test_worker_receives_and_processes_valid_message_success_path(
    mock_async_sleep, mock_execute_pipeline_actual, mock_get_status, mock_session_local, mock_nats_connect
):
    mock_nc = AsyncMock(); mock_js = AsyncMock(); mock_psub = AsyncMock()
    mock_nats_connect.return_value = mock_nc; mock_nc.jetstream.return_value = mock_js
    mock_js.pull_subscribe.return_value = mock_psub
    mock_msg_data = {"job_id": "job-success-123"}
    mock_msg = create_mock_nats_msg(mock_msg_data)
    mock_psub.fetch = AsyncMock(side_effect=[[mock_msg], TestWorkerStopException])
    mock_db_session = MagicMock(spec=Session)
    mock_session_local.return_value = mock_db_session
    mock_get_status.return_value = "PENDING_PUBLISH"

    mock_execute_pipeline_sync = MagicMock(return_value=True)
    with patch('asyncio.get_running_loop') as mock_get_loop:
        mock_loop = MagicMock(); mock_loop.run_in_executor = AsyncMock(side_effect=lambda _, func, *args: mock_execute_pipeline_sync(*args))
        mock_get_loop.return_value = mock_loop
        with pytest.raises(TestWorkerStopException): await run_worker()

    mock_get_status.assert_called_once_with(mock_db_session, "job-success-123")
    mock_execute_pipeline_sync.assert_called_once_with("job-success-123", mock_db_session, ANY)
    mock_msg.ack.assert_called_once()

@pytest.mark.asyncio
@patch('backend.aegis_worker.nats.connect', new_callable=AsyncMock)
@patch('backend.aegis_worker.SessionLocal')
@patch('backend.aegis_worker.get_job_status')
@patch('backend.pipeline_logic.execute_aegis_pipeline')
@patch('backend.aegis_worker.asyncio.sleep', new_callable=AsyncMock)
async def test_worker_idempotency_job_already_completed(
    mock_async_sleep, mock_execute_pipeline, mock_get_status, mock_session_local, mock_nats_connect
):
    mock_nc = AsyncMock(); mock_js = AsyncMock(); mock_psub = AsyncMock()
    mock_nats_connect.return_value = mock_nc; mock_nc.jetstream.return_value = mock_js
    mock_js.pull_subscribe.return_value = mock_psub
    mock_msg = create_mock_nats_msg({"job_id": "job-completed-456"})
    mock_psub.fetch = AsyncMock(side_effect=[[mock_msg], TestWorkerStopException])
    mock_db_session = MagicMock(spec=Session)
    mock_session_local.return_value = mock_db_session
    mock_get_status.return_value = "COMPLETED_PIPELINE"

    with pytest.raises(TestWorkerStopException): await run_worker()

    mock_get_status.assert_called_once_with(mock_db_session, "job-completed-456")
    mock_execute_pipeline.assert_not_called()
    mock_msg.ack.assert_called_once()

@pytest.mark.asyncio
@patch('backend.aegis_worker.nats.connect', new_callable=AsyncMock)
@patch('backend.aegis_worker.SessionLocal')
@patch('backend.aegis_worker.get_job_status')
@patch('backend.pipeline_logic.execute_aegis_pipeline')
@patch('backend.aegis_worker.asyncio.sleep', new_callable=AsyncMock)
async def test_worker_idempotency_job_not_found_in_db(
    mock_async_sleep, mock_execute_pipeline, mock_get_status, mock_session_local, mock_nats_connect
):
    mock_nc = AsyncMock(); mock_js = AsyncMock(); mock_psub = AsyncMock()
    mock_nats_connect.return_value = mock_nc; mock_nc.jetstream.return_value = mock_js
    mock_js.pull_subscribe.return_value = mock_psub
    mock_msg = create_mock_nats_msg({"job_id": "job-notfound-789"})
    mock_psub.fetch = AsyncMock(side_effect=[[mock_msg], TestWorkerStopException])
    mock_db_session = MagicMock(spec=Session)
    mock_session_local.return_value = mock_db_session
    mock_get_status.return_value = None

    with pytest.raises(TestWorkerStopException): await run_worker()
    mock_get_status.assert_called_once_with(mock_db_session, "job-notfound-789")
    mock_execute_pipeline.assert_not_called()
    mock_msg.term.assert_called_once()

@pytest.mark.asyncio
@patch('backend.aegis_worker.nats.connect', new_callable=AsyncMock)
@patch('backend.aegis_worker.SessionLocal')
@patch('backend.pipeline_logic.execute_aegis_pipeline')
@patch('backend.aegis_worker.asyncio.sleep', new_callable=AsyncMock)
async def test_worker_handles_malformed_json_message(
    mock_async_sleep, mock_execute_pipeline, mock_session_local, mock_nats_connect
):
    mock_nc = AsyncMock(); mock_js = AsyncMock(); mock_psub = AsyncMock()
    mock_nats_connect.return_value = mock_nc; mock_nc.jetstream.return_value = mock_js
    mock_js.pull_subscribe.return_value = mock_psub
    malformed_msg = MagicMock(spec=nats.aio.client.Msg)
    malformed_msg.data = b"this is not json"; malformed_msg.ack = AsyncMock(); malformed_msg.term = AsyncMock()
    mock_psub.fetch = AsyncMock(side_effect=[[malformed_msg], TestWorkerStopException])
    with pytest.raises(TestWorkerStopException): await run_worker()
    mock_execute_pipeline.assert_not_called()
    malformed_msg.term.assert_called_once()

@pytest.mark.asyncio
@patch('backend.aegis_worker.nats.connect', new_callable=AsyncMock)
@patch('backend.aegis_worker.SessionLocal')
@patch('backend.pipeline_logic.execute_aegis_pipeline')
@patch('backend.aegis_worker.asyncio.sleep', new_callable=AsyncMock)
async def test_worker_handles_message_missing_job_id(
    mock_async_sleep, mock_execute_pipeline, mock_session_local, mock_nats_connect
):
    mock_nc = AsyncMock(); mock_js = AsyncMock(); mock_psub = AsyncMock()
    mock_nats_connect.return_value = mock_nc; mock_nc.jetstream.return_value = mock_js
    mock_js.pull_subscribe.return_value = mock_psub
    msg_no_job_id = create_mock_nats_msg({"staged_file_path": "/path/to/file"})
    mock_psub.fetch = AsyncMock(side_effect=[[msg_no_job_id], TestWorkerStopException])
    with pytest.raises(TestWorkerStopException): await run_worker()
    mock_execute_pipeline.assert_not_called()
    msg_no_job_id.term.assert_called_once()

@pytest.mark.asyncio
@patch('backend.aegis_worker.nats.connect', new_callable=AsyncMock)
@patch('backend.aegis_worker.SessionLocal')
@patch('backend.aegis_worker.get_job_status')
@patch('backend.pipeline_logic.execute_aegis_pipeline')
@patch('backend.aegis_worker.asyncio.sleep', new_callable=AsyncMock)
async def test_worker_pipeline_returns_false_job_in_terminal_failed_state(
    mock_async_sleep, mock_execute_pipeline_module, mock_get_status, mock_session_local, mock_nats_connect
):
    mock_nc = AsyncMock(); mock_js = AsyncMock(); mock_psub = AsyncMock()
    mock_nats_connect.return_value = mock_nc; mock_nc.jetstream.return_value = mock_js
    mock_js.pull_subscribe.return_value = mock_psub
    mock_msg_data = {"job_id": "job-pipe-fail-term-1"}
    mock_msg = create_mock_nats_msg(mock_msg_data)
    mock_psub.fetch = AsyncMock(side_effect=[[mock_msg], TestWorkerStopException])

    mock_db_session = MagicMock(spec=Session)
    mock_session_local.return_value = mock_db_session

    # First call for idempotency, second call after pipeline failure
    mock_get_status.side_effect = ["PENDING_PUBLISH", "MODULE_FAILED_XYZ"]

    mock_execute_pipeline_sync = MagicMock(return_value=False) # Pipeline indicates failure
    with patch('asyncio.get_running_loop') as mock_get_loop:
        mock_loop = MagicMock(); mock_loop.run_in_executor = AsyncMock(side_effect=lambda _, func, *args: mock_execute_pipeline_sync(*args))
        mock_get_loop.return_value = mock_loop
        with pytest.raises(TestWorkerStopException): await run_worker()

    assert mock_get_status.call_count == 2
    mock_execute_pipeline_sync.assert_called_once()
    mock_msg.ack.assert_called_once() # ACK because pipeline handled and set terminal state

@pytest.mark.asyncio
@patch('backend.aegis_worker.nats.connect', new_callable=AsyncMock)
@patch('backend.aegis_worker.SessionLocal')
@patch('backend.aegis_worker.get_job_status')
@patch('backend.pipeline_logic.execute_aegis_pipeline')
@patch('backend.aegis_worker.asyncio.sleep', new_callable=AsyncMock)
async def test_worker_pipeline_returns_false_job_not_in_terminal_failed_state(
    mock_async_sleep, mock_execute_pipeline_module, mock_get_status, mock_session_local, mock_nats_connect
):
    mock_nc = AsyncMock(); mock_js = AsyncMock(); mock_psub = AsyncMock()
    mock_nats_connect.return_value = mock_nc; mock_nc.jetstream.return_value = mock_js
    mock_js.pull_subscribe.return_value = mock_psub
    mock_msg_data = {"job_id": "job-pipe-fail-nonterm-2"}
    mock_msg = create_mock_nats_msg(mock_msg_data)
    mock_psub.fetch = AsyncMock(side_effect=[[mock_msg], TestWorkerStopException])

    mock_db_session = MagicMock(spec=Session)
    mock_session_local.return_value = mock_db_session
    mock_get_status.side_effect = ["PENDING_PUBLISH", "PROCESSING_GEOMETRY"] # Non-terminal state after pipeline "failure"

    mock_execute_pipeline_sync = MagicMock(return_value=False) # Pipeline indicates failure
    with patch('asyncio.get_running_loop') as mock_get_loop:
        mock_loop = MagicMock(); mock_loop.run_in_executor = AsyncMock(side_effect=lambda _, func, *args: mock_execute_pipeline_sync(*args))
        mock_get_loop.return_value = mock_loop
        with pytest.raises(TestWorkerStopException): await run_worker()

    assert mock_get_status.call_count == 2
    mock_execute_pipeline_sync.assert_called_once()
    mock_msg.ack.assert_not_called() # Not ACKed due to non-terminal failure state
    mock_msg.nak.assert_not_called() # Current worker does not explicitly NAK based on this

@pytest.mark.asyncio
@patch('backend.aegis_worker.nats.connect', new_callable=AsyncMock)
@patch('backend.aegis_worker.SessionLocal')
@patch('backend.aegis_worker.get_job_status')
@patch('backend.pipeline_logic.execute_aegis_pipeline') # This will be for the run_in_executor call
@patch('backend.aegis_worker.asyncio.sleep', new_callable=AsyncMock)
async def test_worker_pipeline_raises_exception(
    mock_async_sleep, mock_execute_pipeline_module, mock_get_status, mock_session_local, mock_nats_connect
):
    mock_nc = AsyncMock(); mock_js = AsyncMock(); mock_psub = AsyncMock()
    mock_nats_connect.return_value = mock_nc; mock_nc.jetstream.return_value = mock_js
    mock_js.pull_subscribe.return_value = mock_psub
    mock_msg_data = {"job_id": "job-pipe-exception-3"}
    mock_msg = create_mock_nats_msg(mock_msg_data)
    mock_psub.fetch = AsyncMock(side_effect=[[mock_msg], TestWorkerStopException])

    mock_db_session = MagicMock(spec=Session)
    mock_session_local.return_value = mock_db_session
    mock_get_status.return_value = "PENDING_PUBLISH" # Initial check passes

    # Simulate exception during the execution of pipeline_logic.execute_aegis_pipeline
    mock_execute_pipeline_sync = MagicMock(side_effect=Exception("Pipeline processing error!"))

    with patch('asyncio.get_running_loop') as mock_get_loop:
        mock_loop = MagicMock()
        # run_in_executor will propagate the exception from mock_execute_pipeline_sync
        mock_loop.run_in_executor = AsyncMock(side_effect=lambda _, func, *args: mock_execute_pipeline_sync(*args))
        mock_get_loop.return_value = mock_loop
        with pytest.raises(TestWorkerStopException): await run_worker()

    mock_get_status.assert_called_once() # Only initial idempotency check
    mock_execute_pipeline_sync.assert_called_once() # Attempted to execute
    mock_msg.ack.assert_not_called() # Should not ACK on unhandled exception
    mock_msg.nak.assert_not_called() # Current worker does not explicitly NAK
    # Rollback is not explicitly tested here as it's within the SessionLocal context manager
    # of execute_aegis_pipeline or the worker's own session handling for get_job_status.
    # The key is that the message is not ACKed.
    # The worker's main try/except e_proc would catch this and prevent ack.
    assert mock_db_session.close.call_count >= 1 # Session should be closed at least once

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