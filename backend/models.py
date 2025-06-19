from sqlalchemy import Column, Integer, String, DateTime, JSON, Text, ForeignKey, Boolean
from sqlalchemy.orm import relationship
from backend.database import Base # Import Base from database.py
import uuid # For job_id default
from datetime import datetime

class IngestionJobDB(Base):
    """
    SQLAlchemy ORM model for an ingestion job. This table tracks the state and
    metadata of each data file uploaded and processed by the AEGIS pipeline.
    """
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

    staged_file_path = Column(String, nullable=True, unique=True)
    location_of_promoted_data = Column(String, nullable=True, unique=True)
    location_of_quarantined_data = Column(String, nullable=True, unique=True)

    promotion_timestamp = Column(DateTime, nullable=True)
    quarantine_timestamp = Column(DateTime, nullable=True)

    # For JSON fields, SQLAlchemy's JSON type is appropriate.
    pipeline_stages_executed_log = Column(JSON, nullable=True) # List of dicts
    validation_summary = Column(JSON, nullable=True) # Dict
    error_log_references = Column(JSON, nullable=True) # List of strings

    data_hash_original = Column(String, nullable=True)
    data_hash_processed = Column(String, nullable=True)

    celery_task_id = Column(String, nullable=True, index=True, unique=True)

    # Summaries from specific pipeline stages
    geocoding_summary = Column(JSON, nullable=True)
    complex_geometry_summary = Column(JSON, nullable=True)
    ner_summary = Column(JSON, nullable=True)
    content_security_summary = Column(JSON, nullable=True) # This was the key missing field

    intermediate_geojson_path = Column(String, nullable=True, unique=True) # Path to intermediate GeoJSON


    def __repr__(self):
        return f"<IngestionJobDB(job_id='{self.job_id}', filename='{self.original_filename}', status='{self.processing_status}')>"

class IndexMetadataDB(Base):
    __tablename__ = "index_metadata"

    index_id = Column(String, primary_key=True, index=True, default=lambda: str(uuid.uuid4()))
    index_name = Column(String, index=True, nullable=False)
    index_type = Column(String, nullable=False) # e.g., "elasticsearch", "solr", "qdrant_vector"
    promoted_data_path = Column(String, nullable=False) # Path to the data that was indexed (e.g., from IngestionJobDB.location_of_promoted_data)
    version = Column(Integer, default=1, nullable=False) # For tracking updates or re-indexing of the same data
    status = Column(String, default="creating", index=True, nullable=False) # e.g., creating, active, inactive, failed, updating

    created_at = Column(DateTime, default=datetime.utcnow, nullable=False)
    updated_at = Column(DateTime, default=datetime.utcnow, onupdate=datetime.utcnow, nullable=True)

    celery_task_id = Column(String, nullable=True, index=True) # If index creation is a background task

    # Potentially add fields for connection details if not managed externally,
    # or specific metadata about the index schema or configuration.
    # Example: index_specific_config = Column(JSON, nullable=True)

    def __repr__(self):
        return f"<IndexMetadataDB(index_id='{self.index_id}', name='{self.index_name}', type='{self.index_type}', status='{self.status}')>"

class JobOutbox(Base):
    __tablename__ = "job_outbox"

    event_id = Column(Integer, primary_key=True, autoincrement=True) # Changed to Integer for typical auto-increment PK
    job_id = Column(String, ForeignKey("ingestion_jobs.job_id"), nullable=False, index=True)

    nats_subject = Column(String(255), nullable=False)
    nats_message_payload = Column(JSON, nullable=False)
    nats_message_id = Column(String(128), nullable=False, index=True) # For NATS deduplication

    published_status = Column(String(20), default='UNPUBLISHED', index=True, nullable=False) # e.g., UNPUBLISHED, PUBLISHED, FAILED_TO_PUBLISH

    created_at = Column(DateTime, default=datetime.utcnow)
    published_at = Column(DateTime, nullable=True)

    # Relationship (optional, but good for ORM use)
    # ingestion_job = relationship("IngestionJobDB") # If needed for ORM queries

    def __repr__(self):
        return f"<JobOutbox(event_id='{self.event_id}', job_id='{self.job_id}', status='{self.published_status}')>"

class JobModuleStatus(Base):
    __tablename__ = "job_module_status"

    module_status_id = Column(Integer, primary_key=True, autoincrement=True) # Changed to Integer
    job_id = Column(String, ForeignKey("ingestion_jobs.job_id"), nullable=False, index=True)

    module_name = Column(String(100), nullable=False)
    module_status = Column(String(50), nullable=False) # PENDING, RUNNING, COMPLETED_SUCCESS, COMPLETED_WARN, FAILED
    status_message = Column(Text, nullable=True)

    started_at = Column(DateTime, nullable=True)
    completed_at = Column(DateTime, nullable=True)

    input_data_path = Column(Text, nullable=True)
    output_data_path = Column(Text, nullable=True)

    created_at = Column(DateTime, default=datetime.utcnow) # Added for consistency

    # Relationship (optional)
    # ingestion_job = relationship("IngestionJobDB")

    def __repr__(self):
        return f"<JobModuleStatus(id='{self.module_status_id}', job_id='{self.job_id}', module='{self.module_name}', status='{self.module_status}')>"

# Note: To initialize these tables in your database, you will typically use Alembic migrations.
# Ensure that `Base.metadata.create_all(bind=engine)` in `database.py` is commented out
# or removed if you are exclusively using Alembic for schema management, to avoid conflicts.