from sqlalchemy import Column, Integer, String, DateTime, JSON, Text, ForeignKey, Boolean
from sqlalchemy.orm import relationship
from backend.database import Base # Import Base from database.py
import uuid # For job_id default
from datetime import datetime

class IngestionJobDB(Base):
    __tablename__ = "ingestion_jobs"

    # Using String for UUIDs that are generated as strings.
    # For actual UUID type, some DBs need `sqlalchemy.dialects.postgresql.UUID` or similar.
    job_id = Column(String, primary_key=True, index=True, default=lambda: str(uuid.uuid4()))

    original_filename = Column(String, nullable=True)
    upload_timestamp = Column(DateTime, default=datetime.utcnow)
    user_id = Column(String, nullable=True, index=True) # Assuming user_id is a string
    source_type = Column(String, default="file_upload")

    detected_format = Column(String, nullable=True)
    original_crs = Column(Text, nullable=True) # WKT can be long
    target_crs = Column(String, nullable=True)

    processing_status = Column(String, index=True, default="pending_staging")

    staged_file_path = Column(String, nullable=True, unique=True) # Path should be unique if files are not deleted post-staging

    promotion_timestamp = Column(DateTime, nullable=True)
    quarantine_timestamp = Column(DateTime, nullable=True)

    location_of_promoted_data = Column(String, nullable=True, unique=True) # Path should be unique
    location_of_quarantined_data = Column(String, nullable=True, unique=True) # Path should be unique

    # For JSON fields, SQLAlchemy's JSON type is appropriate.
    # It handles JSON decoding/encoding. For SQLite, it's often stored as TEXT.
    pipeline_stages_executed_log = Column(JSON, nullable=True) # List of dicts
    validation_summary = Column(JSON, nullable=True) # Dict
    error_log_references = Column(JSON, nullable=True) # List of strings

    data_hash_original = Column(String, nullable=True)
    data_hash_processed = Column(String, nullable=True)

    celery_task_id = Column(String, nullable=True, index=True, unique=True) # Celery task ID should be unique

    geocoding_summary = Column(JSON, nullable=True) # Summary from geocoding stage
    complex_geometry_summary = Column(JSON, nullable=True) # Summary from complex geometry handling
    ner_summary = Column(JSON, nullable=True) # Summary from NER processing stage

    # Consider adding 'created_at' and 'updated_at' managed by SQLAlchemy/database
    # For example:
    # created_at = Column(DateTime, default=datetime.utcnow)
    # updated_at = Column(DateTime, default=datetime.utcnow, onupdate=datetime.utcnow)

    def __repr__(self):
        return f"<IngestionJobDB(job_id='{self.job_id}', filename='{self.original_filename}', status='{self.processing_status}')>"

# If you had other models, they would be defined here, e.g.:
# class IndexMetadataDB(Base):
#     __tablename__ = "spatial_indexes"
#     # ... columns ...

# class QueryHistoryDB(Base):
#     __tablename__ = "query_history"
#     # ... columns ...

# Note: To initialize these tables in your database, you will typically use Alembic migrations.
# Ensure that `Base.metadata.create_all(bind=engine)` in `database.py` is commented out
# or removed if you are exclusively using Alembic for schema management, to avoid conflicts.
