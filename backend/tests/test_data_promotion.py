import os
import shutil
import uuid
import pytest
from datetime import datetime
from sqlalchemy.orm import Session
from backend.pipeline_logic import _execute_data_promotion, calculate_sha256_checksum
from backend.models import IngestionJobDB, JobModuleStatus
from backend.database import SessionLocal, engine, Base

# NOTE: These tests assume that the `promoted_data_hash` column has been added to the `ingestion_jobs` table.
# The migration for this could not be run due to environment issues.

@pytest.fixture(scope="function")
def db_session():
    """Create a new database session for each test function."""
    Base.metadata.create_all(bind=engine)
    db = SessionLocal()
    try:
        yield db
    finally:
        db.close()
        Base.metadata.drop_all(bind=engine)

@pytest.fixture(scope="function")
def sample_job(db_session: Session):
    """Create a sample ingestion job for testing."""
    job_id = str(uuid.uuid4())
    job = IngestionJobDB(
        job_id=job_id,
        original_filename="test.txt",
        content_type="text/plain",
        upload_timestamp=datetime.utcnow(),
        user_id="test_user",
        source_type="file_upload",
        processing_status="completed_complex_geometry",
        staged_file_path=f"/tmp/{job_id}.txt",
        intermediate_geojson_path=f"/tmp/{job_id}.geojson",
    )
    db_session.add(job)
    db_session.commit()
    db_session.refresh(job)

    # Create a dummy intermediate file
    os.makedirs(os.path.dirname(job.intermediate_geojson_path), exist_ok=True)
    with open(job.intermediate_geojson_path, "w") as f:
        f.write("test data")

    yield job

    # Clean up the dummy file
    if os.path.exists(job.intermediate_geojson_path):
        os.remove(job.intermediate_geojson_path)

def test_successful_data_promotion(db_session: Session, sample_job: IngestionJobDB):
    """Test the successful promotion of a file."""

    # Run the data promotion logic
    _execute_data_promotion(sample_job, db_session, logging.getLogger())

    # Verify the job status
    assert sample_job.processing_status == "PROMOTED"
    assert sample_job.promotion_timestamp is not None
    assert sample_job.location_of_promoted_data is not None
    assert sample_job.promoted_data_hash is not None

    # Verify the promoted file
    assert os.path.exists(sample_job.location_of_promoted_data)
    with open(sample_job.location_of_promoted_data, "r") as f:
        assert f.read() == "test data"

    # Verify the checksum
    assert sample_job.promoted_data_hash == calculate_sha256_checksum(sample_job.location_of_promoted_data)

    # Verify the module status
    module_status = db_session.query(JobModuleStatus).filter_by(job_id=sample_job.job_id, module_name="DataPromotion").first()
    assert module_status is not None
    assert module_status.module_status == "COMPLETED_SUCCESS"

    # Clean up the promoted file
    os.remove(sample_job.location_of_promoted_data)

def test_checksum_mismatch(db_session: Session, sample_job: IngestionJobDB):
    """Test the case where the checksums of the source and destination files do not match."""

    # Monkeypatch the shutil.copy2 function to create a corrupted file
    original_copy2 = shutil.copy2
    def corrupted_copy2(src, dst):
        original_copy2(src, dst)
        with open(dst, "w") as f:
            f.write("corrupted data")
    shutil.copy2 = corrupted_copy2

    # Run the data promotion logic
    _execute_data_promotion(sample_job, db_session, logging.getLogger())

    # Verify the job status
    assert sample_job.processing_status == "FAILED_PROMOTION"
    assert sample_job.promotion_timestamp is None
    assert sample_job.location_of_promoted_data is None
    assert sample_job.promoted_data_hash is None

    # Verify that the destination file was cleaned up
    assert not os.path.exists(f"trusted_data_area/{sample_job.original_filename.replace('.txt', '_promoted.geojson')}")

    # Verify the module status
    module_status = db_session.query(JobModuleStatus).filter_by(job_id=sample_job.job_id, module_name="DataPromotion").first()
    assert module_status is not None
    assert module_status.module_status == "FAILED"

    # Restore the original shutil.copy2 function
    shutil.copy2 = original_copy2

def test_source_file_not_found(db_session: Session, sample_job: IngestionJobDB):
    """Test the case where the source file is not found."""

    # Remove the intermediate file
    os.remove(sample_job.intermediate_geojson_path)

    # Run the data promotion logic
    _execute_data_promotion(sample_job, db_session, logging.getLogger())

    # Verify the job status
    assert sample_job.processing_status == "FAILED_PROMOTION"
    assert sample_job.promotion_timestamp is None
    assert sample_job.location_of_promoted_data is None
    assert sample_job.promoted_data_hash is None

    # Verify the module status
    module_status = db_session.query(JobModuleStatus).filter_by(job_id=sample_job.job_id, module_name="DataPromotion").first()
    assert module_status is not None
    assert module_status.module_status == "FAILED"
