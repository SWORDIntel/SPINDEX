from sqlalchemy.orm import Session
from sqlalchemy import asc # Add asc for ordering
from backend import models, database # Import SQLAlchemy model
from datetime import datetime
from typing import List, Dict, Any, Optional
import uuid # For index_id default if needed, though Pydantic model should provide it

# Attempt to import Pydantic model, handle potential circularity if this file grows
try:
    from backend.main import IndexMetadata as IndexMetadataPydantic
    from backend.main import QueryHistoryItem as QueryHistoryItemPydantic
    # QueryRequest is part of QueryHistoryItemPydantic, so direct import might not be needed here
    # from backend.main import QueryRequest as QueryRequestPydantic
except ImportError:
    # This is a fallback. In a real scenario, restructure to avoid circular deps,
    # or use a more generic Dict[str, Any] for creation if Pydantic model not available at this point.
    # For this subtask, we assume direct import is fine or will be resolved by overall structure.
    IndexMetadataPydantic = Dict[str, Any] # Placeholder if import fails
    QueryHistoryItemPydantic = Dict[str, Any] # Placeholder if import fails

# CRUD operations for IngestionJobDB

def get_ingestion_job(db: Session, job_id: str) -> Optional[models.IngestionJobDB]:
    return db.query(models.IngestionJobDB).filter(models.IngestionJobDB.job_id == job_id).first()

def get_ingestion_jobs(db: Session, skip: int = 0, limit: int = 100) -> List[models.IngestionJobDB]:
    return db.query(models.IngestionJobDB).order_by(models.IngestionJobDB.upload_timestamp.desc()).offset(skip).limit(limit).all()

def create_ingestion_job(db: Session, job_data: Dict[str, Any]) -> models.IngestionJobDB:
    # Ensure defaultable fields are handled if not in job_data
    db_job = models.IngestionJobDB(
        job_id=job_data.get('job_id') or str(database.uuid.uuid4()), # Ensure job_id is always set
        original_filename=job_data.get('original_filename'),
        upload_timestamp=job_data.get('upload_timestamp', datetime.utcnow()),
        user_id=job_data.get('user_id'),
        source_type=job_data.get('source_type', 'file_upload'),
        processing_status=job_data.get('processing_status', 'pending_staging'),
        staged_file_path=job_data.get('staged_file_path'),
        celery_task_id=job_data.get('celery_task_id'),
        pipeline_stages_executed_log=job_data.get('pipeline_stages_executed_log', []),
        error_log_references=job_data.get('error_log_references', []),
        validation_summary=job_data.get('validation_summary'),
        geocoding_summary=job_data.get('geocoding_summary'),
        complex_geometry_summary=job_data.get('complex_geometry_summary'),
    ner_summary=job_data.get('ner_summary'),
    # Ensure all fields from the model are handled, defaulting to None if not applicable at creation
    detected_format=job_data.get('detected_format', None),
    original_crs=job_data.get('original_crs', None),
    target_crs=job_data.get('target_crs', None),
    location_of_promoted_data=job_data.get('location_of_promoted_data', None),
    location_of_quarantined_data=job_data.get('quarantined_data', None),
    promotion_timestamp=job_data.get('promotion_timestamp', None),
    quarantine_timestamp=job_data.get('quarantine_timestamp', None),
    data_hash_original=job_data.get('data_hash_original', None),
    data_hash_processed=job_data.get('data_hash_processed', None),
    content_security_summary=job_data.get('content_security_summary', None),
    intermediate_geojson_path=job_data.get('intermediate_geojson_path', None)
    )
    db.add(db_job)
    # db.commit() # REMOVED: Commit will be handled by the calling endpoint logic
    # db.refresh(db_job) # REMOVED: Refresh will be handled by the calling endpoint logic
    return db_job

def update_ingestion_job_fields(db: Session, job_id: str, fields_to_update: Dict[str, Any]) -> Optional[models.IngestionJobDB]:
    db_job = get_ingestion_job(db, job_id)
    if db_job:
        for key, value in fields_to_update.items():
            if hasattr(db_job, key):
                setattr(db_job, key, value)
            else:
                # Handle cases where the key might not directly map, or log a warning
                print(f"Warning: Field {key} not found in IngestionJobDB model.")

        # Special handling for list appends if necessary (pipeline_stages_executed_log, error_log_references)
        # The current approach in main.py for these (read, append, write) is okay for JSON fields.
        # If these were separate related tables, different logic would apply.

        # Note: The IngestionJobDB model does not currently have an 'updated_at' field
        # with an onupdate trigger. If it did, SQLAlchemy would handle it.
        # Removing manual setting of a non-existent field:
        # db_job.updated_at = datetime.utcnow()
        db.commit()
        db.refresh(db_job)
    return db_job

def update_pipeline_log(db: Session, job_id: str, log_entry: Dict[str, Any]):
    db_job = get_ingestion_job(db, job_id)
    if db_job:
        current_log = list(db_job.pipeline_stages_executed_log or [])
        current_log.append(log_entry)
        db_job.pipeline_stages_executed_log = current_log
        # db.commit() # Commit can be handled by the calling function after several updates
        # db.refresh(db_job)
    else:
        # Log error: job not found
        print(f"Error in update_pipeline_log: Job {job_id} not found.")


def update_validation_summary(db: Session, job_id: str, summary_update: Dict[str, Any]):
    db_job = get_ingestion_job(db, job_id)
    if db_job:
        current_summary = dict(db_job.validation_summary or {})
        current_summary.update(summary_update) # Merge new summary data
        db_job.validation_summary = current_summary
        # db.commit()
        # db.refresh(db_job)
    else:
        print(f"Error in update_validation_summary: Job {job_id} not found.")

def update_error_logs(db: Session, job_id: str, error_message: str):
    db_job = get_ingestion_job(db, job_id)
    if db_job:
        current_errors = list(db_job.error_log_references or [])
        current_errors.append(error_message)
        db_job.error_log_references = current_errors
        # db.commit()
        # db.refresh(db_job)
    else:
        print(f"Error in update_error_logs: Job {job_id} not found.")


def remove_ingestion_job(db: Session, job_id: str) -> bool:
    db_job = get_ingestion_job(db, job_id)
    if db_job:
        db.delete(db_job)
        db.commit()
        return True
    return False

# CRUD operations for QueryHistoryDB

def create_query_history_item(db: Session, history_item_pydantic: QueryHistoryItemPydantic) -> models.QueryHistoryDB:
    """
    Creates a new query history record in the database.
    Relies on QueryHistoryItemPydantic for structure, including converting
    its 'request_data' (which is a QueryRequest Pydantic model) to a dict.
    """
    request_data_dict = {}
    if isinstance(history_item_pydantic, dict): # Fallback if Pydantic import failed
        request_data_dict = history_item_pydantic.get("request_data", {})
        user_id = history_item_pydantic.get("user_id", "unknown_user") # Should be provided
        query_id_val = history_item_pydantic.get("query_id") or str(uuid.uuid4())
        timestamp_val = history_item_pydantic.get("timestamp") or datetime.utcnow()
    else: # Assuming history_item_pydantic is the actual Pydantic model
        request_data_dict = history_item_pydantic.request_data.dict()
        user_id = history_item_pydantic.user_id
        query_id_val = history_item_pydantic.query_id
        timestamp_val = history_item_pydantic.timestamp

    db_history_item = models.QueryHistoryDB(
        query_id=query_id_val,
        timestamp=timestamp_val,
        user_id=user_id,
        request_data=request_data_dict
    )
    db.add(db_history_item)
    db.commit()
    db.refresh(db_history_item)
    return db_history_item

def get_query_history_item(db: Session, query_id: str) -> Optional[models.QueryHistoryDB]:
    """
    Retrieves a single query history record by its query_id.
    """
    return db.query(models.QueryHistoryDB).filter(models.QueryHistoryDB.query_id == query_id).first()

def get_query_history_for_user(db: Session, user_id: str, skip: int = 0, limit: int = 100) -> List[models.QueryHistoryDB]:
    """
    Retrieves a list of query history records for a specific user_id, with pagination.
    Orders by timestamp descending.
    """
    return db.query(models.QueryHistoryDB)\
             .filter(models.QueryHistoryDB.user_id == user_id)\
             .order_by(models.QueryHistoryDB.timestamp.desc())\
             .offset(skip)\
             .limit(limit)\
             .all()

def create_job_outbox_event(db: Session, event_data: Dict[str, Any]) -> models.JobOutbox:
    db_event = models.JobOutbox(
        job_id=event_data.get("job_id"),
        nats_subject=event_data.get("nats_subject"),
        nats_message_payload=event_data.get("nats_message_payload"),
        nats_message_id=event_data.get("nats_message_id"),
        # published_status defaults to 'UNPUBLISHED' in the model
        # created_at defaults to datetime.utcnow in the model
    )
    db.add(db_event)
    # The commit will be handled by the calling function or FastAPI's session management
    # to ensure it's part of the same transaction as other DB operations.
    # db.commit() # Do not commit here
    # db.refresh(db_event) # Refresh will also be handled after the commit
    return db_event

def get_unpublished_outbox_events(db: Session, limit: int = 100) -> List[models.JobOutbox]:
    """Fetches a list of unpublished outbox events, oldest first."""
    return db.query(models.JobOutbox)\
             .filter(models.JobOutbox.published_status == 'UNPUBLISHED')\
             .order_by(asc(models.JobOutbox.created_at))\
             .limit(limit)\
             .all()

def update_outbox_event_status(db: Session, event_id: int, new_status: str, published_at: Optional[datetime] = None) -> Optional[models.JobOutbox]:
    """Updates the status and published_at timestamp of an outbox event."""
    db_event = db.query(models.JobOutbox).filter(models.JobOutbox.event_id == event_id).first()
    if db_event:
        db_event.published_status = new_status
        if published_at:
            db_event.published_at = published_at
        # The commit will be handled by the calling function (message_relay.py)
        # after processing a batch of events.
        # db.commit() # No commit here
        # db.refresh(db_event) # No refresh here
        return db_event
    return None

def get_job_status(db: Session, job_id: str) -> Optional[str]:
    """Fetches the current processing_status of an ingestion job."""
    job = db.query(models.IngestionJobDB.processing_status)\
             .filter(models.IngestionJobDB.job_id == job_id)\
             .first()
    if job:
        return job.processing_status
    return None

# Add other CRUD functions as needed for other models (IndexMetadataDB, QueryHistoryDB) later.

# CRUD operations for IndexMetadataDB

def create_index_metadata(db: Session, metadata_pydantic: IndexMetadataPydantic) -> models.IndexMetadataDB:
    """
    Creates a new index metadata record in the database.
    Relies on the Pydantic model `IndexMetadataPydantic` to provide necessary fields,
    including those with default factories (like index_id, created_at) or simple defaults.
    """
    # Ensure metadata_pydantic is not the placeholder Dict if import failed and we need specific fields
    if isinstance(metadata_pydantic, dict):
        # This block is for the placeholder scenario where IndexMetadataPydantic import might fail.
        # In a properly structured app, IndexMetadataPydantic would be a Pydantic model.
        db_metadata_item = models.IndexMetadataDB(
            index_id=metadata_pydantic.get("index_id") or str(uuid.uuid4()), # Generate if not provided
            index_name=metadata_pydantic["index_name"], # Required
            index_type=metadata_pydantic["index_type"], # Required
            promoted_data_path=metadata_pydantic["promoted_data_path"], # Required
            version=metadata_pydantic.get("version", 1),
            status=metadata_pydantic.get("status", "creating"),
            created_at=metadata_pydantic.get("created_at") or datetime.utcnow(), # Generate if not provided
            updated_at=metadata_pydantic.get("updated_at"), # Can be None
            celery_task_id=metadata_pydantic.get("celery_task_id")
        )
    else: # Assumes metadata_pydantic is the actual Pydantic model instance
        db_metadata_item = models.IndexMetadataDB(
            index_id=metadata_pydantic.index_id, # Relies on Pydantic default_factory
            index_name=metadata_pydantic.index_name,
            index_type=metadata_pydantic.index_type,
            promoted_data_path=metadata_pydantic.promoted_data_path,
            version=metadata_pydantic.version, # Relies on Pydantic default
            status=metadata_pydantic.status,     # Relies on Pydantic default
            created_at=metadata_pydantic.created_at, # Relies on Pydantic default_factory
            updated_at=metadata_pydantic.updated_at, # Can be None initially
            celery_task_id=metadata_pydantic.celery_task_id
        )
    db.add(db_metadata_item)
    db.commit()
    db.refresh(db_metadata_item)
    return db_metadata_item

def get_index_metadata(db: Session, index_id: str) -> Optional[models.IndexMetadataDB]:
    """
    Retrieves a single index metadata record by its index_id.
    """
    return db.query(models.IndexMetadataDB).filter(models.IndexMetadataDB.index_id == index_id).first()

def get_all_index_metadata(db: Session, skip: int = 0, limit: int = 100) -> List[models.IndexMetadataDB]:
    """
    Retrieves a list of index metadata records, with pagination.
    Orders by creation date descending.
    """
    return db.query(models.IndexMetadataDB).order_by(models.IndexMetadataDB.created_at.desc()).offset(skip).limit(limit).all()

def update_index_metadata(db: Session, index_id: str, update_data: Dict[str, Any]) -> Optional[models.IndexMetadataDB]:
    """
    Updates an existing index metadata record.
    'updated_at' is handled automatically by the SQLAlchemy model's onupdate.
    """
    db_metadata = get_index_metadata(db, index_id)
    if db_metadata:
        for key, value in update_data.items():
            if hasattr(db_metadata, key):
                setattr(db_metadata, key, value)
            else:
                # Optionally log a warning for keys not found, or raise an error
                print(f"Warning: Field '{key}' not found in IndexMetadataDB model during update.")
        # db_metadata.updated_at = datetime.utcnow() # Not needed due to onupdate in model
        db.commit()
        db.refresh(db_metadata)
    return db_metadata

def delete_index_metadata(db: Session, index_id: str) -> bool:
    """
    Deletes an index metadata record by its index_id.
    Returns True if deletion was successful, False otherwise.
    """
    db_metadata = get_index_metadata(db, index_id)
    if db_metadata:
        db.delete(db_metadata)
        db.commit()
        return True
    return False