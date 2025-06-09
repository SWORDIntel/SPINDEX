from fastapi import FastAPI, Depends, HTTPException, status, File, UploadFile
from fastapi.security import OAuth2PasswordBearer, APIKeyHeader
from pydantic import BaseModel, Field as PydanticField
from typing import List, Optional, Dict, Any
import time
from datetime import datetime
from celery import Celery, shared_task
from celery.result import AsyncResult
import shutil
import os
import uuid
from osgeo import ogr, osr # type: ignore
import json
from shapely.geometry import shape, Point as ShapelyPoint # type: ignore
from shapely.validation import is_valid, make_valid
import requests # type: ignore
from geopy.geocoders import Nominatim # type: ignore
from geopy.exc import GeocoderTimedOut, GeocoderUnavailable, GeocoderServiceError # type: ignore
import logging

from sqlalchemy.orm import Session
from backend import database, models, crud

# --- Constants ---
# STAGING_AREA_PATH: Defines the directory for temporary storage of uploaded raw files.
# Security Considerations:
# 1. OS-level permissions: This directory should have strict permissions.
#    - Write access should ideally be limited to the user account running the application.
#    - It should NOT be world-writable.
#    - Read access by other unauthorized users/services should be prevented.
# 2. No Direct HTTP Serving: This directory should NOT be configured to serve files directly
#    via HTTP by the web server (e.g., Nginx, Apache). Files are accessed by the application logic only.
# 3. Lifecycle Management: Consider a strategy for cleaning up orphaned files (e.g., from failed
#    processing jobs or incomplete uploads) to prevent disk space exhaustion and data remanence.
#    This might involve periodic cleanup scripts or specific error handling routines.
STAGING_AREA_PATH = "staging_area"
PROCESSED_AREA_PATH = "processed_area" # Stores files after initial processing steps
TARGET_CRS_EPSG = 4326
EXPECTED_INPUT_FORMATS = [
    "ESRI Shapefile", "GeoJSON", "KML", "GML", "GPKG", "FlatGeobuf"
]

# --- File Upload Validation Constants ---
MAX_FILE_SIZE = 100 * 1024 * 1024
ALLOWED_MIME_TYPES = [
    "application/geo+json", "application/json", "application/zip",
    "application/vnd.google-earth.kml+xml", "application/gml+xml",
    "text/xml", "application/octet-stream"
]

# --- Logging Configuration ---
logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(name)s - %(levelname)s - %(message)s')
logger = logging.getLogger(__name__)

# --- Ensure Core Directories Exist on Startup ---
os.makedirs(STAGING_AREA_PATH, exist_ok=True)
os.makedirs(PROCESSED_AREA_PATH, exist_ok=True)
logger.info(f"Staging area for temporary raw file storage ensured at: {STAGING_AREA_PATH}")
logger.info(f"Processed area for intermediate and final data products ensured at: {PROCESSED_AREA_PATH}")


# --- Celery Configuration ---
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

# --- Pydantic Models ---
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
    geocoding_summary: Optional[Dict[str, Any]] = None  # New field for Pydantic model
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

# --- Dummy Auth Data ---
dummy_admin_role = Role(name="Admin", permissions=["create_index", "delete_index", "query_data", "manage_users", "upload_data"])
dummy_analyst_role = Role(name="Analyst", permissions=["create_index", "query_data", "upload_data"])
dummy_viewer_role = Role(name="Viewer", permissions=["query_data"])
dummy_users_db = {
    "admin": User(username="admin", roles=[dummy_admin_role]),
    "analyst": User(username="analyst", roles=[dummy_analyst_role]),
    "viewer": User(username="viewer", roles=[dummy_viewer_role]),
}
# oauth2_scheme = OAuth2PasswordBearer(tokenUrl="token") # Old scheme
X_AUTH_TOKEN_SCHEME = APIKeyHeader(name="X-Auth-Token", auto_error=False) # auto_error=False to handle missing token manually

app = FastAPI(title="SPINDEX Backend", version="0.1.0")

# Placeholder for a secure token store/validation mechanism
SECURE_PLACEHOLDER_TOKEN_ADMIN = "secure-placeholder-token-admin"
SECURE_PLACEHOLDER_TOKEN_ANALYST = "secure-placeholder-token-analyst"
SECURE_PLACEHOLDER_TOKEN_VIEWER = "secure-placeholder-token-viewer"

# Mapping of placeholder tokens to usernames
# In a real system, this would involve a database lookup or a call to an auth service.
TOKEN_TO_USER_MAPPING = {
    SECURE_PLACEHOLDER_TOKEN_ADMIN: "admin",
    SECURE_PLACEHOLDER_TOKEN_ANALYST: "analyst",
    SECURE_PLACEHOLDER_TOKEN_VIEWER: "viewer",
}

async def get_current_user(token: Optional[str] = Depends(X_AUTH_TOKEN_SCHEME)) -> User:
    """
    Placeholder authentication mechanism.
    TODO: Replace with a production-ready authentication system (e.g., JWT, OAuth2).
    This current implementation uses a hardcoded token for demonstration and testing.
    """
    if not token:
        logger.warning("Authentication attempt failed: No X-Auth-Token header provided.")
        raise HTTPException(
            status_code=status.HTTP_401_UNAUTHORIZED,
            detail="Not authenticated: X-Auth-Token header is missing.",
            headers={"WWW-Authenticate": "Header"}, # Using "Header" as scheme for custom header
        )

    username = TOKEN_TO_USER_MAPPING.get(token)
    if not username or username not in dummy_users_db:
        logger.warning(f"Authentication attempt failed: Invalid token received. User mapping not found for token ending with '...{token[-4:] if len(token) > 4 else token}'.") # Log last few chars for tracing, not the whole token
        raise HTTPException(
            status_code=status.HTTP_401_UNAUTHORIZED,
            detail="Invalid authentication credentials.",
            headers={"WWW-Authenticate": "Header"},
        )

    logger.info(f"Authentication successful for user: {username} (token provided).")
    return dummy_users_db[username]

def user_has_permission(user: User, permission_needed: str) -> bool:
    for role in user.roles:
        if permission_needed in role.permissions: return True
    return False

# --- AEGIS Pipeline Helper Functions ---
def convert_to_geojson(source_filepath: str, output_dir: str, job_id: str, original_filename: str, log_prefix: str) -> Dict[str, Any]:
    logger.info(f"{log_prefix} Starting format conversion for {original_filename} (path: {source_filepath})")
    geojson_filename = f"{job_id}_converted.geojson"
    geojson_filepath = os.path.join(output_dir, geojson_filename)
    conversion_result = {"detected_format": "Unknown", "geojson_filepath": None, "layer_count": 0, "total_features_processed": 0, "errors_encountered": [], "original_crs_wkt": None}
    in_ds = ogr.Open(source_filepath, 0)
    if in_ds is None:
        error_msg = f"GDAL: Could not open source file '{source_filepath}'."; logger.error(f"{log_prefix} {error_msg}"); conversion_result["errors_encountered"].append(error_msg); raise Exception(error_msg)
    detected_format = in_ds.GetDriver().GetName(); conversion_result["detected_format"] = detected_format
    logger.info(f"{log_prefix} Detected format: {detected_format} for {original_filename}.")
    if detected_format not in EXPECTED_INPUT_FORMATS: logger.warning(f"{log_prefix} Format '{detected_format}' not in EXPECTED_INPUT_FORMATS list.")
    out_driver = ogr.GetDriverByName("GeoJSON")
    if out_driver is None:
        error_msg = "GDAL: GeoJSON driver not available."; logger.error(f"{log_prefix} {error_msg}"); in_ds.Destroy(); conversion_result["errors_encountered"].append(error_msg); raise Exception(error_msg)
    if os.path.exists(geojson_filepath): out_driver.DeleteDataSource(geojson_filepath)
    out_ds = out_driver.CreateDataSource(geojson_filepath)
    if out_ds is None:
        error_msg = f"GDAL: Could not create output GeoJSON file '{geojson_filepath}'."; logger.error(f"{log_prefix} {error_msg}"); in_ds.Destroy(); conversion_result["errors_encountered"].append(error_msg); raise Exception(error_msg)
    total_features = 0; num_layers = in_ds.GetLayerCount(); conversion_result["layer_count"] = num_layers;
    if num_layers > 0:
        first_layer_srs = in_ds.GetLayer(0).GetSpatialRef()
        if first_layer_srs: conversion_result["original_crs_wkt"] = first_layer_srs.ExportToWkt()
    for i in range(num_layers):
        in_layer = in_ds.GetLayer(i); out_layer_name = in_layer.GetName()
        out_layer = out_ds.CreateLayer(out_layer_name, geom_type=in_layer.GetGeomType(), srs=in_layer.GetSpatialRef())
        layer_defn = in_layer.GetLayerDefn()
        for j in range(layer_defn.GetFieldCount()): out_layer.CreateField(layer_defn.GetFieldDefn(j))
        for in_feature in in_layer:
            out_feature = ogr.Feature(out_layer.GetLayerDefn()); out_feature.SetFrom(in_feature)
            if out_layer.CreateFeature(out_feature) != ogr.OGRERR_NONE:
                err = f"Failed to create feature in output layer for job {job_id}"; logger.warning(f"{log_prefix} {err}"); conversion_result["errors_encountered"].append(err)
            out_feature.Destroy(); total_features += 1
    conversion_result["total_features_processed"] = total_features; conversion_result["geojson_filepath"] = geojson_filepath
    logger.info(f"{log_prefix} Converted {original_filename} to GeoJSON: {geojson_filepath} ({num_layers} layers, {total_features} features). Original CRS (first layer WKT): {conversion_result['original_crs_wkt']}")
    in_ds.Destroy(); out_ds.Destroy(); return conversion_result

def transform_geojson_crs(source_geojson_filepath: str, output_dir: str, job_id: str, target_crs_epsg: int, original_crs_wkt_from_previous_step: Optional[str], log_prefix: str) -> Dict[str, Any]:
    logger.info(f"{log_prefix} Starting CRS transformation for {source_geojson_filepath} to EPSG:{target_crs_epsg}")
    transformed_filename = f"{job_id}_crs_transformed.geojson"
    transformed_filepath = os.path.join(output_dir, transformed_filename)
    transform_result = {"crs_transformed_geojson_filepath": None, "detected_input_crs_wkt": original_crs_wkt_from_previous_step, "errors_encountered": []}
    in_ds = ogr.Open(source_geojson_filepath, 0)
    if in_ds is None:
        error_msg = f"GDAL: Could not open GeoJSON file '{source_geojson_filepath}' for CRS transformation."; logger.error(f"{log_prefix} {error_msg}"); transform_result["errors_encountered"].append(error_msg); raise Exception(error_msg)
    target_srs = osr.SpatialReference(); target_srs.ImportFromEPSG(target_crs_epsg)
    out_driver = ogr.GetDriverByName("GeoJSON")
    if out_driver is None:
        error_msg = "GDAL: GeoJSON driver not available for CRS transform output."; logger.error(f"{log_prefix} {error_msg}"); in_ds.Destroy(); transform_result["errors_encountered"].append(error_msg); raise Exception(error_msg)
    if os.path.exists(transformed_filepath): out_driver.DeleteDataSource(transformed_filepath)
    out_ds = out_driver.CreateDataSource(transformed_filepath)
    if out_ds is None:
        error_msg = f"GDAL: Could not create output CRS-transformed file '{transformed_filepath}'."; logger.error(f"{log_prefix} {error_msg}"); in_ds.Destroy(); transform_result["errors_encountered"].append(error_msg); raise Exception(error_msg)
    total_features_transformed = 0
    for i in range(in_ds.GetLayerCount()):
        in_layer = in_ds.GetLayer(i); in_srs = in_layer.GetSpatialRef()
        if not in_srs and original_crs_wkt_from_previous_step:
            in_srs = osr.SpatialReference(); in_srs.ImportFromWkt(original_crs_wkt_from_previous_step)
            logger.info(f"{log_prefix} Used original_crs_wkt for transformation as GeoJSON layer had no SRS.")
        elif not in_srs:
            logger.warning(f"{log_prefix} No CRS found in GeoJSON layer and no original_crs_wkt provided. Assuming EPSG:4326 for input.")
            in_srs = osr.SpatialReference(); in_srs.ImportFromEPSG(4326)
        transform_result["detected_input_crs_wkt"] = in_srs.ExportToWkt() if in_srs else "Unknown"
        transform = osr.CoordinateTransformation(in_srs, target_srs) if in_srs and target_srs and not in_srs.IsSame(target_srs) else None
        out_layer_name = os.path.splitext(transformed_filename)[0]
        out_layer = out_ds.CreateLayer(out_layer_name, geom_type=in_layer.GetGeomType(), srs=target_srs)
        layer_defn = in_layer.GetLayerDefn()
        for j in range(layer_defn.GetFieldCount()): out_layer.CreateField(layer_defn.GetFieldDefn(j))
        for in_feature in in_layer:
            out_feature = ogr.Feature(out_layer.GetLayerDefn()); out_feature.SetFrom(in_feature)
            geom = out_feature.GetGeometryRef()
            if geom and transform:
                ogr_err = geom.Transform(transform)
                if ogr_err != ogr.OGRERR_NONE: logger.warning(f"{log_prefix} Feature {in_feature.GetFID()} geometry transformation failed with OGR error: {ogr_err}")
            elif geom and not transform: logger.debug(f"{log_prefix} No transformation needed/applied for feature {in_feature.GetFID()}.")
            else: logger.debug(f"{log_prefix} No geometry or no transform for feature {in_feature.GetFID()}.")
            out_layer.CreateFeature(out_feature); out_feature.Destroy()
            total_features_transformed +=1
    transform_result["crs_transformed_geojson_filepath"] = transformed_filepath
    logger.info(f"{log_prefix} CRS transformation successful. Output: {transformed_filepath} ({total_features_transformed} features). Detected input CRS for transform: {transform_result['detected_input_crs_wkt']}")
    in_ds.Destroy(); out_ds.Destroy(); return transform_result

def validate_geometries(source_geojson_filepath: str, job_id: str, log_prefix: str, attempt_repair: bool = False) -> Dict[str, Any]:
    logger.info(f"{log_prefix} Starting geometric validation for {source_geojson_filepath}")
    validation_results = {"valid_features": 0, "invalid_features": 0, "repaired_features": 0, "features_total": 0, "validation_passed": False, "output_filepath": source_geojson_filepath, "errors_encountered": []}
    features_to_write = []; modified_by_repair = False
    try:
        with open(source_geojson_filepath, 'r') as f: geojson_data = json.load(f)
        if geojson_data.get("type") != "FeatureCollection" or not isinstance(geojson_data.get("features"), list):
            raise ValueError("Input for geometry validation is not a valid GeoJSON FeatureCollection.")
        features = geojson_data["features"]; validation_results["features_total"] = len(features)
        for i, feature in enumerate(features):
            if 'geometry' in feature and feature['geometry']:
                try:
                    geom = shape(feature['geometry'])
                    if is_valid(geom): validation_results["valid_features"] += 1; features_to_write.append(feature)
                    else:
                        validation_results["invalid_features"] += 1; logger.warning(f"{log_prefix} Invalid geometry found for feature {i} (FID if available: {feature.get('id', 'N/A')}).")
                        if attempt_repair:
                            repaired_geom = make_valid(geom)
                            if repaired_geom and is_valid(repaired_geom):
                                logger.info(f"{log_prefix} Successfully repaired geometry for feature {i}.")
                                validation_results["repaired_features"] += 1; import shapely.geometry; feature['geometry'] = shapely.geometry.mapping(repaired_geom)
                                features_to_write.append(feature); modified_by_repair = True
                            else: logger.warning(f"{log_prefix} Failed to repair geometry for feature {i} or repaired geometry is still invalid."); features_to_write.append(feature)
                        else: features_to_write.append(feature)
                except Exception as e_shapely:
                    logger.error(f"{log_prefix} Error processing geometry for feature {i}: {str(e_shapely)}"); validation_results["invalid_features"] += 1
                    validation_results["errors_encountered"].append(f"Feature {i} geometry processing error: {str(e_shapely)}"); features_to_write.append(feature)
            else: validation_results["valid_features"] += 1; features_to_write.append(feature)
        if attempt_repair and modified_by_repair:
            repaired_geojson_filename = f"{job_id}_geom_repaired.geojson"; repaired_filepath = os.path.join(os.path.dirname(source_geojson_filepath), repaired_geojson_filename)
            repaired_geojson_data = {"type": "FeatureCollection", "features": features_to_write}
            with open(repaired_filepath, 'w') as f: json.dump(repaired_geojson_data, f)
            logger.info(f"{log_prefix} Saved geometry-repaired data to: {repaired_filepath}"); validation_results["output_filepath"] = repaired_filepath
        if attempt_repair: validation_results["validation_passed"] = (validation_results["invalid_features"] == validation_results["repaired_features"])
        else: validation_results["validation_passed"] = (validation_results["invalid_features"] == 0)
    except Exception as e:
        error_msg = f"Geometric validation stage failed critically: {str(e)}"; logger.error(f"{log_prefix} {error_msg}")
        validation_results["errors_encountered"].append(error_msg); validation_results["validation_passed"] = False
    logger.info(f"{log_prefix} Geometric validation complete. Results: {validation_results}"); return validation_results

# --- Geocoding Helper Function ---
def geocode_features(source_geojson_filepath: str, output_dir: str, job_id: str, log_prefix: str) -> Dict[str, Any]:
    """
    Geocodes features in a GeoJSON file if address-like properties are found.
    Uses Nominatim for geocoding.
    """
    logger.info(f"{log_prefix} Starting geocoding for {source_geojson_filepath}")
    geolocator = Nominatim(user_agent=f"spindex_aegis_pipeline/0.1 job_id/{job_id}")

    # Hardcoded address field configuration
    # Maps address part names to potential feature property names (case-insensitive check)
    address_field_config = {
        "street": ["street", "address", "addr", "street_address"],
        "city": ["city", "town", "municipality"],
        "state": ["state", "province", "region"],
        "country": ["country", "country_code"],
        "postalcode": ["postal_code", "zipcode", "zip_code", "postcode"],
        "full_address": ["full_address", "address_line", "location", "beschreibung"] # For single field address
    }

    geocoded_filename = f"{job_id}_geocoded.geojson"
    geocoded_filepath = os.path.join(output_dir, geocoded_filename)

    summary = {
        "output_filepath": None,
        "features_processed": 0,
        "geocoded_successfully": 0,
        "geocoding_failed_no_match": 0,
        "geocoding_service_error": 0,
        "skipped_no_address_fields": 0,
        "errors_encountered": []
    }

    output_features = []

    try:
        with open(source_geojson_filepath, 'r', encoding='utf-8') as f:
            geojson_data = json.load(f)

        if not isinstance(geojson_data, dict) or geojson_data.get("type") != "FeatureCollection" or not isinstance(geojson_data.get("features"), list):
            err_msg = "Input for geocoding is not a valid GeoJSON FeatureCollection."
            logger.error(f"{log_prefix} {err_msg}")
            summary["errors_encountered"].append(err_msg)
            # Depending on desired strictness, could raise Exception(err_msg) here
            return summary

        features = geojson_data["features"]
        summary["features_processed"] = len(features)

        for i, feature in enumerate(features):
            properties = feature.get("properties", {})
            if not properties: # Skip if no properties
                summary["skipped_no_address_fields"] += 1
                output_features.append(feature)
                continue

            # Case-insensitive property key matching
            props_lower = {k.lower(): v for k, v in properties.items()}

            query_parts = {}
            address_found = False

            # Check for a single "full_address" field first
            for field_name_variant in address_field_config["full_address"]:
                if field_name_variant.lower() in props_lower and isinstance(props_lower[field_name_variant.lower()], str) and props_lower[field_name_variant.lower()].strip():
                    query_string = props_lower[field_name_variant.lower()].strip()
                    address_found = True
                    break

            if not address_found:
                # Try to construct address from parts
                for part_key, common_names in address_field_config.items():
                    if part_key == "full_address": continue # Already handled
                    for name_variant in common_names:
                        if name_variant.lower() in props_lower and isinstance(props_lower[name_variant.lower()], str) and props_lower[name_variant.lower()].strip():
                            query_parts[part_key] = props_lower[name_variant.lower()].strip()
                            address_found = True
                            break # Found one variant for this part

                if query_parts:
                    # Construct query string (example: street, city, country)
                    # More sophisticated ordering could be applied here.
                    query_string = ", ".join(filter(None, [
                        query_parts.get("street"),
                        query_parts.get("city"),
                        query_parts.get("postalcode"),
                        query_parts.get("state"),
                        query_parts.get("country")
                    ]))
                else:
                    query_string = None

            if not query_string:
                summary["skipped_no_address_fields"] += 1
                output_features.append(feature)
                continue

            logger.info(f"{log_prefix} Attempting to geocode address for feature {i}: '{query_string}'")
            try:
                time.sleep(1.1) # IMPORTANT: Respect Nominatim's usage policy
                location = geolocator.geocode(query_string, timeout=10)
                if location:
                    logger.info(f"{log_prefix} Geocoded feature {i} to: {location.address} ({location.latitude}, {location.longitude})")
                    original_geometry = feature.get("geometry")
                    feature["geometry"] = {
                        "type": "Point",
                        "coordinates": [location.longitude, location.latitude]
                    }
                    properties["geocoding_service"] = "Nominatim"
                    properties["geocoded_address"] = location.address
                    if original_geometry and original_geometry.get("type"):
                        properties["original_geometry_type"] = original_geometry.get("type")
                    else:
                        properties["original_geometry_type"] = "Unknown/None"
                    feature["properties"] = properties # Ensure updated properties are set
                    summary["geocoded_successfully"] += 1
                else:
                    logger.warning(f"{log_prefix} Geocoding failed (no match) for feature {i}, address: '{query_string}'")
                    summary["geocoding_failed_no_match"] += 1
            except GeocoderTimedOut:
                logger.error(f"{log_prefix} Geocoding service timed out for feature {i}, address: '{query_string}'")
                summary["geocoding_service_error"] += 1
                summary["errors_encountered"].append(f"GeocoderTimedOut for feature {i}: {query_string}")
            except (GeocoderUnavailable, GeocoderServiceError) as e_geo:
                logger.error(f"{log_prefix} Geocoding service error for feature {i}, address: '{query_string}': {str(e_geo)}")
                summary["geocoding_service_error"] += 1
                summary["errors_encountered"].append(f"GeocoderServiceError for feature {i} ({query_string}): {str(e_geo)}")
            except Exception as e_gen: # Catch any other unexpected error during geocoding attempt
                logger.error(f"{log_prefix} Unexpected error during geocoding feature {i}, address: '{query_string}': {str(e_gen)}", exc_info=True)
                summary["geocoding_service_error"] += 1 # Count as a service error for simplicity
                summary["errors_encountered"].append(f"Unexpected error geocoding feature {i} ({query_string}): {str(e_gen)}")

            output_features.append(feature)

        # Write the new GeoJSON file
        updated_geojson_data = {"type": "FeatureCollection", "features": output_features}
        if not os.path.exists(output_dir):
            os.makedirs(output_dir) # Should already exist but good practice
        with open(geocoded_filepath, 'w', encoding='utf-8') as f:
            json.dump(updated_geojson_data, f, ensure_ascii=False, indent=2)

        summary["output_filepath"] = geocoded_filepath
        logger.info(f"{log_prefix} Geocoding process complete. Output: {geocoded_filepath}. Summary: {summary}")

    except FileNotFoundError:
        err_msg = f"Source GeoJSON file not found: {source_geojson_filepath}"
        logger.error(f"{log_prefix} {err_msg}")
        summary["errors_encountered"].append(err_msg)
        # raise Exception(err_msg) # Or handle as per desired strictness
    except json.JSONDecodeError as e_json:
        err_msg = f"Invalid JSON in source file {source_geojson_filepath}: {str(e_json)}"
        logger.error(f"{log_prefix} {err_msg}")
        summary["errors_encountered"].append(err_msg)
        # raise Exception(err_msg)
    except Exception as e_main:
        err_msg = f"Critical error in geocode_features: {str(e_main)}"
        logger.error(f"{log_prefix} {err_msg}", exc_info=True)
        summary["errors_encountered"].append(err_msg)
        # raise Exception(err_msg) # Propagate if the entire stage should halt the pipeline on any error

    return summary

# --- Celery Tasks ---
@shared_task
def process_staged_file_task(job_id_from_upload: str, staged_file_path: str, original_filename: str, user_id: str) -> str:
    task_id = process_staged_file_task.request.id
    log_prefix = f"CeleryTaskID-{task_id} (JobID: {job_id_from_upload}, File: {original_filename}, User: {user_id}):"
    logger.info(f"{log_prefix} Received for processing. Staged path: '{staged_file_path}'.")
    db: Session = database.SessionLocal()

    # Initialize step success flags
    format_conversion_success = False
    crs_transform_success = False
    geometric_validation_passed_flag = False # Explicit flag for this stage's direct outcome
    geocoding_attempted = False # Flag to know if geocoding was tried
    geocoding_success = False # Represents success of the geocoding module execution itself
    complex_geom_handling_success = True # Placeholder
    schema_validation_passed = True # Placeholder
    content_security_passed = True # Placeholder

    current_processed_file = staged_file_path
    original_crs_wkt = None # To be populated by format conversion

    try:
        crud.update_ingestion_job_fields(db, job_id_from_upload, {"processing_status": "processing_in_progress", "celery_task_id": str(task_id)})
        crud.update_pipeline_log(db, job_id_from_upload, {"stage": "initialization", "status": "success", "timestamp": datetime.utcnow().isoformat(), "message": "Processing started."})
        db.commit()

        # === Stage 1: Format Conversion to GeoJSON ===
        try:
            crud.update_pipeline_log(db, job_id_from_upload, {"stage": "format_conversion_started", "status": "running", "timestamp": datetime.utcnow().isoformat(), "input_file": current_processed_file})
            conversion_details = convert_to_geojson(current_processed_file, PROCESSED_AREA_PATH, job_id_from_upload, original_filename, log_prefix)
            original_crs_wkt = conversion_details["original_crs_wkt"]
            crud.update_ingestion_job_fields(db, job_id_from_upload, {"detected_format": conversion_details["detected_format"]})
            current_processed_file = conversion_details["geojson_filepath"]
            crud.update_pipeline_log(db, job_id_from_upload, {"stage": "format_conversion_completed", "status": "success", "timestamp": datetime.utcnow().isoformat(), "details": conversion_details})
            logger.info(f"{log_prefix} Format conversion successful. Output: {current_processed_file}")
            format_conversion_success = True
        except Exception as e:
            error_message = f"Format Conversion failed: {str(e)}"; logger.error(f"{log_prefix} ERROR: {error_message}", exc_info=True)
            crud.update_error_logs(db, job_id_from_upload, f"format_conversion_error: {error_message}")
            crud.update_pipeline_log(db, job_id_from_upload, {"stage": "format_conversion_error", "status": "failure", "error": error_message, "timestamp": datetime.utcnow().isoformat()})
            crud.update_ingestion_job_fields(db, job_id_from_upload, {
                "processing_status": "format_conversion_failed",
                "quarantine_timestamp": datetime.utcnow(),
                "location_of_quarantined_data": staged_file_path # File is still in its original staged location
            })
            db.commit(); return f"Error during Format Conversion for '{original_filename}'."
        db.commit()

        # === Stage 2: CRS Transformation ===
        if format_conversion_success:
            try:
                crud.update_pipeline_log(db, job_id_from_upload, {"stage": "crs_transformation_started", "status": "running", "timestamp": datetime.utcnow().isoformat(), "input_file": current_processed_file})
                transform_details = transform_geojson_crs(current_processed_file, PROCESSED_AREA_PATH, job_id_from_upload, TARGET_CRS_EPSG, original_crs_wkt, log_prefix)
                crud.update_ingestion_job_fields(db, job_id_from_upload, {"original_crs": transform_details["detected_input_crs_wkt"], "target_crs": f"EPSG:{TARGET_CRS_EPSG}"})
                current_processed_file = transform_details["crs_transformed_geojson_filepath"]
                crud.update_pipeline_log(db, job_id_from_upload, {"stage": "crs_transformation_completed", "status": "success", "timestamp": datetime.utcnow().isoformat(), "details": transform_details})
                logger.info(f"{log_prefix} CRS transformation successful. Output: {current_processed_file}")
                crs_transform_success = True
            except Exception as e:
                error_message = f"CRS Transformation failed: {str(e)}"; logger.error(f"{log_prefix} ERROR: {error_message}", exc_info=True)
                crud.update_error_logs(db, job_id_from_upload, f"crs_transformation_error: {error_message}")
                crud.update_pipeline_log(db, job_id_from_upload, {"stage": "crs_transformation_error", "status": "failure", "error": error_message, "timestamp": datetime.utcnow().isoformat()})
                crud.update_ingestion_job_fields(db, job_id_from_upload, {
                    "processing_status": "crs_transformation_failed",
                    "quarantine_timestamp": datetime.utcnow(),
                    "location_of_quarantined_data": current_processed_file # This is the output of format conversion
                })
                db.commit(); return f"Error during CRS Transformation for '{original_filename}'."
            db.commit()

        # === Stage 3: Geometric Validation ===
        if crs_transform_success:
            try:
                crud.update_pipeline_log(db, job_id_from_upload, {"stage": "geometric_validation_started", "status": "running", "timestamp": datetime.utcnow().isoformat(), "input_file": current_processed_file})
                validation_results = validate_geometries(current_processed_file, job_id_from_upload, log_prefix, attempt_repair=False)
                crud.update_validation_summary(db, job_id_from_upload, {"geometric_validation": validation_results})
                geometric_validation_passed_flag = validation_results["validation_passed"]
                # current_processed_file remains unchanged if no repair occurs that creates a new file
                # current_processed_file = validation_results["output_filepath"] # Only if repair creates new file
                log_status = "success" if geometric_validation_passed_flag else "failure_with_invalids"
                crud.update_pipeline_log(db, job_id_from_upload, {"stage": "geometric_validation_completed", "status": log_status, "timestamp": datetime.utcnow().isoformat(), "details": validation_results})
                if not geometric_validation_passed_flag:
                    logger.warning(f"{log_prefix} Geometric validation found invalid geometries. Invalid count: {validation_results['invalid_features']}")
            except Exception as e:
                error_message = f"Geometric Validation failed critically: {str(e)}"; logger.error(f"{log_prefix} ERROR: {error_message}", exc_info=True)
                crud.update_error_logs(db, job_id_from_upload, f"geometric_validation_critical_error: {error_message}")
                crud.update_pipeline_log(db, job_id_from_upload, {"stage": "geometric_validation_critical_error", "status": "failure", "error": error_message, "timestamp": datetime.utcnow().isoformat()})
                crud.update_ingestion_job_fields(db, job_id_from_upload, {
                    "processing_status": "geometric_validation_critical_error",
                    "quarantine_timestamp": datetime.utcnow(),
                    "location_of_quarantined_data": current_processed_file # Input to this failed stage
                })
                geometric_validation_passed_flag = False
            db.commit()

        # === Stage 4: Conditional Geocoding ===
        if geometric_validation_passed_flag: # Only proceed if previous critical stages passed
            geocoding_attempted = False
            # Heuristic: Check if first feature has potential address fields
            try:
                with open(current_processed_file, 'r', encoding='utf-8') as f:
                    temp_geojson_data = json.load(f)
                if temp_geojson_data.get("type") == "FeatureCollection" and temp_geojson_data.get("features"):
                    first_feature_props = temp_geojson_data["features"][0].get("properties", {})
                    if first_feature_props:
                        props_lower_keys = [key.lower() for key in first_feature_props.keys()]
                        # Check against a subset of address_field_config keys used in geocode_features
                        address_heuristic_fields = ["address", "street", "addr", "city", "location", "beschreibung"]
                        if any(any(ah_field in prop_key for ah_field in address_heuristic_fields) for prop_key in props_lower_keys):
                            geocoding_attempted = True
                            logger.info(f"{log_prefix} Address-like fields detected in first feature, proceeding with geocoding attempt.")
                        else:
                            logger.info(f"{log_prefix} No common address-like fields detected in first feature's properties. Skipping geocoding.")
                    else:
                        logger.info(f"{log_prefix} First feature has no properties. Skipping geocoding.")
                else:
                    logger.info(f"{log_prefix} Not a FeatureCollection or no features found in {current_processed_file}. Skipping geocoding.")
            except Exception as e_heuristic:
                logger.warning(f"{log_prefix} Error during geocoding heuristic check: {str(e_heuristic)}. Skipping geocoding.", exc_info=True)
                geocoding_attempted = False # Ensure it's false if heuristic check fails

            if geocoding_attempted:
                try:
                    crud.update_pipeline_log(db, job_id_from_upload, {"stage": "geocoding_started", "status": "running", "timestamp": datetime.utcnow().isoformat(), "input_file": current_processed_file})
                    geocoding_details = geocode_features(current_processed_file, PROCESSED_AREA_PATH, job_id_from_upload, log_prefix)
                    crud.update_ingestion_job_fields(db, job_id_from_upload, {"geocoding_summary": geocoding_details})

                    if geocoding_details.get("errors_encountered"):
                        logger.error(f"{log_prefix} Geocoding module encountered errors: {geocoding_details['errors_encountered']}")
                        # Decide if these errors are critical enough to fail the geocoding_success flag
                        # For now, if the module produced an output file, we consider the module itself "successful" in execution
                        # The summary details will show specific feature geocoding failures.
                        # If geocoding_details["output_filepath"] is None, it means a critical error happened before file write.
                        if geocoding_details["output_filepath"] is None:
                             geocoding_success = False # Critical failure within geocoding module
                             crud.update_error_logs(db, job_id_from_upload, f"geocoding_critical_error: {'; '.join(geocoding_details['errors_encountered'])}")
                        else:
                             current_processed_file = geocoding_details["output_filepath"]
                             geocoding_success = True # Module ran, file produced, specific errors are in summary
                    else:
                        current_processed_file = geocoding_details["output_filepath"]
                        geocoding_success = True

                    log_status = "success" if geocoding_success else "failure"
                    if not geocoding_success and geocoding_details.get("output_filepath") is None : # Critical failure
                         log_status = "critical_failure"
                    elif geocoding_details.get("errors_encountered"):
                        log_status = "completed_with_errors"

                    crud.update_pipeline_log(db, job_id_from_upload, {"stage": "geocoding_completed", "status": log_status, "timestamp": datetime.utcnow().isoformat(), "details": geocoding_details})
                    logger.info(f"{log_prefix} Geocoding completed. Success: {geocoding_success}. Output: {current_processed_file if geocoding_success and geocoding_details.get('output_filepath') else 'N/A'}")
                except Exception as e:
                    error_message = f"Geocoding stage failed critically: {str(e)}"; logger.error(f"{log_prefix} ERROR: {error_message}", exc_info=True)
                    crud.update_error_logs(db, job_id_from_upload, f"geocoding_critical_error: {error_message}")
                    crud.update_pipeline_log(db, job_id_from_upload, {"stage": "geocoding_critical_error", "status": "failure", "error": error_message, "timestamp": datetime.utcnow().isoformat()})
                    crud.update_ingestion_job_fields(db, job_id_from_upload, {
                        "processing_status": "geocoding_failed_critical", # New status
                        "quarantine_timestamp": datetime.utcnow(),
                        "location_of_quarantined_data": current_processed_file # File before geocoding attempt
                    })
                    geocoding_success = False
                db.commit()
            else: # Geocoding not attempted
                geocoding_success = True # Skipped, so not a failure of the module itself
                crud.update_pipeline_log(db, job_id_from_upload, {"stage": "geocoding_skipped", "status": "success", "timestamp": datetime.utcnow().isoformat(), "message": "Geocoding skipped based on heuristic."})
                db.commit()
        else: # Previous stage failed, skip geocoding
             geocoding_success = True # Skipped due to prior error, not a geocoding failure itself.
             logger.info(f"{log_prefix} Skipping geocoding as previous stages were not successful.")
             crud.update_pipeline_log(db, job_id_from_upload, {"stage": "geocoding_skipped_prior_failure", "status": "skipped", "timestamp": datetime.utcnow().isoformat()})
             db.commit()


        # Placeholders for: Complex Geometry Handling, Security Scan, Schema Validation
        # These would set their own success flags: complex_geom_handling_success, etc.

        # === Final Status Update (Promotion/Quarantine Logic) ===
        final_status_fields = {}
        overall_success = (
            format_conversion_success and
            crs_transform_success and
            geometric_validation_passed_flag and
            (geocoding_success if geocoding_attempted else True) and # Geocoding module must succeed *if attempted*
            complex_geom_handling_success and # Placeholder
            schema_validation_passed and      # Placeholder
            content_security_passed           # Placeholder
        )

        if overall_success:
            final_status_fields["processing_status"] = "validation_successful_ready_for_promotion"
            final_status_fields["promotion_timestamp"] = datetime.utcnow()
            final_status_fields["location_of_promoted_data"] = current_processed_file # This is the final validated file path
            result_message = f"File '{original_filename}' processed successfully. Promoted: {current_processed_file}"; logger.info(f"{log_prefix} {result_message}")
        else:
            final_status_fields["processing_status"] = "quarantined_validation_failed"
            final_status_fields["quarantine_timestamp"] = datetime.utcnow()
            final_status_fields["location_of_quarantined_data"] = current_processed_file # Path to file at point of failure/last good state

            error_list = []
            if not format_conversion_success: error_list.append("FormatConversionFailed")
            elif not crs_transform_success: error_list.append("CRSTransformationFailed")
            elif not geometric_validation_passed_flag: error_list.append("GeometricValidationFailed")
            elif geocoding_attempted and not geocoding_success: error_list.append("GeocodingModuleFailedCritically")
            # Add other specific failure reasons as they are implemented
            error_detail = "; ".join(error_list) if error_list else "Unknown validation failure or non-critical processing issue."
            crud.update_error_logs(db, job_id_from_upload, f"overall_pipeline_failure_leading_to_quarantine: {error_detail}")
            result_message = f"File '{original_filename}' failed validation or a critical processing stage and was quarantined. Details: {error_detail}"; logger.warning(f"{log_prefix} {result_message}")

        crud.update_ingestion_job_fields(db, job_id_from_upload, final_status_fields)
        db.commit()
        return result_message

    except Exception as e:
        logger.error(f"{log_prefix} Unhandled exception in Celery task: {str(e)}", exc_info=True)
        if db.is_active: db.rollback() # Rollback any partial transaction from the try block
        try:
            # current_processed_file holds the path to the file at the last known good state,
            # or the initial staged_file_path if no processing stage even started successfully.
            crud.update_ingestion_job_fields(db, job_id_from_upload, {
                "processing_status": "task_failed_uncaught_exception",
                "quarantine_timestamp": datetime.utcnow(),
                "location_of_quarantined_data": current_processed_file
            })
            crud.update_error_logs(db, job_id_from_upload, f"task_failed_uncaught_exception: {str(e)}")
            db.commit()
        except Exception as db_err:
            logger.error(f"{log_prefix} CRITICAL: Failed to update DB with uncaught exception details: {db_err}", exc_info=True)
        return f"Task failed for {original_filename} due to unhandled exception: {str(e)}"
    finally:
        if db.is_active: db.close()

# ... (Rest of the file: create_spindex_task, API endpoints, etc.)
@shared_task
def create_spindex_task(index_id: str, promoted_data_path: str, index_name: str, index_type: str, index_parameters: Optional[dict]):
    task_id = create_spindex_task.request.id
    log_prefix = f"CeleryTaskID-{task_id} (Index Creation for ID: {index_id}):"
    logger.info(f"{log_prefix} Received. Index Name: {index_name}, Type: {index_type}, Data: {promoted_data_path}, Params: {index_parameters}")
    metadata_record = next((idx for idx in index_metadata_db if idx.index_id == index_id), None)
    if metadata_record: metadata_record.celery_task_id = str(task_id)
    else: logger.critical(f"{log_prefix} CRITICAL_ERROR: IndexMetadata record (in-memory) not found for index_id {index_id} at task start.")
    logger.info(f"{log_prefix} SPINDEX core simulation: Creating {index_name} ({index_type}) from {promoted_data_path}...")
    time.sleep(15)
    if metadata_record:
        creation_succeeded = True
        if creation_succeeded:
            metadata_record.status = "ready"; metadata_record.updated_at = datetime.utcnow()
            logger.info(f"{log_prefix} Index '{index_name}' marked as 'ready'.")
            return f"Index '{index_name}' (ID: {index_id}) created successfully."
        else:
            metadata_record.status = "failed"; metadata_record.updated_at = datetime.utcnow()
            logger.warning(f"{log_prefix} Index '{index_name}' marked as 'failed'.")
            return f"Failed to create index '{index_name}' (ID: {index_id})."
    else:
        logger.critical(f"{log_prefix} CRITICAL_ERROR: IndexMetadata record (in-memory) not found for index_id {index_id} at task completion.")
        return f"Critical error: Metadata for index {index_id} not found upon task completion."

@app.post("/tasks/run_sample_task", response_model=TaskCreationResponse, status_code=status.HTTP_202_ACCEPTED, summary="Run Sample Celery Task", tags=["Tasks"])
async def run_sample_task(payload: TaskCreationRequest, current_user: User = Depends(get_current_user)):
    task = sample_celery_task.delay(payload.param_x, payload.param_y)
    return {"task_id": task.id, "message": "Sample task submitted successfully."}

@app.get("/tasks/status/{task_id}", response_model=TaskStatusResponse, summary="Get Task Status and Result", tags=["Tasks"])
async def get_task_status(task_id: str, current_user: User = Depends(get_current_user)):
    task_result = AsyncResult(task_id, app=celery_app)
    response_data = {"task_id": task_id, "status": task_result.status}
    if task_result.successful(): response_data["result"] = task_result.result
    elif task_result.failed(): response_data["result"] = {"error": str(task_result.info)}
    return response_data

@app.post("/upload/data", response_model=FileUploadResponse, status_code=status.HTTP_202_ACCEPTED, summary="Upload Data for Processing", tags=["Data Upload"])
async def upload_data_for_processing(
    current_user: User = Depends(get_current_user),
    file: UploadFile = File(...),
    db: Session = Depends(database.get_db)
):
    if not user_has_permission(current_user, "upload_data"):
        logger.warning(f"User '{current_user.username}' attempted to upload data without 'upload_data' permission.")
        raise HTTPException(status_code=status.HTTP_403_FORBIDDEN, detail="User does not have permission to upload data.")
    if not user_has_permission(current_user, "upload_data"):
        logger.warning(f"Authorization failed: User '{current_user.username}' attempted to upload data without 'upload_data' permission.") # Enhanced logging
        raise HTTPException(status_code=status.HTTP_403_FORBIDDEN, detail="User does not have permission to upload data.")

    # --- Input Validation (File Properties) ---
    # MAX_FILE_SIZE and ALLOWED_MIME_TYPES are defined as global constants.
    # Consider making these configurable via environment variables or a config file in a production setup.
    if not file.filename:
        logger.error(f"File upload attempt by user '{current_user.username}' failed: No filename provided.") # Enhanced logging
        raise HTTPException(status_code=status.HTTP_400_BAD_REQUEST, detail="No filename provided.")

    if file.size is None or file.size > MAX_FILE_SIZE:
        actual_size = file.size if file.size is not None else "unknown"
        logger.warning(f"File '{file.filename}' from user '{current_user.username}' rejected: size ({actual_size} bytes) exceeds MAX_FILE_SIZE ({MAX_FILE_SIZE} bytes).") # Enhanced logging
        raise HTTPException(status_code=status.HTTP_413_REQUEST_ENTITY_TOO_LARGE, detail=f"File size {actual_size} bytes exceeds maximum allowed size of {MAX_FILE_SIZE} bytes.")

    if file.content_type not in ALLOWED_MIME_TYPES:
        logger.warning(f"File '{file.filename}' from user '{current_user.username}' rejected: MIME type '{file.content_type}' not in ALLOWED_MIME_TYPES ({', '.join(ALLOWED_MIME_TYPES)}).") # Enhanced logging
        raise HTTPException(status_code=status.HTTP_415_UNSUPPORTED_MEDIA_TYPE, detail=f"File MIME type '{file.content_type}' is not allowed. Allowed types: {', '.join(ALLOWED_MIME_TYPES)}")

    logger.info(f"File '{file.filename}' (size: {file.size}, type: {file.content_type}) passed initial validation for user '{current_user.username}'.")

    # --- Secure File Staging ---
    # Ensure STAGING_AREA_PATH directory has appropriate OS-level permissions (e.g., writeable by the app user, not world-writeable).
    # This is an infrastructure concern but good to note in code.
    os.makedirs(STAGING_AREA_PATH, exist_ok=True) # Ensures directory exists
    original_filename = file.filename
    file_extension = os.path.splitext(original_filename)[1]
    unique_staged_filename = f"{uuid.uuid4()}{file_extension}"
    staged_file_path = os.path.join(STAGING_AREA_PATH, unique_staged_filename)

    try:
        with open(staged_file_path, "wb") as buffer:
            shutil.copyfileobj(file.file, buffer)
        logger.info(f"File '{original_filename}' from user '{current_user.username}' successfully saved to staging area: '{staged_file_path}'.") # Enhanced logging
    except Exception as e:
        logger.error(f"Could not save uploaded file '{original_filename}' from user '{current_user.username}' to staging area '{staged_file_path}': {str(e)}", exc_info=True) # Added exc_info for more details
        # Attempt to remove partially written file if saving failed
        if os.path.exists(staged_file_path):
            try:
                os.remove(staged_file_path)
                logger.info(f"Successfully removed partially written file: {staged_file_path}")
            except Exception as rm_e:
                logger.error(f"Failed to remove partially written file {staged_file_path} after save error: {rm_e}", exc_info=True)
        raise HTTPException(status_code=status.HTTP_500_INTERNAL_SERVER_ERROR, detail=f"Could not save uploaded file. An internal error occurred.") # More generic message to client
    finally:
        if file and hasattr(file, 'file') and not file.file.closed: # Ensure file object and its underlying stream exist and are not closed
            file.file.close()

    job_id = str(uuid.uuid4())
    celery_task_obj = process_staged_file_task.delay(job_id, staged_file_path, original_filename, current_user.username) # current_user.username will be available due to Depends(get_current_user)
    logger.info(f"Celery task '{celery_task_obj.id}' dispatched for file '{original_filename}' (user: {current_user.username}), associated with DB job_id '{job_id}'.") # Enhanced logging

    job_data_dict = {
        "job_id":job_id, "celery_task_id":str(celery_task_obj.id), "original_filename":original_filename,
        "upload_timestamp":datetime.utcnow(), "user_id":current_user.username, "source_type":"file_upload",
        "processing_status":"staged_and_queued", "staged_file_path":staged_file_path,
        "pipeline_stages_executed_log":[{"stage": "upload_complete_queued", "status": "success", "timestamp": datetime.utcnow().isoformat(), "detail": f"File received from user {current_user.username}"}]
    }
    crud.create_ingestion_job(db=db, job_data=job_data_dict)
    logger.info(f"Initial metadata record created in DB for job_id '{job_id}' (Celery Task ID: {celery_task_obj.id}, user: {current_user.username}).") # Enhanced logging

    return FileUploadResponse(job_id=job_id, filename=original_filename, message="File successfully uploaded, validated, and queued for processing.", staged_filepath=staged_file_path)

@app.post("/metadata/ingestion_jobs", response_model=IngestionJobMetadata, status_code=status.HTTP_201_CREATED, summary="Create Ingestion Job Metadata (Manual)", tags=["Metadata"])
async def create_ingestion_job_metadata_manual(metadata_in: IngestionJobMetadata, db: Session = Depends(database.get_db), current_user: User = Depends(get_current_user)):
    db_job = crud.get_ingestion_job(db, job_id=metadata_in.job_id)
    if db_job: raise HTTPException(status_code=400, detail=f"Job with ID {metadata_in.job_id} already exists.")
    job_data = metadata_in.dict();
    if 'job_id' not in job_data or not job_data['job_id']: job_data['job_id'] = str(uuid.uuid4())
    created_job_db = crud.create_ingestion_job(db=db, job_data=job_data)
    return IngestionJobMetadata.from_orm(created_job_db)

@app.get("/metadata/ingestion_jobs/{job_id}", response_model=IngestionJobMetadata, summary="Get Ingestion Job Metadata by ID", tags=["Metadata"])
async def get_ingestion_job_metadata_by_id(job_id: str, db: Session = Depends(database.get_db), current_user: User = Depends(get_current_user)):
    db_job = crud.get_ingestion_job(db, job_id=job_id)
    if db_job is None: raise HTTPException(status_code=404, detail=f"Ingestion job with ID {job_id} not found.")
    return IngestionJobMetadata.from_orm(db_job)

@app.get("/metadata/ingestion_jobs", response_model=List[IngestionJobMetadata], summary="List Ingestion Job Metadata", tags=["Metadata"])
async def list_ingestion_job_metadata(skip: int = 0, limit: int = 10, db: Session = Depends(database.get_db), current_user: User = Depends(get_current_user)):
    jobs_db = crud.get_ingestion_jobs(db, skip=skip, limit=limit)
    return [IngestionJobMetadata.from_orm(job) for job in jobs_db]

@app.put("/metadata/ingestion_jobs/{job_id}", response_model=IngestionJobMetadata, summary="Update Ingestion Job Metadata", tags=["Metadata"])
async def update_ingestion_job_metadata_endpoint(job_id: str, job_update_data: IngestionJobMetadata, db: Session = Depends(database.get_db), current_user: User = Depends(get_current_user)):
    update_data_dict = job_update_data.dict(exclude_unset=True)
    if 'job_id' in update_data_dict and update_data_dict['job_id'] != job_id:
        logger.warning(f"job_id in path ({job_id}) and body ({update_data_dict['job_id']}) differ. Using path job_id.")
    update_data_dict['job_id'] = job_id
    updated_job_db = crud.update_ingestion_job_fields(db, job_id=job_id, fields_to_update=update_data_dict)
    if updated_job_db is None: raise HTTPException(status_code=404, detail=f"Ingestion job with ID {job_id} not found.")
    return IngestionJobMetadata.from_orm(updated_job_db)

@app.post("/indexes", response_model=IndexMetadata, status_code=status.HTTP_202_ACCEPTED, summary="Create New Spatial Index", tags=["Indexes"])
async def create_new_index(request: IndexCreationRequest, current_user: User = Depends(get_current_user)):
    if not user_has_permission(current_user, "create_index"): raise HTTPException(status_code=status.HTTP_403_FORBIDDEN, detail="User does not have permission to create indexes.")
    if not os.path.exists(request.promoted_data_path): raise HTTPException(status_code=status.HTTP_400_BAD_REQUEST, detail=f"Promoted data path does not exist: {request.promoted_data_path}")
    new_index_id = str(uuid.uuid4())
    index_meta = IndexMetadata(index_id=new_index_id, index_name=request.index_name, index_type=request.index_type, promoted_data_path=request.promoted_data_path, status="creating", version=1, created_at=datetime.utcnow())
    celery_task_obj = create_spindex_task.delay(index_id=new_index_id, promoted_data_path=request.promoted_data_path, index_name=request.index_name, index_type=request.index_type, index_parameters=request.index_parameters)
    index_meta.celery_task_id = str(celery_task_obj.id); index_metadata_db.append(index_meta); return index_meta

@app.get("/indexes", response_model=List[IndexMetadata], summary="List All Spatial Indexes", tags=["Indexes"])
async def list_all_indexes(skip: int = 0, limit: int = 10, current_user: User = Depends(get_current_user)): return index_metadata_db[skip : skip + limit]

@app.get("/indexes/{index_id}", response_model=IndexMetadata, summary="Get Spatial Index Metadata by ID", tags=["Indexes"])
async def get_index_metadata_by_id(index_id: str, current_user: User = Depends(get_current_user)):
    index_meta = next((idx for idx in index_metadata_db if idx.index_id == index_id), None)
    if not index_meta: raise HTTPException(status_code=status.HTTP_404_NOT_FOUND, detail=f"Index with ID {index_id} not found.")
    return index_meta

@app.delete("/indexes/{index_id}", status_code=status.HTTP_200_OK, summary="Delete Spatial Index", tags=["Indexes"])
async def delete_spatial_index(index_id: str, current_user: User = Depends(get_current_user)):
    if not user_has_permission(current_user, "delete_index"): raise HTTPException(status_code=status.HTTP_403_FORBIDDEN, detail="User does not have permission to delete indexes.")
    global index_metadata_db; original_len = len(index_metadata_db)
    index_metadata_db = [idx for idx in index_metadata_db if idx.index_id != index_id]
    if len(index_metadata_db) == original_len: raise HTTPException(status_code=status.HTTP_404_NOT_FOUND, detail=f"Index with ID {index_id} not found for deletion.")
    return {"message": f"Index with ID {index_id} deleted successfully (metadata only)."}

@app.post("/query/execute", response_model=QueryResponse, summary="Execute Spatial Query", tags=["Query"])
async def execute_query(query_request: QueryRequest, current_user: User = Depends(get_current_user)):
    if not user_has_permission(current_user, "query_data"): raise HTTPException(status_code=status.HTTP_403_FORBIDDEN, detail="User does not have permission to execute queries.")
    target_index = next((idx for idx in index_metadata_db if idx.index_id == query_request.index_id), None)
    if not target_index: raise HTTPException(status_code=status.HTTP_404_NOT_FOUND, detail=f"Index with ID {query_request.index_id} not found.")
    if target_index.status != "ready": raise HTTPException(status_code=status.HTTP_400_BAD_REQUEST, detail=f"Index {query_request.index_id} is not in 'ready' state (current: {target_index.status}).")
    history_item = QueryHistoryItem(user_id=current_user.username, request_data=query_request)
    query_history_db.insert(0, history_item)
    logger.info(f"Simulating query on index {target_index.index_name} (ID: {target_index.index_id}) with params: {query_request.parameters}")
    start_time = time.time(); time.sleep(0.1); end_time = time.time(); query_time_ms = int((end_time - start_time) * 1000)
    dummy_results = []
    if query_request.query_type == "bbox": dummy_results = [{'type': 'Feature', 'geometry': {'type': 'Polygon', 'coordinates': [[[0,0], [1,0], [1,1], [0,1], [0,0]]]}, 'properties': {'id': 1, 'name': 'Dummy BBox Result'}}]
    elif query_request.query_type == "point_radius" or query_request.query_type == "knn": dummy_results = [{'type': 'Feature', 'geometry': {'type': 'Point', 'coordinates': query_request.parameters.get("point", [0,0])}, 'properties': {'id': 2, 'name': 'Dummy Point Result'}}]
    return QueryResponse(results=dummy_results, count=len(dummy_results), query_time_ms=query_time_ms, message="Query executed successfully (simulated).")

@app.get("/query/history", response_model=List[QueryHistoryItem], summary="Get Query History", tags=["Query"])
async def get_query_history(skip: int = 0, limit: int = 20, current_user: User = Depends(get_current_user)):
    user_history = query_history_db
    return user_history[skip : skip + limit]


if __name__ == "__main__":
    import uvicorn
    logger.info("Starting Uvicorn server for SPINDEX Backend.")
    uvicorn.run(app, host="0.0.0.0", port=8000)

[end of backend/main.py]

[start of backend/README.md]
# SPINDEX Backend

This directory contains the Python FastAPI backend application for SPINDEX. It includes features for asynchronous task processing using Celery and Redis, a file upload endpoint for data ingestion, and a persistent metadata catalogue for tracking ingestion jobs using SQLAlchemy and SQLite.

## Prerequisites

*   Docker (for containerized deployment)
*   Python 3.11+ (for local development)
*   `pip` (Python package installer)
*   Redis server (for local development)
*   **GDAL (Geospatial Data Abstraction Library):** System libraries and Python bindings.
*   **GEOS:** System library required by Shapely.
*   **SQLAlchemy, Alembic:** For database ORM and migrations.

## Application Structure

*   `main.py`: The main FastAPI application file. Contains API endpoints, Pydantic models, Celery app configuration, and task definitions including the AEGIS pipeline logic.
*   `database.py`: Configures the SQLAlchemy engine and session management.
*   `models.py`: Defines SQLAlchemy ORM models (e.g., `IngestionJobDB`).
*   `crud.py`: Contains CRUD functions for database model interactions.
*   `requirements.txt`: Python dependencies.
*   `Dockerfile`, `entrypoint.sh`: For Docker containerization.
*   `alembic.ini`, `alembic/`: Alembic migration configuration and scripts.
*   `spindex_aegis.db`: SQLite database file (created by Alembic).
*   `staging_area/`: Temporary storage for raw uploaded files.
*   `processed_area/`: Stores files processed by the AEGIS pipeline (e.g., initial GeoJSON conversion, CRS transformed GeoJSON).

**Note on Directory Permissions:** (As previously stated)

## Database Setup (SQLite, SQLAlchemy, Alembic)

(As previously stated)

## Asynchronous Task Processing (Celery & Redis)

(As previously stated)

### Ingestion Task Triggering & AEGIS Pipeline Execution Flow

The `process_staged_file_task` Celery task orchestrates the AEGIS-ECHO pipeline:

1.  **Initial Status:** Metadata status set to `"processing_in_progress"`.
2.  **Format Conversion to GeoJSON:**
    *   The `convert_to_geojson` internal function uses GDAL to convert the uploaded file from `staging_area/` to a standardized GeoJSON format (e.g., `{job_id}_converted.geojson`) in `processed_area/`.
    *   Detects and stores the source format. Original CRS WKT is also extracted if available.
3.  **Coordinate Reference System (CRS) Transformation:**
    *   The `transform_geojson_crs` internal function processes the GeoJSON file from the previous step.
    *   It uses GDAL to transform its content to the system's standard CRS (EPSG:4326 - WGS 84).
    *   A new CRS-transformed GeoJSON file (e.g., `{job_id}_crs_transformed.geojson`) is saved in `processed_area/`. This becomes the primary file for subsequent steps.
    *   `original_crs` (verified) and `target_crs` fields in metadata are updated.
4.  **Geometric Validation:**
    *   The `validate_geometries` internal function processes the CRS-transformed GeoJSON file.
    *   It uses Shapely to check each geometry's validity (`is_valid`).
    *   Currently, it does not attempt to repair invalid geometries but counts valid and invalid features.
    *   The validation summary (counts, pass/fail status) is updated in the job's metadata. If invalid geometries are found, `geometric_validation_passed` flag is set to false, which will lead to quarantine.
5.  **Subsequent Pipeline Stages (currently placeholders or simplified):**
    *   Conditional Geocoding.
    *   Complex Geometry Handling.
    *   Placeholders for Content-Level Security Scanning and Schema & Value-Domain Validation.
6.  **Metadata Updates:** Each step logs its outcome to `pipeline_stages_executed_log` and updates relevant fields in the `IngestionJobDB` record.
7.  **Final Status:** Based on the success of all critical stages, the job status is set to `"validation_successful_ready_for_promotion"` or a relevant failure/quarantine status.

This modular, step-by-step processing within the Celery task ensures that each stage's output can be inspected and that failures are handled at the point they occur.

## Development

(As previously stated)

### Docker Deployment

(As previously stated)

## API Endpoints

(As previously stated)

### Data Upload Endpoint:

*   **POST `/upload/data`**:
    *   (Details as previously documented. The Celery task it triggers now includes distinct steps for format conversion, CRS transformation, and geometric validation.)

(Rest of the README remains the same)

[end of backend/README.md]

[start of docs/AEGIS_PIPELINE_DESIGN.md]
# AEGIS-ECHO Data Ingestion Pipeline Design

## 1. Overview

*   **Purpose:** This document details the architecture and design of the AEGIS-ECHO Data Ingestion Pipeline for SPINDEX. The primary goal of this pipeline is to provide a secure, resilient, and extensible mechanism for ingesting heterogeneous, unstructured, and potentially malicious geospatial data from various sources into the SPINDEX system. It aims to abstract the complexities of data handling, ensuring that only validated and standardized data is made available for indexing.
*   **Scope:** Covers the pipeline from data reception to its promotion for indexing or quarantine.
*   **Guiding Principles:** Security by design, resilience, modularity, extensibility, detailed metadata tracking.

## 2. Pipeline Architecture

**(Placeholder for a diagram illustrating the stages and data flow that can be added later)**

### 2.1. Pipeline Stages

(Pipeline Stages 1-7 remain as previously defined, with Metadata Cataloguing using a persistent database)
*   **Stage 1: Secure Upload/Reception Endpoint**
*   **Stage 2: Staging Area**
*   **Stage 3: Ingestion Task Trigger**
*   **Stage 4: Pipeline Worker Execution**
*   **Stage 5: Data Promotion**
    *   **Responsibilities:** Move validated and processed data to a trusted location for subsequent indexing by SPINDEX.
    *   **Process:**
        *   If all critical pipeline modules (Format Conversion, CRS Transformation, Geometric Validation, and eventually Security/Schema checks) execute successfully:
            1.  The `IngestionJobDB.processing_status` is updated to `"validation_successful_ready_for_promotion"`.
            2.  The `IngestionJobDB.location_of_promoted_data` is set to the path of the final, validated file (e.g., the CRS-transformed GeoJSON, or a geometry-repaired GeoJSON if that step modifies the file).
            3.  `IngestionJobDB.promotion_timestamp` is set to the current time.
            4.  A log entry indicating successful promotion is added to `pipeline_stages_executed_log`.
*   **Stage 6: Data Quarantine (DLQ)**
    *   (As previously defined)
*   **Stage 7: Metadata Cataloguing**
    *   (As previously defined)

## 3. Core Pluggable Modules (Executed within `process_staged_file_task`)

*   **3.1. Format Conversion & Abstraction (GDAL/OGR Integration)**
    *   (As previously defined)
*   **3.2. Coordinate Reference System (CRS) Transformation**
    *   (As previously defined)
*   **3.3. Geometric Validation**
    *   (As previously defined)
*   **3.4. Geocoding Service Integration**
    *   (As previously defined)
*   **3.5. Complex Geometry Handling**
    *   (As previously defined)
*   **3.6. Unstructured Text Parsing (NLP/NER)**
    *   (As previously defined)

## 4. Security: Validation & Sanitization Mandates

*   **4.1. Geometric Validation:** (Covered in section 3.3)
*   **4.2. Content-Level Security Scanning**
    *   (As previously defined)
*   **4.3. Value-Domain & Schema Validation**
    *   (As previously defined)

## 5. UI Integration

(As previously stated)

## 6. Metadata Cataloguing (Reiteration from Stage 7)

(As previously stated)

[end of docs/AEGIS_PIPELINE_DESIGN.md]
