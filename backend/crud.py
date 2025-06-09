from sqlalchemy.orm import Session
from backend import models, database # Import SQLAlchemy model and Pydantic schema
from datetime import datetime
from typing import List, Dict, Any, Optional

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
        ner_summary=job_data.get('ner_summary') # Add new field
        # Add other fields as necessary, ensuring defaults or None if not provided
    )
    db.add(db_job)
    db.commit()
    db.refresh(db_job)
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

# Add other CRUD functions as needed for other models (IndexMetadataDB, QueryHistoryDB) later.
