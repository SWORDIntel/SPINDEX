from sqlalchemy.orm import Session
from sqlalchemy import asc, exc as sqlalchemy_exc # Add asc for ordering, exc for exceptions
from backend import models, database # Import SQLAlchemy model
from backend.exceptions import ResourceNotFoundException, DatabaseErrorException, DuplicateResourceException # Custom exceptions
from datetime import datetime
from typing import List, Dict, Any, Optional
import uuid # For index_id default if needed, though Pydantic model should provide it
import logging

logger = logging.getLogger(__name__)

# Attempt to import Pydantic model, handle potential circularity if this file grows
try:
    from backend.main import IndexMetadata as IndexMetadataPydantic
    from backend.main import QueryHistoryItem as QueryHistoryItemPydantic
    # QueryRequest is part of QueryHistoryItemPydantic, so direct import might not be needed here
    # from backend.main import QueryRequest as QueryRequestPydantic
except ImportError:
    IndexMetadataPydantic = Dict[str, Any]
    QueryHistoryItemPydantic = Dict[str, Any]

# CRUD operations for IngestionJobDB

def get_ingestion_job(db: Session, job_id: str) -> models.IngestionJobDB:
    db_job = db.query(models.IngestionJobDB).filter(models.IngestionJobDB.job_id == job_id).first()
    if not db_job:
        raise ResourceNotFoundException(resource_name="IngestionJob", resource_id=job_id)
    return db_job

def get_ingestion_jobs(db: Session, skip: int = 0, limit: int = 100) -> List[models.IngestionJobDB]:
    try:
        return db.query(models.IngestionJobDB).order_by(models.IngestionJobDB.upload_timestamp.desc()).offset(skip).limit(limit).all()
    except sqlalchemy_exc.SQLAlchemyError as e:
        logger.error(f"Database error while fetching ingestion jobs: {e}", exc_info=True)
        raise DatabaseErrorException("Error fetching ingestion jobs.", original_exception=e)


def get_job_by_hash(db: Session, file_hash: str) -> Optional[models.IngestionJobDB]:
    """
    Get ingestion job by file hash (for deduplication).
    Returns None if no job with this hash exists.
    """
    try:
        return db.query(models.IngestionJobDB).filter(
            models.IngestionJobDB.data_hash_original == file_hash
        ).first()
    except sqlalchemy_exc.SQLAlchemyError as e:
        logger.error(f"Database error while fetching job by hash: {e}", exc_info=True)
        raise DatabaseErrorException("Error fetching job by hash.", original_exception=e)


def create_ingestion_job(db: Session, job_data: Dict[str, Any]) -> models.IngestionJobDB:
    try:
        db_job = models.IngestionJobDB(
            job_id=job_data.get('job_id') or str(uuid.uuid4()),
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
        db.flush() # Use flush to assign IDs and check constraints before commit (if any are deferred)
        # db.commit() # Commit handled by endpoint
        # db.refresh(db_job) # Refresh handled by endpoint after commit
        return db_job
    except sqlalchemy_exc.IntegrityError as e:
        db.rollback()
        logger.error(f"Database integrity error creating ingestion job: {e}", exc_info=True)
        # Determine if it's a known unique constraint or other integrity issue
        # For example, if job_id was supposed to be unique and was provided:
        if "job_id" in str(e.orig).lower() and "unique constraint" in str(e.orig).lower(): # DB specific check
             raise DuplicateResourceException(resource_name="IngestionJob", conflicting_field="job_id", conflicting_value=job_data.get('job_id'))
        raise DatabaseErrorException("Error creating ingestion job due to data integrity issue.", original_exception=e)
    except sqlalchemy_exc.SQLAlchemyError as e:
        db.rollback()
        logger.error(f"Database error creating ingestion job: {e}", exc_info=True)
        raise DatabaseErrorException("Error creating ingestion job.", original_exception=e)


def update_ingestion_job_fields(db: Session, job_id: str, fields_to_update: Dict[str, Any]) -> models.IngestionJobDB:
    db_job = get_ingestion_job(db, job_id) # Will raise ResourceNotFound if not found
    try:
        for key, value in fields_to_update.items():
            if hasattr(db_job, key):
                setattr(db_job, key, value)
            else:
                logger.warning(f"Field {key} not found in IngestionJobDB model during update for job {job_id}.")
        db.commit()
        db.refresh(db_job)
        return db_job
    except sqlalchemy_exc.SQLAlchemyError as e:
        db.rollback()
        logger.error(f"Database error updating ingestion job {job_id}: {e}", exc_info=True)
        raise DatabaseErrorException(f"Error updating ingestion job {job_id}.", original_exception=e)


def update_pipeline_log(db: Session, job_id: str, log_entry: Dict[str, Any]):
    db_job = get_ingestion_job(db, job_id) # Raises ResourceNotFound
    try:
        current_log = list(db_job.pipeline_stages_executed_log or [])
        current_log.append(log_entry)
        db_job.pipeline_stages_executed_log = current_log
        # db.commit() # Let caller handle commit
    except Exception as e: # Catch broader exceptions if JSON manipulation fails
        logger.error(f"Error updating pipeline log for job {job_id}: {e}", exc_info=True)
        # Decide if this should raise a specific application error


def update_validation_summary(db: Session, job_id: str, summary_update: Dict[str, Any]):
    db_job = get_ingestion_job(db, job_id) # Raises ResourceNotFound
    try:
        current_summary = dict(db_job.validation_summary or {})
        current_summary.update(summary_update)
        db_job.validation_summary = current_summary
    except Exception as e:
        logger.error(f"Error updating validation summary for job {job_id}: {e}", exc_info=True)

def update_error_logs(db: Session, job_id: str, error_message: str):
    db_job = get_ingestion_job(db, job_id) # Raises ResourceNotFound
    try:
        current_errors = list(db_job.error_log_references or [])
        current_errors.append(error_message)
        db_job.error_log_references = current_errors
    except Exception as e:
        logger.error(f"Error updating error logs for job {job_id}: {e}", exc_info=True)


def remove_ingestion_job(db: Session, job_id: str):
    db_job = get_ingestion_job(db, job_id) # Raises ResourceNotFound
    try:
        db.delete(db_job)
        db.commit()
    except sqlalchemy_exc.SQLAlchemyError as e:
        db.rollback()
        logger.error(f"Database error removing ingestion job {job_id}: {e}", exc_info=True)
        raise DatabaseErrorException(f"Error removing ingestion job {job_id}.", original_exception=e)

# CRUD operations for QueryHistoryDB

def create_query_history_item(db: Session, history_item_pydantic: QueryHistoryItemPydantic) -> models.QueryHistoryDB:
    try:
        request_data_dict = {}
        if isinstance(history_item_pydantic, dict):
            request_data_dict = history_item_pydantic.get("request_data", {})
            user_id = history_item_pydantic.get("user_id", "unknown_user")
            query_id_val = history_item_pydantic.get("query_id") or str(uuid.uuid4())
            timestamp_val = history_item_pydantic.get("timestamp") or datetime.utcnow()
        else:
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
    except sqlalchemy_exc.IntegrityError as e:
        db.rollback()
        logger.error(f"Database integrity error creating query history item: {e}", exc_info=True)
        raise DatabaseErrorException("Error creating query history item due to data integrity.", original_exception=e)
    except sqlalchemy_exc.SQLAlchemyError as e:
        db.rollback()
        logger.error(f"Database error creating query history item: {e}", exc_info=True)
        raise DatabaseErrorException("Error creating query history item.", original_exception=e)

def get_query_history_item(db: Session, query_id: str) -> models.QueryHistoryDB:
    db_item = db.query(models.QueryHistoryDB).filter(models.QueryHistoryDB.query_id == query_id).first()
    if not db_item:
        raise ResourceNotFoundException(resource_name="QueryHistoryItem", resource_id=query_id)
    return db_item

def get_query_history_for_user(db: Session, user_id: str, skip: int = 0, limit: int = 100) -> List[models.QueryHistoryDB]:
    """
    Retrieves a list of query history records for a specific user_id, with pagination.
    Orders by timestamp descending.
    """
    try:
        return db.query(models.QueryHistoryDB)\
                 .filter(models.QueryHistoryDB.user_id == user_id)\
                 .order_by(models.QueryHistoryDB.timestamp.desc())\
                 .offset(skip)\
                 .limit(limit)\
                 .all()
    except sqlalchemy_exc.SQLAlchemyError as e:
        logger.error(f"Database error retrieving query history for user {user_id}: {e}", exc_info=True)
        raise DatabaseErrorException(f"Error retrieving query history for user {user_id}.", original_exception=e)

def create_job_outbox_event(db: Session, event_data: Dict[str, Any]) -> models.JobOutbox:
    try:
        db_event = models.JobOutbox(
            job_id=event_data.get("job_id"),
            nats_subject=event_data.get("nats_subject"),
            nats_message_payload=event_data.get("nats_message_payload"),
            nats_message_id=event_data.get("nats_message_id"),
        )
        db.add(db_event)
        db.flush() # Check for immediate errors before commit is handled by caller
        return db_event
    except sqlalchemy_exc.IntegrityError as e:
        db.rollback()
        logger.error(f"Database integrity error creating job outbox event: {e}", exc_info=True)
        # Consider if a more specific error like DuplicateResourceException is applicable
        raise DatabaseErrorException("Error creating job outbox event due to data integrity.", original_exception=e)
    except sqlalchemy_exc.SQLAlchemyError as e:
        db.rollback()
        logger.error(f"Database error creating job outbox event: {e}", exc_info=True)
        raise DatabaseErrorException("Error creating job outbox event.", original_exception=e)

def get_unpublished_outbox_events(db: Session, limit: int = 100) -> List[models.JobOutbox]:
    try:
        return db.query(models.JobOutbox)\
                 .filter(models.JobOutbox.published_status == 'UNPUBLISHED')\
                 .order_by(asc(models.JobOutbox.created_at))\
                 .limit(limit)\
                 .all()
    except sqlalchemy_exc.SQLAlchemyError as e:
        logger.error(f"Database error fetching unpublished outbox events: {e}", exc_info=True)
        raise DatabaseErrorException("Error fetching unpublished outbox events.", original_exception=e)

def update_outbox_event_status(db: Session, event_id: int, new_status: str, published_at: Optional[datetime] = None) -> models.JobOutbox:
    db_event = db.query(models.JobOutbox).filter(models.JobOutbox.event_id == event_id).first()
    if not db_event:
        raise ResourceNotFoundException(resource_name="JobOutboxEvent", resource_id=event_id)
    try:
        db_event.published_status = new_status
        if published_at:
            db_event.published_at = published_at
        # db.commit() # Commit handled by caller (message_relay.py)
        return db_event
    except sqlalchemy_exc.SQLAlchemyError as e: # Should not happen if only updating fields on existing obj without commit
        db.rollback() # Still good practice
        logger.error(f"Database error updating outbox event {event_id}: {e}", exc_info=True)
        raise DatabaseErrorException(f"Error updating outbox event {event_id}.", original_exception=e)


def get_job_status(db: Session, job_id: str) -> Optional[str]:
    # This function is simple and might not need extensive try-except if ResourceNotFound is acceptable from get_ingestion_job
    try:
        job = get_ingestion_job(db, job_id) # This will raise ResourceNotFound if job doesn't exist
        return job.processing_status
    except ResourceNotFoundException: # Propagate if that's desired behavior
        raise
    except sqlalchemy_exc.SQLAlchemyError as e:
        logger.error(f"Database error fetching job status for {job_id}: {e}", exc_info=True)
        raise DatabaseErrorException(f"Error fetching job status for {job_id}.", original_exception=e)


# CRUD operations for IndexMetadataDB

def create_index_metadata(db: Session, metadata_pydantic: IndexMetadataPydantic) -> models.IndexMetadataDB:
    try:
        if isinstance(metadata_pydantic, dict):
            db_metadata_item = models.IndexMetadataDB(
                index_id=metadata_pydantic.get("index_id") or str(uuid.uuid4()),
                index_name=metadata_pydantic["index_name"],
                index_type=metadata_pydantic["index_type"],
                promoted_data_path=metadata_pydantic["promoted_data_path"],
                version=metadata_pydantic.get("version", 1),
                status=metadata_pydantic.get("status", "creating"),
                created_at=metadata_pydantic.get("created_at") or datetime.utcnow(),
                updated_at=metadata_pydantic.get("updated_at"),
                celery_task_id=metadata_pydantic.get("celery_task_id")
            )
        else:
            db_metadata_item = models.IndexMetadataDB(
                index_id=metadata_pydantic.index_id,
                index_name=metadata_pydantic.index_name,
                index_type=metadata_pydantic.index_type,
                promoted_data_path=metadata_pydantic.promoted_data_path,
                version=metadata_pydantic.version,
                status=metadata_pydantic.status,
                created_at=metadata_pydantic.created_at,
                updated_at=metadata_pydantic.updated_at,
                celery_task_id=metadata_pydantic.celery_task_id
            )
        db.add(db_metadata_item)
        db.commit()
        db.refresh(db_metadata_item)
        return db_metadata_item
    except sqlalchemy_exc.IntegrityError as e:
        db.rollback()
        logger.error(f"Database integrity error creating index metadata for '{metadata_pydantic.index_name if not isinstance(metadata_pydantic, dict) else metadata_pydantic.get('index_name')}': {e}", exc_info=True)
        if "index_name" in str(e.orig).lower() and ("unique constraint" in str(e.orig).lower() or "duplicate key" in str(e.orig).lower()):
             raise DuplicateResourceException(resource_name="IndexMetadata", conflicting_field="index_name", conflicting_value=metadata_pydantic.index_name if not isinstance(metadata_pydantic, dict) else metadata_pydantic.get('index_name'))
        raise DatabaseErrorException("Error creating index metadata due to data integrity.", original_exception=e)
    except sqlalchemy_exc.SQLAlchemyError as e:
        db.rollback()
        logger.error(f"Database error creating index metadata: {e}", exc_info=True)
        raise DatabaseErrorException("Error creating index metadata.", original_exception=e)

def get_index_metadata(db: Session, index_id: str) -> models.IndexMetadataDB:
    db_metadata = db.query(models.IndexMetadataDB).filter(models.IndexMetadataDB.index_id == index_id).first()
    if not db_metadata:
        raise ResourceNotFoundException(resource_name="IndexMetadata", resource_id=index_id)
    return db_metadata

def get_all_index_metadata(db: Session, skip: int = 0, limit: int = 100) -> List[models.IndexMetadataDB]:
    try:
        return db.query(models.IndexMetadataDB).order_by(models.IndexMetadataDB.created_at.desc()).offset(skip).limit(limit).all()
    except sqlalchemy_exc.SQLAlchemyError as e:
        logger.error(f"Database error fetching all index metadata: {e}", exc_info=True)
        raise DatabaseErrorException("Error fetching all index metadata.", original_exception=e)

def update_index_metadata(db: Session, index_id: str, update_data: Dict[str, Any]) -> models.IndexMetadataDB:
    db_metadata = get_index_metadata(db, index_id) # Raises ResourceNotFound
    try:
        for key, value in update_data.items():
            if hasattr(db_metadata, key):
                setattr(db_metadata, key, value)
            else:
                logger.warning(f"Field '{key}' not found in IndexMetadataDB model during update for index {index_id}.")
        db.commit()
        db.refresh(db_metadata)
        return db_metadata
    except sqlalchemy_exc.IntegrityError as e: # Catch potential integrity errors on update (e.g. unique name change)
        db.rollback()
        logger.error(f"Database integrity error updating index metadata {index_id}: {e}", exc_info=True)
        if "index_name" in str(e.orig).lower() and ("unique constraint" in str(e.orig).lower() or "duplicate key" in str(e.orig).lower()):
             raise DuplicateResourceException(resource_name="IndexMetadata", conflicting_field="index_name", conflicting_value=update_data.get('index_name'))
        raise DatabaseErrorException(f"Error updating index metadata {index_id} due to data integrity.", original_exception=e)
    except sqlalchemy_exc.SQLAlchemyError as e:
        db.rollback()
        logger.error(f"Database error updating index metadata {index_id}: {e}", exc_info=True)
        raise DatabaseErrorException(f"Error updating index metadata {index_id}.", original_exception=e)

def delete_index_metadata(db: Session, index_id: str):
    db_metadata = get_index_metadata(db, index_id) # Raises ResourceNotFound
    try:
        db.delete(db_metadata)
        db.commit()
    except sqlalchemy_exc.SQLAlchemyError as e:
        db.rollback()
        logger.error(f"Database error deleting index metadata {index_id}: {e}", exc_info=True)
        raise DatabaseErrorException(f"Error deleting index metadata {index_id}.", original_exception=e)
