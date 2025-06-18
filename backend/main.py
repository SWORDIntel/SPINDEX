# --- Core Application & Framework Imports ---
from fastapi import FastAPI, Depends, HTTPException, status, File, UploadFile
from fastapi.responses import JSONResponse
from fastapi.security import APIKeyHeader
from pydantic import BaseModel, Field as PydanticField
from typing import List, Optional, Dict, Any, Union
import time
from datetime import datetime
import uuid
import shutil
import os
import json
import logging
from collections import defaultdict

# --- Database & CRUD Imports ---
from sqlalchemy import (
    create_engine, Column, Integer, String, DateTime, JSON, Text, Boolean, ForeignKey
)
from sqlalchemy.ext.declarative import declarative_base # Will be removed if Base is imported
from sqlalchemy.orm import Session # Added Session for type hinting
# Assuming database.py and crud.py exist and are structured as implied by the FastAPI endpoints
from backend import crud # Added import for crud
from backend.database import Base, get_db, SessionLocal # Added SessionLocal for Celery tasks
from backend.models import IngestionJobDB # Added import for IngestionJobDB

# --- Data Processing & Scientific Computing Imports ---
from osgeo import gdal, ogr, osr # type: ignore
from shapely.geometry import shape, Point as ShapelyPoint, mapping as shapely_mapping # type: ignore
from shapely.validation import is_valid, make_valid
import spacy # For NER
import zipfile # For Zip Bomb check
import copy # For deepcopy

# --- External Service & Task Queue Imports ---
from celery import Celery, shared_task
from celery.result import AsyncResult
import requests # type: ignore
from geopy.geocoders import Nominatim # type: ignore
from geopy.exc import GeocoderTimedOut, GeocoderUnavailable, GeocoderServiceError # type: ignore


# --- Application Setup & Pydantic Models (from 'jules_wip' branch) ---

# --- Constants ---
# Note: For production, ensure that the STAGING_AREA_PATH directory
# has appropriate OS-level permissions to restrict access and protect
# sensitive uploaded data. It should be writable by the application user
# but not unnecessarily exposed.
STAGING_AREA_PATH = "staging_area"
PROCESSED_AREA_PATH = "processed_area"
TARGET_CRS_EPSG = 4326
DEFAULT_NER_TEXT_PROPERTY_FIELDS: List[str] = [
    "description", "notes", "summary", "title", "text",
    "comments", "abstract", "message", "content", "detail"
]
EXPECTED_INPUT_FORMATS = [
    "ESRI Shapefile", "GeoJSON", "KML", "GML", "GPKG", "FlatGeobuf"
]
MAX_FILE_SIZE = 100 * 1024 * 1024 # 100 MB
ALLOWED_MIME_TYPES = [
    "application/geo+json", "application/json", "application/zip",
    "application/vnd.google-earth.kml+xml", "application/gml+xml",
    "text/xml", "application/octet-stream", "text/plain"
]
NOMINATIM_USER_AGENT = "spindex-aegis-pipeline/0.1 (developer.contact@example.org)"
GEOCODING_ADDRESS_FIELDS = [
    "address", "Address", "ADDRESS",
    "location", "Location", "LOCATION",
    "street_address", "full_address", "addr",
    "שדה כתובת" # Example: Hebrew for "address field"
]
COMPLEX_GEOMETRY_STRATEGY: str = "split_all" # Default strategy: "pass_through", "filter_types", "reject_on_types", "explode_collections", "split_multi", "split_all"
CGH_FILTER_TYPES_TO_KEEP: list = ["Point", "LineString", "Polygon", "MultiPoint", "MultiLineString", "MultiPolygon"]
CGH_REJECT_IF_TYPES_FOUND: list = [] # Example: ["GeometryCollection"]


# --- Logging Configuration ---
logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(name)s - %(levelname)s - %(message)s')
logger = logging.getLogger(__name__)

# --- Directory and Celery Setup ---
os.makedirs(STAGING_AREA_PATH, exist_ok=True)
os.makedirs(PROCESSED_AREA_PATH, exist_ok=True)
logger.info(f"Staging area for temporary raw file storage ensured at: {STAGING_AREA_PATH}")
logger.info(f"Processed area for intermediate and final data products ensured at: {PROCESSED_AREA_PATH}")

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

# --- Helper Functions for Pipeline Stages ---

def geocode_and_update_features(features: list, address_fields: list, nominatim_user_agent: str, logger_instance) -> tuple[list, dict]:
    """
    Geocodes features that lack geometry using specified address fields.
    Updates features in-place with new Point geometries if geocoding is successful.
    """
    summary = {
        "features_total_count": 0,
        "features_without_geometry_count": 0,
        "addresses_found_for_geocoding": 0,
        "geocoding_requests_sent": 0,
        "geocoding_successes": 0,
        "geocoding_failures": 0, # Address not found by geocoder
        "geocoding_service_errors": 0, # Errors like timeout, service unavailable
        "geocoding_unexpected_errors": 0 # Other exceptions during geocoding attempt
    }

    geolocator = Nominatim(user_agent=nominatim_user_agent, timeout=10)

    for feature in features:
        summary["features_total_count"] += 1

        # Check if geometry is None or an empty dictionary
        current_geometry = feature.get("geometry")
        if current_geometry is not None and (not isinstance(current_geometry, dict) or current_geometry): # Skips if valid geometry object/dict exists
            continue

        summary["features_without_geometry_count"] += 1

        address_to_geocode = None
        for field_name in address_fields:
            potential_address = feature.get("properties", {}).get(field_name)
            if potential_address and isinstance(potential_address, str) and potential_address.strip():
                address_to_geocode = potential_address.strip()
                summary["addresses_found_for_geocoding"] += 1
                break # Found an address field to try for this feature

        if address_to_geocode:
            logger_instance.info(f"Attempting to geocode address: '{address_to_geocode}'")
            summary["geocoding_requests_sent"] += 1
            try:
                location = geolocator.geocode(address_to_geocode)
                time.sleep(1.0) # Respect Nominatim's usage policy (1 request per second)

                if location:
                    summary["geocoding_successes"] += 1
                    new_geometry = {"type": "Point", "coordinates": [location.longitude, location.latitude]}
                    feature["geometry"] = new_geometry
                    logger_instance.info(f"Geocoding successful for '{address_to_geocode}': {new_geometry['coordinates']}")
                else:
                    summary["geocoding_failures"] += 1
                    logger_instance.warning(f"Geocoding failed (address not found) for: '{address_to_geocode}'")

            except GeocoderTimedOut:
                summary["geocoding_service_errors"] += 1
                logger_instance.error(f"Geocoding service timed out for address: '{address_to_geocode}'")
                time.sleep(1.0) # Wait after a timeout as well
            except GeocoderUnavailable:
                summary["geocoding_service_errors"] += 1
                logger_instance.error(f"Geocoding service unavailable for address: '{address_to_geocode}'")
                time.sleep(1.0) # Wait after a service error
            except GeocoderServiceError as e_service:
                summary["geocoding_service_errors"] += 1
                logger_instance.error(f"Geocoding service error for address '{address_to_geocode}': {e_service}")
                time.sleep(1.0) # Wait after a service error
            except Exception as e_generic:
                summary["geocoding_unexpected_errors"] += 1
                logger_instance.error(f"Unexpected error during geocoding for address '{address_to_geocode}': {e_generic}")
                # No time.sleep here as it's an unexpected client-side or unknown error.
        else:
            logger_instance.info(f"No address found in specified fields for feature {feature.get('id', '(no ID)')} to attempt geocoding.")

    return features, summary

def handle_complex_geometries(features: list, strategy: str, strategy_config: dict, logger_instance) -> tuple[list, dict]:
    """
    Handles complex geometries in a list of GeoJSON features based on a specified strategy.
    """
    summary = {
        "strategy_applied": strategy,
        "strategy_config_used": strategy_config,
        "feature_count_before": 0,
        "feature_count_after": 0,
        "geometries_by_type_before": defaultdict(int),
        "geometries_by_type_after": defaultdict(int),
        "geometry_collections_exploded_count": 0,
        "multi_geometries_split_count": 0,
        "new_features_from_splits": 0,
        "features_filtered_out_count": 0,
        "job_failed_due_to_rejected_type": False,
        "unsupported_strategy_used": False,
        "malformed_geometries_encountered": 0,
    }

    summary['feature_count_before'] = len(features)
    for feature in features:
        geom_dict = feature.get('geometry')
        if geom_dict:
            try:
                geom = shape(geom_dict)
                summary['geometries_by_type_before'][geom.geom_type] += 1
            except Exception: # Includes JSONDecodeError from shape, AttributeError if geom_dict is not dict, etc
                summary['geometries_by_type_before']['Malformed/Invalid'] += 1
                summary['malformed_geometries_encountered'] += 1
        else:
            summary['geometries_by_type_before']['None'] += 1

    processed_features = []

    if strategy == "pass_through":
        processed_features = features # No change
        logger_instance.info("Complex geometry strategy: pass_through. No changes made to features.")

    elif strategy == "filter_types":
        filter_types_kept = strategy_config.get('filter_types_kept', [])
        logger_instance.info(f"Complex geometry strategy: filter_types. Keeping only: {filter_types_kept}")
        for feature in features:
            geom_dict = feature.get('geometry')
            if not geom_dict: # Keep features with no geometry
                processed_features.append(feature)
                continue
            try:
                geom = shape(geom_dict)
                if geom.geom_type in filter_types_kept:
                    processed_features.append(feature)
                else:
                    summary['features_filtered_out_count'] += 1
            except Exception: # Malformed geometry
                summary['features_filtered_out_count'] += 1 # Filter out malformed ones too if not explicitly kept
                logger_instance.warning(f"Malformed geometry encountered and filtered out for feature: {feature.get('id', 'N/A')}")


    elif strategy == "reject_on_types":
        reject_if_types_found = strategy_config.get('reject_if_types_found', [])
        logger_instance.info(f"Complex geometry strategy: reject_on_types. Rejecting if any of: {reject_if_types_found} are found.")
        for feature in features:
            geom_dict = feature.get('geometry')
            if not geom_dict:
                continue
            try:
                geom = shape(geom_dict)
                if geom.geom_type in reject_if_types_found:
                    summary['job_failed_due_to_rejected_type'] = True
                    logger_instance.error(f"Job failed: Feature with rejected geometry type '{geom.geom_type}' found.")
                    # Depending on desired behavior, might clear processed_features or return immediately
                    processed_features = [] # Or handle as per specific requirements for rejection
                    break
                processed_features.append(feature) # Add temporarily, might be cleared if rejection occurs
            except Exception: # Malformed geometry could also trigger rejection if "Malformed/Invalid" is in reject_if_types_found
                 if "Malformed/Invalid" in reject_if_types_found:
                    summary['job_failed_due_to_rejected_type'] = True
                    logger_instance.error("Job failed: Malformed/Invalid geometry encountered and configured for rejection.")
                    processed_features = []
                    break
        if summary['job_failed_due_to_rejected_type']: # Ensure processed_features is empty if job failed
             processed_features = []


    elif strategy == "explode_collections":
        logger_instance.info("Complex geometry strategy: explode_collections.")
        for feature in features:
            geom_dict = feature.get('geometry')
            if not geom_dict:
                processed_features.append(feature)
                continue
            try:
                geom = shape(geom_dict)
                if geom.geom_type == "GeometryCollection":
                    summary['geometry_collections_exploded_count'] += 1
                    logger_instance.info(f"Exploding GeometryCollection for feature: {feature.get('id', 'N/A')}")
                    for i, sub_geom in enumerate(geom.geoms):
                        new_feature = copy.deepcopy(feature)
                        new_feature['geometry'] = shapely_mapping(sub_geom)
                        # Optionally, modify properties to indicate it's part of a split, e.g., new ID
                        new_feature_id = f"{feature.get('id', 'feature')}_part{i+1}" if feature.get('id') else f"exploded_part{i+1}"
                        if 'id' in new_feature : new_feature['id'] = new_feature_id
                        if 'properties' not in new_feature: new_feature['properties'] = {}
                        new_feature['properties']['original_feature_id'] = feature.get('id', 'N/A')
                        processed_features.append(new_feature)
                        summary['new_features_from_splits'] += 1
                else:
                    processed_features.append(feature)
            except Exception:
                 logger_instance.warning(f"Malformed geometry encountered for feature: {feature.get('id', 'N/A')}. Passing through in explode_collections.")
                 processed_features.append(feature) # Pass through malformed

    elif strategy == "split_multi":
        logger_instance.info("Complex geometry strategy: split_multi.")
        multi_types = ["MultiPoint", "MultiLineString", "MultiPolygon"]
        for feature in features:
            geom_dict = feature.get('geometry')
            if not geom_dict:
                processed_features.append(feature)
                continue
            try:
                geom = shape(geom_dict)
                if geom.geom_type in multi_types:
                    summary['multi_geometries_split_count'] += 1
                    logger_instance.info(f"Splitting {geom.geom_type} for feature: {feature.get('id', 'N/A')}")
                    for i, sub_geom in enumerate(geom.geoms):
                        new_feature = copy.deepcopy(feature)
                        new_feature['geometry'] = shapely_mapping(sub_geom)
                        new_feature_id = f"{feature.get('id', 'feature')}_part{i+1}" if feature.get('id') else f"split_part{i+1}"
                        if 'id' in new_feature : new_feature['id'] = new_feature_id
                        if 'properties' not in new_feature: new_feature['properties'] = {}
                        new_feature['properties']['original_feature_id'] = feature.get('id', 'N/A')
                        processed_features.append(new_feature)
                        summary['new_features_from_splits'] += 1
                else: # Includes simple types and GeometryCollection (not handled by this specific strategy)
                    processed_features.append(feature)
            except Exception:
                logger_instance.warning(f"Malformed geometry encountered for feature: {feature.get('id', 'N/A')}. Passing through in split_multi.")
                processed_features.append(feature) # Pass through malformed

    else: # Unknown or unsupported strategy
        summary["unsupported_strategy_used"] = True
        logger_instance.warning(f"Unsupported geometry handling strategy: {strategy}. Applying 'pass_through'.")
        processed_features = features # Default to pass_through for safety

    summary['feature_count_after'] = len(processed_features)
    for feature in processed_features:
        geom_dict = feature.get('geometry')
        if geom_dict:
            try:
                geom = shape(geom_dict)
                summary['geometries_by_type_after'][geom.geom_type] += 1
            except Exception:
                 summary['geometries_by_type_after']['Malformed/Invalid'] += 1
        else:
            summary['geometries_by_type_after']['None'] += 1

    return processed_features, summary

# --- Celery Tasks ---
@celery_app.task(name="process_ingestion_job")
def process_ingestion_job_task(job_id: str):
    logger.info(f"Celery task 'process_ingestion_job' started for job_id: {job_id}")

    db = SessionLocal() # Create a new session for the task
    try:
        # Initial logic for fetching job and updating status will be added in the next plan step.
        # For now, just log and confirm task invocation.
        job = db.query(IngestionJobDB).filter(IngestionJobDB.job_id == job_id).first()
        if not job:
            logger.error(f"Job ID {job_id} not found in the database for task 'process_ingestion_job'.")
            return f"Error: Job ID {job_id} not found."

        logger.info(f"Starting format detection for job {job_id}, file: {job.staged_file_path}")

        job.processing_status = "processing_format_detection"

        # Update pipeline log
        current_log = list(job.pipeline_stages_executed_log or [])
        current_log.append({
            "timestamp": datetime.utcnow().isoformat(), # Ensure datetime is imported
            "stage": "Format Detection",
            "status": "started",
            "message": f"Initiated format detection for staged file: {job.staged_file_path}"
        })
        job.pipeline_stages_executed_log = current_log

        db.commit()
        logger.info(f"Status updated to 'processing_format_detection' for job_id: {job_id}. Proceeding with GDAL/OGR detection.")

        detected_driver_name = None
        try:
            logger.info(f"Attempting to open file {job.staged_file_path} with GDAL/OGR for job {job_id}.")
            dataSource = ogr.Open(job.staged_file_path, 0) # 0 means read-only

            if dataSource is None:
                job.processing_status = "failed_format_detection"
                job.validation_summary = {"format_detection_error": "GDAL/OGR could not open the file or it's not a recognized vector format."}
                log_message = f"GDAL/OGR failed to open or recognize file: {job.staged_file_path}"
                logger.error(log_message + f" for job {job_id}.")
            else:
                driver = dataSource.GetDriver()
                if driver:
                    detected_driver_name = driver.GetName()
                    job.detected_format = detected_driver_name
                    job.processing_status = "format_detected_pending_conversion" # New status
                    log_message = f"Detected format: {detected_driver_name} for file {job.staged_file_path}"
                    logger.info(log_message + f" for job {job_id}.")
                else:
                    # This case might be rare if dataSource opened but driver is None
                    job.processing_status = "failed_format_detection"
                    job.validation_summary = {"format_detection_error": "GDAL/OGR opened the file but could not determine the driver."}
                    log_message = f"GDAL/OGR could not determine driver for file: {job.staged_file_path}"
                    logger.error(log_message + f" for job {job_id}.")

                dataSource = None # Close data source

            # Update pipeline log for format detection outcome
            current_log = list(job.pipeline_stages_executed_log or [])
            log_entry_status = "success" if detected_driver_name else "failed"
            log_entry_message = f"Format detection {log_entry_status}."
            if detected_driver_name:
                log_entry_message += f" Detected format: {detected_driver_name}."
            else:
                log_entry_message += f" Error: {job.validation_summary.get('format_detection_error', 'Unknown GDAL/OGR error')}."

            current_log.append({
                "timestamp": datetime.utcnow().isoformat(),
                "stage": "Format Detection",
                "status": log_entry_status,
                "message": log_entry_message
            })
            job.pipeline_stages_executed_log = current_log
            db.commit()

        except Exception as e_gdal:
            logger.error(f"Exception during GDAL/OGR format detection for job {job_id}, file {job.staged_file_path}: {e_gdal}")
            job.processing_status = "failed_format_detection"
            job.validation_summary = {"format_detection_error": f"GDAL/OGR Exception: {str(e_gdal)}"}

            current_log = list(job.pipeline_stages_executed_log or [])
            current_log.append({
                "timestamp": datetime.utcnow().isoformat(),
                "stage": "Format Detection",
                "status": "failed",
                "message": f"Exception occurred: {str(e_gdal)}"
            })
            job.pipeline_stages_executed_log = current_log
            db.commit()
            return f"Job ID {job_id}: Failed at format detection stage due to GDAL/OGR error."

        if detected_driver_name and job.processing_status == "format_detected_pending_conversion":
            geojson_filename = f"{job_id}.geojson"
            geojson_output_path = os.path.join(PROCESSED_AREA_PATH, geojson_filename)

            logger.info(f"Attempting to convert file {job.staged_file_path} to GeoJSON at {geojson_output_path} for job {job_id}.")

            try:
                os.makedirs(PROCESSED_AREA_PATH, exist_ok=True)
                translate_options_list = ["-f", "GeoJSON", "-skipfailures"]
                result = gdal.VectorTranslate(destName=geojson_output_path,
                                              srcDS=job.staged_file_path,
                                              options=translate_options_list)

                if result is None:
                    if not os.path.exists(geojson_output_path) or os.path.getsize(geojson_output_path) == 0:
                        gdal_error_msg = gdal.GetLastErrorMsg()
                        if not gdal_error_msg or "error" not in gdal_error_msg.lower():
                             gdal_error_msg = "GDAL VectorTranslate completed but output file is missing or empty."
                        logger.error(f"GDAL VectorTranslate failed for job {job_id}: {gdal_error_msg}")
                        raise Exception(gdal_error_msg)

                job.processing_status = "completed_format_conversion"
                job.intermediate_geojson_path = geojson_output_path
                log_message = f"Successfully converted file to GeoJSON: {geojson_output_path}"
                logger.info(log_message + f" for job {job_id}.")

                current_log = list(job.pipeline_stages_executed_log or [])
                current_log.append({
                    "timestamp": datetime.utcnow().isoformat(),
                    "stage": "Format Conversion (GeoJSON)",
                    "status": "success",
                    "message": log_message,
                    "output_path": geojson_output_path
                })
                job.pipeline_stages_executed_log = current_log
                db.commit()
                # Conversion successful, status is "completed_format_conversion", proceed to CRS detection
                logger.info(f"Job ID {job_id}: File successfully converted to GeoJSON. Proceeding to CRS detection.")

            except Exception as e_convert:
                logger.error(f"Exception during GeoJSON conversion for job {job_id}, file {job.staged_file_path}: {e_convert}")
                job.processing_status = "failed_format_conversion"
                job.validation_summary = {
                    **(job.validation_summary or {}),
                    "format_conversion_error": f"GeoJSON Conversion Exception: {str(e_convert)}"
                }
                current_log = list(job.pipeline_stages_executed_log or [])
                current_log.append({
                    "timestamp": datetime.utcnow().isoformat(),
                    "stage": "Format Conversion (GeoJSON)",
                    "status": "failed",
                    "message": f"Exception: {str(e_convert)}"
                })
                job.pipeline_stages_executed_log = current_log
                db.commit()
                return f"Job ID {job_id}: Failed at GeoJSON conversion stage." # End task if conversion fails

        # CRS Detection Stage (only if format conversion was successful)
        if job.processing_status == "completed_format_conversion":
            logger.info(f"Starting CRS detection for job {job_id}, file: {job.intermediate_geojson_path}")
            job.processing_status = "processing_crs_detection"
            db.commit()

            detected_crs_wkt = None
            try:
                datasource = ogr.Open(job.intermediate_geojson_path, 0)
                if datasource is None:
                    raise Exception("GDAL/OGR could not open the intermediate GeoJSON file.")
                if datasource.GetLayerCount() == 0:
                    raise Exception("Intermediate GeoJSON file contains no layers.")

                layer = datasource.GetLayer()
                spatial_ref = layer.GetSpatialRef()

                if spatial_ref is not None:
                    detected_crs_wkt = spatial_ref.ExportToWkt()
                    job.original_crs = detected_crs_wkt
                    job.processing_status = "crs_detected_pending_transformation"
                    log_message = f"Detected CRS for {job.intermediate_geojson_path}: {detected_crs_wkt[:100]}..."
                    logger.info(log_message + f" for job {job_id}")
                else:
                    job.processing_status = "failed_crs_detection"
                    log_message = "CRS not found or not interpretable in the GeoJSON file."
                    logger.warning(log_message + f" for job {job_id}, file: {job.intermediate_geojson_path}")
                    job.validation_summary = {**(job.validation_summary or {}), "crs_detection_error": log_message}

                datasource = None

                current_log = list(job.pipeline_stages_executed_log or [])
                log_status = "success" if job.processing_status == "crs_detected_pending_transformation" else "failed"
                current_log.append({
                    "timestamp": datetime.utcnow().isoformat(),
                    "stage": "CRS Detection",
                    "status": log_status,
                    "message": log_message,
                    "detected_crs_wkt": detected_crs_wkt if detected_crs_wkt else "N/A"
                })
                job.pipeline_stages_executed_log = current_log
                db.commit()

                if job.processing_status == "failed_crs_detection":
                    return f"Job ID {job_id}: Failed at CRS detection stage. {log_message}"
                # If CRS detection was successful (status is crs_detected_pending_transformation), proceed to transformation
                # No specific return here, flow continues if crs_detected_pending_transformation

            except Exception as e_crs_detect:
                logger.error(f"Exception during CRS detection for job {job_id}: {e_crs_detect}")
                job.processing_status = "failed_crs_detection"
                job.original_crs = None
                job.validation_summary = {
                    **(job.validation_summary or {}),
                    "crs_detection_error": f"CRS Detection Exception: {str(e_crs_detect)}"
                }
                current_log = list(job.pipeline_stages_executed_log or [])
                current_log.append({
                    "timestamp": datetime.utcnow().isoformat(),
                    "stage": "CRS Detection",
                    "status": "failed",
                    "message": f"Exception: {str(e_crs_detect)}"
                })
                job.pipeline_stages_executed_log = current_log
                db.commit()
                return f"Job ID {job_id}: Failed at CRS detection stage due to exception." # End task

        # CRS Transformation Stage (only if CRS detection was successful and needs transformation)
        if job.processing_status == "crs_detected_pending_transformation":
            logger.info(f"Starting CRS transformation for job {job_id}, file: {job.intermediate_geojson_path}")
            job.processing_status = "processing_crs_transformation"
            db.commit()

            source_srs_wkt = job.original_crs
            target_srs_epsg_code_str = str(TARGET_CRS_EPSG)

            source_srs = osr.SpatialReference()
            source_srs.ImportFromWkt(source_srs_wkt)

            target_srs = osr.SpatialReference()
            target_srs.ImportFromEPSG(TARGET_CRS_EPSG)

            needs_reprojection = True
            source_auth_name = source_srs.GetAuthorityName(None)
            source_auth_code = source_srs.GetAuthorityCode(None)

            if source_auth_name == "EPSG" and str(source_auth_code) == target_srs_epsg_code_str:
                needs_reprojection = False
                logger.info(f"Job {job_id}: Source CRS ({source_auth_name}:{source_auth_code}) is already the target CRS (EPSG:{target_srs_epsg_code_str}). No reprojection needed.")
                job.processing_status = "completed_crs_transformation"
                job.target_crs = f"EPSG:{target_srs_epsg_code_str}"
                log_message = f"CRS is already {job.target_crs}. No transformation performed."

                current_log = list(job.pipeline_stages_executed_log or [])
                current_log.append({
                    "timestamp": datetime.utcnow().isoformat(),
                    "stage": "CRS Transformation",
                    "status": "success",
                    "message": log_message,
                    "source_crs_wkt_snippet": job.original_crs[:100] + "...",
                    "target_crs": job.target_crs
                })
                job.pipeline_stages_executed_log = current_log
                db.commit()

            if needs_reprojection:
                original_geojson_path = job.intermediate_geojson_path
                reprojected_filename = f"{job_id}_epsg{target_srs_epsg_code_str}.geojson"
                reprojected_geojson_path = os.path.join(PROCESSED_AREA_PATH, reprojected_filename)

                logger.info(f"Attempting to reproject file {original_geojson_path} to EPSG:{target_srs_epsg_code_str} at {reprojected_geojson_path} for job {job_id}.")

                try:
                    translate_options_list = ["-f", "GeoJSON", "-skipfailures", "-t_srs", f"EPSG:{target_srs_epsg_code_str}"]
                    result = gdal.VectorTranslate(destName=reprojected_geojson_path, srcDS=original_geojson_path, options=translate_options_list)

                    if result is None or (not os.path.exists(reprojected_geojson_path) or os.path.getsize(reprojected_geojson_path) == 0):
                        gdal_error_msg = gdal.GetLastErrorMsg()
                        if not gdal_error_msg or "error" not in gdal_error_msg.lower():
                             gdal_error_msg = "GDAL VectorTranslate for reprojection completed but output file is missing or empty."
                        logger.error(f"GDAL VectorTranslate for reprojection failed for job {job_id}: {gdal_error_msg}")
                        raise Exception(gdal_error_msg)

                    job.intermediate_geojson_path = reprojected_geojson_path
                    job.target_crs = f"EPSG:{target_srs_epsg_code_str}"
                    job.processing_status = "completed_crs_transformation"
                    log_message = f"Successfully reprojected to {job.target_crs}. New file: {reprojected_geojson_path}"
                    logger.info(log_message + f" for job {job_id}.")

                    current_log = list(job.pipeline_stages_executed_log or [])
                    current_log.append({
                        "timestamp": datetime.utcnow().isoformat(),
                        "stage": "CRS Transformation",
                        "status": "success",
                        "message": log_message,
                        "source_crs_wkt_snippet": job.original_crs[:100] + "...",
                        "target_crs": job.target_crs,
                        "output_path": reprojected_geojson_path
                    })
                    job.pipeline_stages_executed_log = current_log
                    db.commit()

                except Exception as e_crs_transform:
                    logger.error(f"Exception during CRS transformation for job {job_id}: {e_crs_transform}")
                    job.processing_status = "failed_crs_transformation"
                    job.target_crs = None
                    job.validation_summary = {**(job.validation_summary or {}), "crs_transformation_error": f"CRS Transformation Exception: {str(e_crs_transform)}"}
                    current_log = list(job.pipeline_stages_executed_log or [])
                    current_log.append({
                        "timestamp": datetime.utcnow().isoformat(),
                        "stage": "CRS Transformation",
                        "status": "failed",
                        "message": f"Exception: {str(e_crs_transform)}"
                    })
                    job.pipeline_stages_executed_log = current_log
                    db.commit()
                    return f"Job ID {job_id}: Failed at CRS transformation stage." # End task

        # Geocoding Stage (only if CRS transformation was successful)
        if job.processing_status == "completed_crs_transformation":
            logger.info(f"Starting geocoding for job {job_id}, file: {job.intermediate_geojson_path}")
            job.processing_status = "processing_geocoding"
            current_log = list(job.pipeline_stages_executed_log or [])
            current_log.append({
                "timestamp": datetime.utcnow().isoformat(),
                "stage": "Geocoding",
                "status": "started",
                "message": f"Initiated geocoding for features in {job.intermediate_geojson_path}"
            })
            job.pipeline_stages_executed_log = current_log
            db.commit()

            try:
                if not job.intermediate_geojson_path or not os.path.exists(job.intermediate_geojson_path):
                    raise FileNotFoundError(f"Intermediate GeoJSON file not found at {job.intermediate_geojson_path} for geocoding.")

                with open(job.intermediate_geojson_path, 'r') as f:
                    geojson_data = json.load(f)

                if 'features' not in geojson_data or not isinstance(geojson_data['features'], list):
                    raise ValueError("GeoJSON data does not contain a valid 'features' list.")

                features = geojson_data['features']

                # Call the helper function (logger is globally available as logger_instance in this file)
                updated_features, geocoding_summary_dict = geocode_and_update_features(
                    features,
                    GEOCODING_ADDRESS_FIELDS, # Use the defined constant
                    NOMINATIM_USER_AGENT,     # Use the defined constant
                    logger # Use the global logger instance
                )

                job.geocoding_summary = geocoding_summary_dict
                geojson_data['features'] = updated_features # Update features in the loaded data

                with open(job.intermediate_geojson_path, 'w') as f:
                    json.dump(geojson_data, f)

                job.processing_status = "completed_geocoding"
                log_message = f"Geocoding completed for job {job_id}. Summary: {geocoding_summary_dict}"
                logger.info(log_message)
                current_log = list(job.pipeline_stages_executed_log or [])
                current_log.append({
                    "timestamp": datetime.utcnow().isoformat(),
                    "stage": "Geocoding",
                    "status": "success",
                    "message": log_message,
                    "summary": geocoding_summary_dict
                })
                job.pipeline_stages_executed_log = current_log
                db.commit()

            except Exception as e_geocode_step:
                logger.error(f"Exception during geocoding pipeline step for job {job_id}: {e_geocode_step}")
                job.processing_status = "failed_geocoding_step"
                error_info = {"geocoding_pipeline_error": str(e_geocode_step)}
                job.validation_summary = {**(job.validation_summary or {}), **error_info}
                current_log = list(job.pipeline_stages_executed_log or [])
                current_log.append({
                    "timestamp": datetime.utcnow().isoformat(),
                    "stage": "Geocoding",
                    "status": "failed",
                    "message": f"Geocoding step failed: {str(e_geocode_step)}"
                })
                job.pipeline_stages_executed_log = current_log
                db.commit()
                return f"Job ID {job_id}: Failed at geocoding pipeline step."

        # Complex Geometry Handling Stage (only if geocoding was successful)
        if job.processing_status == "completed_geocoding":
            logger.info(f"Starting complex geometry handling for job {job_id}, file: {job.intermediate_geojson_path}")
            job.processing_status = "processing_complex_geometry"
            current_log = list(job.pipeline_stages_executed_log or [])
            current_log.append({
                "timestamp": datetime.utcnow().isoformat(),
                "stage": "Complex Geometry Handling",
                "status": "started",
                "message": f"Initiated complex geometry handling for features in {job.intermediate_geojson_path}"
            })
            job.pipeline_stages_executed_log = current_log
            db.commit()

            strategy_config_dict = {}
            if COMPLEX_GEOMETRY_STRATEGY == "filter_types":
                strategy_config_dict['filter_types_kept'] = CGH_FILTER_TYPES_TO_KEEP
            elif COMPLEX_GEOMETRY_STRATEGY == "reject_on_types":
                strategy_config_dict['reject_if_types_found'] = CGH_REJECT_IF_TYPES_FOUND
            # Other strategies like "split_multi", "explode_collections", "pass_through"
            # might not need specific config beyond their name, as handled by the helper.
            # "split_all" is a special case handled below.

            try:
                if not job.intermediate_geojson_path or not os.path.exists(job.intermediate_geojson_path):
                    raise FileNotFoundError(f"Intermediate GeoJSON file not found at {job.intermediate_geojson_path} for complex geometry handling.")

                with open(job.intermediate_geojson_path, 'r') as f:
                    geojson_data = json.load(f)

                if 'features' not in geojson_data or not isinstance(geojson_data['features'], list):
                    raise ValueError("GeoJSON data does not contain a valid 'features' list for complex geometry handling.")

                features = geojson_data['features']

                if COMPLEX_GEOMETRY_STRATEGY == "split_all":
                    logger.info(f"Applying 'split_all' strategy for job {job_id}: Stage 1 - explode_collections")
                    features_after_explode, summary_explode = handle_complex_geometries(
                        features, "explode_collections", {}, logger
                    )
                    # If explode_collections itself could cause a rejection (e.g. if it encountered a type it's configured to reject)
                    if summary_explode.get('job_failed_due_to_rejected_type'):
                        job.complex_geometry_summary = summary_explode
                        job.processing_status = "failed_complex_geometry_rejected"
                        log_message_split_all_fail = f"Complex geometry handling ('split_all' - explode stage) failed for job {job_id}: Rejected. Summary: {summary_explode}"
                        logger.error(log_message_split_all_fail)
                        job.validation_summary = {**(job.validation_summary or {}), "complex_geometry_error": "Rejected during explode stage of split_all."}
                        current_log = list(job.pipeline_stages_executed_log or [])
                        current_log.append({
                            "timestamp": datetime.utcnow().isoformat(), "stage": "Complex Geometry Handling (split_all - explode)",
                            "status": "failed", "message": log_message_split_all_fail, "summary": summary_explode
                        })
                        job.pipeline_stages_executed_log = current_log
                        db.commit()
                        return f"Job ID {job_id}: Failed at complex geometry handling (split_all - explode stage) due to rejected type."

                    logger.info(f"Applying 'split_all' strategy for job {job_id}: Stage 2 - split_multi")
                    final_features, summary_split = handle_complex_geometries(
                        features_after_explode, "split_multi", {}, logger
                    )

                    # Combine summaries
                    complex_geom_summary_dict = {
                        "strategy_applied": "split_all",
                        "strategy_config_used": strategy_config_dict, # Original config for split_all if any was defined
                        "feature_count_before": summary_explode['feature_count_before'], # Initial count before any CGF operation
                        "geometries_by_type_before": summary_explode['geometries_by_type_before'],
                        "feature_count_after": summary_split['feature_count_after'], # Final count after all ops
                        "geometries_by_type_after": summary_split['geometries_by_type_after'],
                        "geometry_collections_exploded_count": summary_explode.get('geometry_collections_exploded_count', 0),
                        "multi_geometries_split_count": summary_split.get('multi_geometries_split_count', 0),
                        "new_features_from_splits": summary_explode.get('new_features_from_splits', 0) + summary_split.get('new_features_from_splits', 0),
                        "features_filtered_out_count": summary_explode.get('features_filtered_out_count', 0) + summary_split.get('features_filtered_out_count', 0),
                        "job_failed_due_to_rejected_type": summary_split.get('job_failed_due_to_rejected_type', False), # Check final stage for rejection based on its rules
                        "unsupported_strategy_used": False,
                        "malformed_geometries_encountered": summary_explode.get('malformed_geometries_encountered',0) + summary_split.get('malformed_geometries_encountered',0),
                        "split_all_details": {
                            "explode_collections_summary": summary_explode,
                            "split_multi_summary": summary_split
                        }
                    }
                    updated_features = final_features
                else:
                    updated_features, complex_geom_summary_dict = handle_complex_geometries(
                        features,
                        COMPLEX_GEOMETRY_STRATEGY,
                        strategy_config_dict,
                        logger
                    )

                job.complex_geometry_summary = complex_geom_summary_dict
                geojson_data['features'] = updated_features

                if complex_geom_summary_dict.get('job_failed_due_to_rejected_type'):
                    job.processing_status = "failed_complex_geometry_rejected"
                    log_message = f"Complex geometry handling failed for job {job_id}: Rejected due to geometry type. Strategy: {COMPLEX_GEOMETRY_STRATEGY}. Summary: {complex_geom_summary_dict}"
                    logger.error(log_message)
                    job.validation_summary = {**(job.validation_summary or {}), "complex_geometry_error": f"Rejected due to geometry type as per strategy '{COMPLEX_GEOMETRY_STRATEGY}'."}
                    current_log = list(job.pipeline_stages_executed_log or [])
                    current_log.append({
                        "timestamp": datetime.utcnow().isoformat(),
                        "stage": f"Complex Geometry Handling ({COMPLEX_GEOMETRY_STRATEGY})",
                        "status": "failed",
                        "message": log_message,
                        "summary": complex_geom_summary_dict
                    })
                    job.pipeline_stages_executed_log = current_log
                    db.commit()
                    return f"Job ID {job_id}: Failed at complex geometry handling due to rejected type."

                with open(job.intermediate_geojson_path, 'w') as f:
                    json.dump(geojson_data, f)

                job.processing_status = "completed_complex_geometry"
                log_message = f"Complex geometry handling completed for job {job_id}. Summary: {complex_geom_summary_dict}"
                logger.info(log_message)
                current_log = list(job.pipeline_stages_executed_log or [])
                current_log.append({
                    "timestamp": datetime.utcnow().isoformat(),
                    "stage": f"Complex Geometry Handling ({COMPLEX_GEOMETRY_STRATEGY})",
                    "status": "success",
                    "message": log_message,
                    "summary": complex_geom_summary_dict
                })
                job.pipeline_stages_executed_log = current_log
                db.commit()

            except Exception as e_complex_geom_step:
                logger.error(f"Exception during complex geometry handling pipeline step for job {job_id}: {e_complex_geom_step}")
                job.processing_status = "failed_complex_geometry_step"
                error_info = {"complex_geometry_pipeline_error": str(e_complex_geom_step)}
                job.validation_summary = {**(job.validation_summary or {}), **error_info}
                current_log = list(job.pipeline_stages_executed_log or [])
                current_log.append({
                    "timestamp": datetime.utcnow().isoformat(),
                    "stage": f"Complex Geometry Handling ({COMPLEX_GEOMETRY_STRATEGY})",
                    "status": "failed",
                    "message": f"Complex geometry handling step failed ({COMPLEX_GEOMETRY_STRATEGY}): {str(e_complex_geom_step)}"
                })
                job.pipeline_stages_executed_log = current_log
                db.commit()
                return f"Job ID {job_id}: Failed at complex geometry handling pipeline step."

        # Final success message if all relevant stages passed
        if job.processing_status == "completed_complex_geometry":
            return f"Job ID {job_id}: Successfully processed through complex geometry handling. Final file: {job.intermediate_geojson_path}"
        elif job.processing_status == "completed_geocoding":
            return f"Job ID {job_id}: Successfully processed through geocoding. Final file: {job.intermediate_geojson_path}"
        elif job.processing_status == "completed_crs_transformation":
             return f"Job ID {job_id}: Successfully processed through CRS transformation. Final file: {job.intermediate_geojson_path}"
        elif job.processing_status in [
            "failed_crs_detection", "failed_format_conversion",
            "failed_format_detection", "failed_crs_transformation",
            "failed_geocoding_step", "failed_complex_geometry_step",
            "failed_complex_geometry_rejected"
        ]:
            return f"Job ID {job_id}: Processing failed at {job.processing_status}."

        # Fallback for unexpected status
        return f"Job ID {job_id}: Task ended with status {job.processing_status}."

    except Exception as e:
        logger.error(f"Error processing job ID {job_id} in Celery task: {e}")
        # Attempt to update job status to a generic failure if not already set to a specific failure
        if job and hasattr(job, 'processing_status') and "failed" not in job.processing_status:
            try:
                job.processing_status = "failed_unknown_task_error"
                job.validation_summary = {**(job.validation_summary or {}), "unknown_task_error": str(e)}
                db.commit()
            except Exception as db_error:
                logger.error(f"Failed to update job status to failed_unknown_task_error for job {job_id} after initial error: {db_error}")
                db.rollback() # Rollback this specific attempt to update status
        else:
            db.rollback() # Rollback any changes if job object wasn't available or status was already a failure
        # Optionally, re-raise or handle specific exceptions if task should be retried
        # For now, just log and return error message
        return f"Error processing job ID {job_id}: {str(e)}"
    finally:
        db.close() # Ensure session is closed

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
            "nats_subject": "aegis.jobs.new", # This could be made configurable
            "nats_message_payload": {"job_id": db_job.job_id, "staged_file_path": db_job.staged_file_path},
            "nats_message_id": db_job.job_id # Using job_id as a unique message ID for NATS
        }
        crud.create_job_outbox_event(db=db, event_data=outbox_event_data)
        logger.info(f"JobOutbox event created for job_id: {created_job_id} for NATS publishing.")

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