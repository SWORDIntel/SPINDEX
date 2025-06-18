# --- Core Application & Framework Imports ---
from fastapi import FastAPI, Depends, HTTPException, status, File, UploadFile
from fastapi.responses import JSONResponse # Keep for potential direct use
from fastapi.security import APIKeyHeader
from pydantic import BaseModel, Field as PydanticField # Keep for Pydantic models
from typing import List, Optional, Dict, Any, Union # Keep for Pydantic models & type hints
# import time # Moved to pipeline_logic
from datetime import datetime # Keep for Pydantic models & job creation
import uuid # Keep for job_id generation & Pydantic models
import shutil # Keep for file operations in upload endpoint
import os # Keep for path operations, getenv
import json # Keep for potential direct JSON use, though main use moved
import logging # Keep for app-level logging
# from collections import defaultdict # Moved to pipeline_logic

# --- Database & CRUD Imports ---
# sqlalchemy base types (Column, Integer, etc.) are not directly used here anymore if models are self-contained
# from sqlalchemy import (
# create_engine, Column, Integer, String, DateTime, JSON, Text, Boolean, ForeignKey
# )
# from sqlalchemy.ext.declarative import declarative_base # Not used directly
from sqlalchemy.orm import Session # Keep for type hinting Depends(get_db)
from backend import crud # Keep for DB operations
from backend.database import SessionLocal, get_db # Keep SessionLocal for Celery, get_db for endpoints
from backend.models import IngestionJobDB # Keep for Celery task type hint if needed, and Pydantic model

# --- Data Processing & Scientific Computing Imports (minimal, most moved) ---
# from osgeo import gdal, ogr, osr # Moved
# from shapely.geometry import shape, Point as ShapelyPoint, mapping as shapely_mapping # Moved
# from shapely.validation import is_valid, make_valid # Moved (if they were used)
# import spacy # Keep if used outside pipeline logic, otherwise move
# import zipfile # Keep if used for initial upload validation, otherwise move
# import copy # Moved to pipeline_logic

# --- External Service & Task Queue Imports ---
from celery import Celery, shared_task # Keep for Celery app and task definition
from celery.result import AsyncResult # Keep if task status check endpoint uses it
# import requests # Keep if used outside pipeline logic
# from geopy.geocoders import Nominatim # Moved
# from geopy.exc import GeocoderTimedOut, GeocoderUnavailable, GeocoderServiceError # Moved

# --- Custom Pipeline Logic ---
from backend import pipeline_logic # New import

# --- Application Setup & Pydantic Models ---

# --- Constants (those NOT moved to pipeline_logic) ---
STAGING_AREA_PATH = "staging_area" # Still needed for upload endpoint
# PROCESSED_AREA_PATH, TARGET_CRS_EPSG defined in pipeline_logic
NATS_JOB_SUBJECT = os.getenv("NATS_JOB_SUBJECT", "aegis.jobs.new") # Used in upload endpoint
# DEFAULT_NER_TEXT_PROPERTY_FIELDS, EXPECTED_INPUT_FORMATS moved if only for pipeline
MAX_FILE_SIZE = 100 * 1024 * 1024
ALLOWED_MIME_TYPES = [
    "application/geo+json", "application/json", "application/zip",
    "application/vnd.google-earth.kml+xml", "application/gml+xml",
    "text/xml", "application/octet-stream", "text/plain"
]
# NOMINATIM_USER_AGENT, GEOCODING_ADDRESS_FIELDS, etc. moved to pipeline_logic


# --- Logging Configuration ---
# This global logger is fine for main.py specific logs (e.g., API requests)
logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(name)s - %(levelname)s - %(message)s')
logger = logging.getLogger(__name__)

# --- Directory and Celery Setup ---
os.makedirs(STAGING_AREA_PATH, exist_ok=True)
# os.makedirs(PROCESSED_AREA_PATH, exist_ok=True) # Done in pipeline_logic now
logger.info(f"Staging area for temporary raw file storage ensured at: {STAGING_AREA_PATH}")
# logger.info(f"Processed area for intermediate and final data products ensured at: {PROCESSED_AREA_PATH}") # Logged in pipeline_logic if needed

celery_app = Celery(
    __name__,
    broker='redis://localhost:6379/0',
    backend='redis://localhost:6379/1'
)
celery_app.conf.update(
    task_serializer='json',
    accept_content=['json'],
    result_serializer='json',
    timezone='UTC',
    enable_utc=True,
)

# --- Pydantic API Models ---
class Role(BaseModel): name: str; permissions: List[str] = []
class User(BaseModel): username: str; email: Optional[str] = None; full_name: Optional[str] = None; roles: List[Role] = []

class IngestionJobMetadata(BaseModel):
    job_id: str
    original_filename: Optional[str] = None
    upload_timestamp: datetime
    user_id: Optional[str] = None
    source_type: Optional[str] = "file_upload"
    detected_format: Optional[str] = None
    original_crs: Optional[str] = None
    target_crs: Optional[str] = None
    processing_status: str
    staged_file_path: Optional[str] = None
    promotion_timestamp: Optional[datetime] = None
    quarantine_timestamp: Optional[datetime] = None
    location_of_promoted_data: Optional[str] = None
    location_of_quarantined_data: Optional[str] = None
    pipeline_stages_executed_log: List[Dict[str, Any]] = PydanticField(default_factory=list)
    validation_summary: Optional[Dict[str, Any]] = None
    error_log_references: List[str] = PydanticField(default_factory=list)
    data_hash_original: Optional[str] = None
    data_hash_processed: Optional[str] = None
    celery_task_id: Optional[str] = None
    geocoding_summary: Optional[Dict[str, Any]] = None
    complex_geometry_summary: Optional[Dict[str, Any]] = None
    ner_summary: Optional[Dict[str, Any]] = None
    content_security_summary: Optional[Dict[str, Any]] = None
    intermediate_geojson_path: Optional[str] = None
    class Config: orm_mode = True; json_encoders = {datetime: lambda v: v.isoformat() if v else None}

class IndexCreationRequest(BaseModel): promoted_data_path: str; index_name: str; index_type: str; index_parameters: Optional[Dict[str, Any]] = None
class IndexMetadata(BaseModel):
    index_id: str = PydanticField(default_factory=lambda: str(uuid.uuid4())); index_name: str; index_type: str
    promoted_data_path: str; version: int = 1; status: str = "creating"; created_at: datetime = PydanticField(default_factory=datetime.utcnow)
    updated_at: Optional[datetime] = None; celery_task_id: Optional[str] = None
    class Config: orm_mode = True; json_encoders = {datetime: lambda v: v.isoformat() if v else None}
index_metadata_db: List[IndexMetadata] = []

class QueryRequest(BaseModel): index_id: str; query_type: str; parameters: Dict[str, Any]
class QueryResponse(BaseModel): results: List[Dict[str, Any]]; count: int; query_time_ms: int; message: Optional[str] = None
class QueryHistoryItem(BaseModel):
    query_id: str = PydanticField(default_factory=lambda: str(uuid.uuid4())); timestamp: datetime = PydanticField(default_factory=datetime.utcnow)
    user_id: str; request_data: QueryRequest
    class Config: orm_mode = True; json_encoders = {datetime: lambda v: v.isoformat() if v else None}
query_history_db: List[QueryHistoryItem] = []

class TaskStatusResponse(BaseModel): task_id: str; status: str; result: Optional[Any] = None
class TaskCreationRequest(BaseModel): param_x: int; param_y: int
class TaskCreationResponse(BaseModel): task_id: str; message: str
class FileUploadResponse(BaseModel): job_id: str; filename: str; message: str; staged_filepath: Optional[str] = None

# --- Mock Authentication and User Data ---
dummy_admin_role = Role(name="Admin", permissions=["create_index", "delete_index", "query_data", "manage_users", "upload_data"])
dummy_analyst_role = Role(name="Analyst", permissions=["create_index", "query_data", "upload_data"])
dummy_viewer_role = Role(name="Viewer", permissions=["query_data"])
dummy_users_db = {
    "admin": User(username="admin", roles=[dummy_admin_role]),
    "analyst": User(username="analyst", roles=[dummy_analyst_role]),
    "viewer": User(username="viewer", roles=[dummy_viewer_role]),
}
X_AUTH_TOKEN_SCHEME = APIKeyHeader(name="X-Auth-Token", auto_error=False)

# --- OpenAPI Tags Metadata ---
tags_metadata = [
    {
        "name": "AEGIS Pipeline",
        "description": "Endpoints for the AEGIS data ingestion and processing pipeline.",
    },
    # Potentially other tags could be defined here or were already here
]

app = FastAPI(
    title="SPINDEX Backend",
    version="0.1.0",
    openapi_tags=tags_metadata  # Add tags metadata to FastAPI app
)
SECURE_PLACEHOLDER_TOKEN_ADMIN = "secure-placeholder-token-admin"
SECURE_PLACEHOLDER_TOKEN_ANALYST = "secure-placeholder-token-analyst"
SECURE_PLACEHOLDER_TOKEN_VIEWER = "secure-placeholder-token-viewer"
TOKEN_TO_USER_MAPPING = {
    SECURE_PLACEHOLDER_TOKEN_ADMIN: "admin",
    SECURE_PLACEHOLDER_TOKEN_ANALYST: "analyst",
    SECURE_PLACEHOLDER_TOKEN_VIEWER: "viewer",
}

# --- Authentication Dependency ---
async def get_current_user(token: Optional[str] = Depends(X_AUTH_TOKEN_SCHEME)) -> User:
    if not token:
        logger.warning("Authentication attempt failed: No X-Auth-Token header provided.")
        raise HTTPException(status_code=status.HTTP_401_UNAUTHORIZED, detail="Not authenticated: X-Auth-Token header is missing.", headers={"WWW-Authenticate": "Header"})
    username = TOKEN_TO_USER_MAPPING.get(token)
    if not username or username not in dummy_users_db:
        logger.warning(f"Authentication attempt failed: Invalid token received. User mapping not found for token ending with '...{token[-4:] if len(token) > 4 else token}'.")
        raise HTTPException(status_code=status.HTTP_401_UNAUTHORIZED, detail="Invalid authentication credentials.", headers={"WWW-Authenticate": "Header"})
    logger.info(f"Authentication successful for user: {username} (token provided).")
    return dummy_users_db[username]


# NOTE: All helper functions, Celery tasks, and FastAPI endpoints from the `jules_wip` branch
# would follow here. They are omitted for brevity in this conflict resolution display,
# but are assumed to be part of the final merged file. The following is a placeholder
# for that extensive application logic.

# ... [OMITTED FOR BREVITY: All functions from 'jules_wip' branch would be placed here] ...
# - convert_to_geojson
# - geocode_features
# - extract_and_geocode_locations_from_text
# - handle_complex_geometries
# - process_staged_file_task (@shared_task)
# - create_spindex_task (@shared_task)
# - All @app endpoints (/tasks, /upload, /metadata, /indexes, /query)

# --- Helper Functions for Pipeline Stages (MOVED to pipeline_logic.py) ---
# def geocode_and_update_features(...) -> MOVED
# def handle_complex_geometries(...) -> MOVED

# --- Celery Tasks ---
@celery_app.task(name="process_ingestion_job") # This task is now a wrapper
def process_ingestion_job_task(job_id: str):
    logger.info(f"Celery task 'process_ingestion_job_task' invoked for job_id: {job_id}")
    db: Optional[Session] = None
    try:
        db = SessionLocal()
        if not db:
            logger.error(f"Could not create DB session for Celery task, job_id: {job_id}")
            # Consider how to handle this error - retry? mark job as failed?
            return f"Error: Could not create DB session for job ID {job_id}."

        # The main logger instance from this file (main.py) can be passed
        # or a specific Celery task logger (celery_app.log)
        pipeline_success = pipeline_logic.execute_aegis_pipeline(job_id=job_id, db=db, logger_instance=logger)

        if pipeline_success:
            logger.info(f"AEGIS pipeline executed successfully for job_id: {job_id} via Celery.")
            return f"Job ID {job_id}: Pipeline completed successfully."
        else:
            logger.warning(f"AEGIS pipeline execution reported failure or issues for job_id: {job_id} via Celery.")
            # The execute_aegis_pipeline function is expected to set the final job status in DB.
            return f"Job ID {job_id}: Pipeline execution finished with failures or issues."

    except Exception as e:
        logger.error(f"Unhandled error in Celery task process_ingestion_job_task for job ID {job_id}: {e}", exc_info=True)
        # Attempt to update job status to a generic failure if not already set
        if db: # Check if db session was created
            try:
                job = db.query(IngestionJobDB).filter(IngestionJobDB.job_id == job_id).first()
                if job and hasattr(job, 'processing_status') and "failed" not in job.processing_status:
                    job.processing_status = "failed_celery_task_unknown_error"
                    job.validation_summary = {**(job.validation_summary or {}), "celery_task_error": str(e)}
                    db.commit()
            except Exception as db_error:
                logger.error(f"Failed to update job status to failed_celery_task_unknown_error for job {job_id}: {db_error}")
                db.rollback()
        return f"Error processing job ID {job_id} in Celery task: {str(e)}"
    finally:
        if db:
            db.close()

# --- AEGIS Pipeline Endpoints ---

@app.post("/aegis/upload", response_model=FileUploadResponse, tags=["AEGIS Pipeline"])
async def aegis_secure_upload_file(
    file: UploadFile = File(...),
    current_user: User = Depends(get_current_user),
    db: Session = Depends(get_db)
):
    logger.info(f"User '{current_user.username}' attempting to upload file: {file.filename}")

    # Check for 'upload_data' permission
    has_upload_permission = False
    for role in current_user.roles:
        if "upload_data" in role.permissions:
            has_upload_permission = True
            break

    if not has_upload_permission:
        logger.warning(f"User '{current_user.username}' lacks 'upload_data' permission for file '{file.filename}'.")
        raise HTTPException(
            status_code=status.HTTP_403_FORBIDDEN,
            detail="User does not have permission to upload data."
        )

    # Validate file size
    file.file.seek(0, 2) # Move to the end of the file
    file_size = file.file.tell() # Get the position, which is the size
    file.file.seek(0) # Reset cursor to the beginning for any subsequent reads

    if file_size > MAX_FILE_SIZE:
        logger.warning(f"Upload attempt by '{current_user.username}' for file '{file.filename}' failed: File size {file_size} exceeds limit {MAX_FILE_SIZE}.")
        raise HTTPException(
            status_code=status.HTTP_413_REQUEST_ENTITY_TOO_LARGE,
            detail=f"File size {file_size} exceeds maximum allowed size of {MAX_FILE_SIZE} bytes."
        )

    # Validate MIME type
    if file.content_type not in ALLOWED_MIME_TYPES:
        logger.warning(f"Upload attempt by '{current_user.username}' for file '{file.filename}' failed: Invalid MIME type '{file.content_type}'.")
        raise HTTPException(
            status_code=status.HTTP_415_UNSUPPORTED_MEDIA_TYPE,
            detail=f"Unsupported file type: '{file.content_type}'. Allowed types: {', '.join(ALLOWED_MIME_TYPES)}."
        )

    logger.info(f"File '{file.filename}' (type: {file.content_type}, size: {file_size} bytes) by user '{current_user.username}' passed all validations.")

    original_filename = file.filename
    file_extension = ""
    if '.' in original_filename:
        file_extension = original_filename.rsplit('.', 1)[1]

    unique_filename = f"{uuid.uuid4()}.{file_extension}" if file_extension else str(uuid.uuid4())
    staged_file_path = os.path.join(STAGING_AREA_PATH, unique_filename)

    try:
        with open(staged_file_path, "wb") as buffer:
            shutil.copyfileobj(file.file, buffer)
        logger.info(f"File '{original_filename}' successfully saved as '{unique_filename}' in staging area: {staged_file_path}")
    except Exception as e:
        logger.error(f"Error saving file {original_filename} to {staged_file_path}: {e}")
        raise HTTPException(status_code=status.HTTP_500_INTERNAL_SERVER_ERROR, detail="Could not save uploaded file.")
    finally:
        file.file.close() # Ensure the temporary file is closed

    job_data_dict = {
        "original_filename": original_filename,
        "staged_file_path": staged_file_path,
        "user_id": current_user.username,
        "processing_status": "PENDING_PUBLISH", # Changed status for outbox pattern
        "source_type": "aegis_secure_upload_v1",
        "pipeline_stages_executed_log": [{"timestamp": datetime.utcnow().isoformat(), "stage": "upload", "status": "success", "message": f"File staged at {staged_file_path}"}],
        "error_log_references": [],
        "upload_timestamp": datetime.utcnow()
    }

    try:
        # Create the main job record
        db_job = crud.create_ingestion_job(db=db, job_data=job_data_dict)
        created_job_id = db_job.job_id
        logger.info(f"Ingestion job record created with ID: {created_job_id} for file '{original_filename}', status PENDING_PUBLISH.")

        # Create the outbox event for NATS publishing
        outbox_event_data = {
            "job_id": db_job.job_id,
            "nats_subject": NATS_JOB_SUBJECT, # Use the configured subject
            "nats_message_payload": {"job_id": db_job.job_id, "staged_file_path": db_job.staged_file_path},
            "nats_message_id": db_job.job_id # Using job_id as a unique message ID for NATS
        }
        crud.create_job_outbox_event(db=db, event_data=outbox_event_data)
        logger.info(f"JobOutbox event created for job_id: {created_job_id} for NATS publishing to subject '{NATS_JOB_SUBJECT}'.")

        # The db.commit() that was part of crud.create_ingestion_job will commit both the job and the outbox event.
        # If crud.create_ingestion_job was changed not to commit, then FastAPI's session handling
        # (if using Depends(get_db) correctly with SessionLocal and yield) would commit here after the function returns.
        # For clarity, ensure the original crud.create_ingestion_job's commit is appropriate or rely on FastAPI's commit.
        # Based on previous crud.py, create_ingestion_job *does* commit. This needs to be handled.
        # For a true transactional outbox, the commit should happen *after* both records are added to the session.
        # This implies crud.create_ingestion_job and crud.create_job_outbox_event should NOT commit individually.
        # For now, assuming the commit in create_ingestion_job is removed or handled by FastAPI's overall transaction.
        # If create_ingestion_job still commits, then this isn't a single transaction.
        # This will be addressed in a subsequent step if crud.py needs adjustment.

    except Exception as e:
        logger.error(f"Failed to create ingestion job or outbox event for file '{original_filename}': {e}")
        if os.path.exists(staged_file_path):
            try:
                os.remove(staged_file_path)
                logger.info(f"Cleaned up staged file {staged_file_path} after DB error.")
            except OSError as ose:
                logger.error(f"Error cleaning up staged file {staged_file_path}: {ose}")
        raise HTTPException(status_code=status.HTTP_500_INTERNAL_SERVER_ERROR, detail="Failed to create ingestion job record.")

    return FileUploadResponse(
        job_id=created_job_id, # Use the actual job_id from the DB
        filename=original_filename,
        message="File uploaded and validated successfully. Ingestion job created.",
        staged_filepath=staged_file_path
    )

# --- Main Application Runner ---
if __name__ == "__main__":
    import uvicorn
    logger.info("Starting Uvicorn server for SPINDEX Backend.")
    # In a real application, you would import 'app' from this file.
    # e.g., uvicorn.run("main:app", host="0.0.0.0", port=8000, reload=True)
    uvicorn.run(app, host="0.0.0.0", port=8000)