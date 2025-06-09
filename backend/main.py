from fastapi import FastAPI, Depends, HTTPException, status, File, UploadFile
from fastapi.security import OAuth2PasswordBearer, APIKeyHeader
from pydantic import BaseModel, Field as PydanticField
from typing import List, Optional, Dict, Any, Union
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
from collections import defaultdict
import spacy # Added for NER
import zipfile # Added for Zip Bomb check

from sqlalchemy.orm import Session
from backend import database, models, crud

# --- Constants ---
STAGING_AREA_PATH = "staging_area"
PROCESSED_AREA_PATH = "processed_area"
TARGET_CRS_EPSG = 4326
EXPECTED_INPUT_FORMATS = [
    "ESRI Shapefile", "GeoJSON", "KML", "GML", "GPKG", "FlatGeobuf"
]
MAX_FILE_SIZE = 100 * 1024 * 1024
ALLOWED_MIME_TYPES = [
    "application/geo+json", "application/json", "application/zip",
    "application/vnd.google-earth.kml+xml", "application/gml+xml",
    "text/xml", "application/octet-stream"
]

logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(name)s - %(levelname)s - %(message)s')
logger = logging.getLogger(__name__)

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

dummy_admin_role = Role(name="Admin", permissions=["create_index", "delete_index", "query_data", "manage_users", "upload_data"])
dummy_analyst_role = Role(name="Analyst", permissions=["create_index", "query_data", "upload_data"])
dummy_viewer_role = Role(name="Viewer", permissions=["query_data"])
dummy_users_db = {
    "admin": User(username="admin", roles=[dummy_admin_role]),
    "analyst": User(username="analyst", roles=[dummy_analyst_role]),
    "viewer": User(username="viewer", roles=[dummy_viewer_role]),
}
X_AUTH_TOKEN_SCHEME = APIKeyHeader(name="X-Auth-Token", auto_error=False)
app = FastAPI(title="SPINDEX Backend", version="0.1.0")
SECURE_PLACEHOLDER_TOKEN_ADMIN = "secure-placeholder-token-admin"
SECURE_PLACEHOLDER_TOKEN_ANALYST = "secure-placeholder-token-analyst"
SECURE_PLACEHOLDER_TOKEN_VIEWER = "secure-placeholder-token-viewer"
TOKEN_TO_USER_MAPPING = {
    SECURE_PLACEHOLDER_TOKEN_ADMIN: "admin",
    SECURE_PLACEHOLDER_TOKEN_ANALYST: "analyst",
    SECURE_PLACEHOLDER_TOKEN_VIEWER: "viewer",
}

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

def user_has_permission(user: User, permission_needed: str) -> bool:
    for role in user.roles:
        if permission_needed in role.permissions: return True
    return False

# --- AEGIS Pipeline Helper Functions ---
# Note on XXE Prevention: GDAL/OGR is used for parsing XML-based formats (KML, GML).
# GDAL's default XML parsers (Expat, Xerces) are generally configured to prevent XXE attacks.
# If direct Python XML parsing is added elsewhere, use 'defusedxml' or ensure lxml is configured safely.
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
    logger.info(f"{log_prefix} Starting geocoding for {source_geojson_filepath}")
    geolocator = Nominatim(user_agent=f"spindex_aegis_pipeline/0.1 job_id/{job_id}")
    address_field_config = {
        "street": ["street", "address", "addr", "street_address"], "city": ["city", "town", "municipality"],
        "state": ["state", "province", "region"], "country": ["country", "country_code"],
        "postalcode": ["postal_code", "zipcode", "zip_code", "postcode"],
        "full_address": ["full_address", "address_line", "location", "beschreibung"]
    }
    geocoded_filename = f"{job_id}_geocoded.geojson"
    geocoded_filepath = os.path.join(output_dir, geocoded_filename)
    summary = {
        "output_filepath": None, "features_processed": 0, "geocoded_successfully": 0,
        "geocoding_failed_no_match": 0, "geocoding_service_error": 0, "skipped_no_address_fields": 0, "errors_encountered": []
    }
    output_features = []
    try:
        with open(source_geojson_filepath, 'r', encoding='utf-8') as f: geojson_data = json.load(f)
        if not isinstance(geojson_data, dict) or geojson_data.get("type") != "FeatureCollection" or not isinstance(geojson_data.get("features"), list):
            err_msg = "Input for geocoding is not a valid GeoJSON FeatureCollection."; logger.error(f"{log_prefix} {err_msg}")
            summary["errors_encountered"].append(err_msg); summary["output_filepath"] = source_geojson_filepath; return summary
        features = geojson_data["features"]; summary["features_processed"] = len(features)
        for i, feature in enumerate(features):
            properties = feature.get("properties", {}); query_string = None
            if not properties: summary["skipped_no_address_fields"] += 1; output_features.append(feature); continue
            props_lower = {k.lower(): v for k, v in properties.items()}; address_found = False
            for field_variant in address_field_config["full_address"]:
                if field_variant.lower() in props_lower and isinstance(props_lower[field_variant.lower()], str) and props_lower[field_variant.lower()].strip():
                    query_string = props_lower[field_variant.lower()].strip(); address_found = True; break
            if not address_found:
                query_parts = {}
                for part_key, common_names in address_field_config.items():
                    if part_key == "full_address": continue
                    for name_variant in common_names:
                        if name_variant.lower() in props_lower and isinstance(props_lower[name_variant.lower()], str) and props_lower[name_variant.lower()].strip():
                            query_parts[part_key] = props_lower[name_variant.lower()].strip(); break
                if query_parts: query_string = ", ".join(filter(None, [query_parts.get(p) for p in ["street", "city", "postalcode", "state", "country"]]))
            if not query_string: summary["skipped_no_address_fields"] += 1; output_features.append(feature); continue
            logger.info(f"{log_prefix} Attempting to geocode address for feature {i}: '{query_string}'")
            try:
                time.sleep(1.1); location = geolocator.geocode(query_string, timeout=10)
                if location:
                    logger.info(f"{log_prefix} Geocoded feature {i} to: {location.address} ({location.latitude}, {location.longitude})")
                    original_geometry = feature.get("geometry")
                    feature["geometry"] = {"type": "Point", "coordinates": [location.longitude, location.latitude]}
                    properties["geocoding_service"] = "Nominatim"; properties["geocoded_address"] = location.address
                    properties["original_geometry_type"] = original_geometry.get("type") if original_geometry else "Unknown/None"
                    feature["properties"] = properties; summary["geocoded_successfully"] += 1
                else: logger.warning(f"{log_prefix} Geocoding failed (no match) for feature {i}, address: '{query_string}'"); summary["geocoding_failed_no_match"] += 1
            except GeocoderTimedOut: logger.error(f"{log_prefix} Geocoding service timed out for feature {i}, address: '{query_string}'"); summary["geocoding_service_error"] += 1; summary["errors_encountered"].append(f"GeocoderTimedOut for feature {i}: {query_string}")
            except (GeocoderUnavailable, GeocoderServiceError) as e_geo: logger.error(f"{log_prefix} Geocoding service error for feature {i}, address: '{query_string}': {str(e_geo)}"); summary["geocoding_service_error"] += 1; summary["errors_encountered"].append(f"GeocoderServiceError for feature {i} ({query_string}): {str(e_geo)}")
            except Exception as e_gen: logger.error(f"{log_prefix} Unexpected error during geocoding feature {i}, address: '{query_string}': {str(e_gen)}", exc_info=True); summary["geocoding_service_error"] += 1; summary["errors_encountered"].append(f"Unexpected error geocoding feature {i} ({query_string}): {str(e_gen)}")
            output_features.append(feature)
        updated_geojson_data = {"type": "FeatureCollection", "features": output_features, "crs": geojson_data.get("crs")}
        if not os.path.exists(output_dir): os.makedirs(output_dir)
        with open(geocoded_filepath, 'w', encoding='utf-8') as f: json.dump(updated_geojson_data, f, ensure_ascii=False, indent=2)
        summary["output_filepath"] = geocoded_filepath
        logger.info(f"{log_prefix} Geocoding process complete. Output: {geocoded_filepath}. Summary: {summary}")
    except FileNotFoundError: err_msg = f"Source GeoJSON file not found: {source_geojson_filepath}"; logger.error(f"{log_prefix} {err_msg}"); summary["errors_encountered"].append(err_msg); summary["output_filepath"] = source_geojson_filepath; summary["status"] = "error_source_not_found"
    except json.JSONDecodeError as e_json: err_msg = f"Invalid JSON in source file {source_geojson_filepath}: {str(e_json)}"; logger.error(f"{log_prefix} {err_msg}"); summary["errors_encountered"].append(err_msg); summary["output_filepath"] = source_geojson_filepath; summary["status"] = "error_invalid_input_geojson"
    except Exception as e_main: err_msg = f"Critical error in geocode_features: {str(e_main)}"; logger.error(f"{log_prefix} {err_msg}", exc_info=True); summary["errors_encountered"].append(err_msg); summary["output_filepath"] = source_geojson_filepath; summary["status"] = "error_critical_failure"
    return summary

# --- NER and Geocoding from Text Helper Function ---
def extract_and_geocode_locations_from_text(source_geojson_filepath: str, output_dir: str, job_id: str, text_property_fields: List[str], log_prefix: str) -> Dict[str, Any]:
    logger.info(f"{log_prefix} Starting NER and geocoding from text for {source_geojson_filepath}")
    nlp = None
    try:
        nlp = spacy.load("en_core_web_sm")
        logger.info(f"{log_prefix} spaCy model 'en_core_web_sm' loaded successfully.")
    except (ImportError, OSError) as e_spacy_load:
        logger.error(f"{log_prefix} Failed to load spaCy model 'en_core_web_sm'. NER processing will be skipped. Error: {e_spacy_load}")
        return {
            "status": "skipped_spacy_model_unavailable", "output_filepath": source_geojson_filepath,
            "features_processed": 0, "features_with_text_fields":0, "features_missing_geometry": 0,
            "locations_extracted_ner": 0, "locations_geocoded_successfully": 0,
            "geocoding_failures_no_match": 0, "geocoding_service_errors": 0,
            "errors_encountered": [f"spaCy model 'en_core_web_sm' not available: {e_spacy_load}"]
        }
    geolocator = Nominatim(user_agent=f"spindex_aegis_ner_geocoder/0.1 job_id/{job_id}")
    ner_geocoded_filename = f"{job_id}_ner_geocoded.geojson"
    ner_geocoded_filepath = os.path.join(output_dir, ner_geocoded_filename)
    summary = {
        "status": "completed", "output_filepath": None, "features_processed": 0,
        "features_with_text_fields": 0, "features_missing_geometry": 0, "locations_extracted_ner": 0,
        "locations_geocoded_successfully": 0, "geocoding_failures_no_match": 0,
        "geocoding_service_errors": 0, "errors_encountered": []
    }
    output_features = []
    try:
        with open(source_geojson_filepath, 'r', encoding='utf-8') as f: geojson_data = json.load(f)
        if not isinstance(geojson_data, dict) or geojson_data.get("type") != "FeatureCollection" or not isinstance(geojson_data.get("features"), list):
            err_msg = "Input for NER geocoding is not a valid GeoJSON FeatureCollection."
            logger.error(f"{log_prefix} {err_msg}"); summary["errors_encountered"].append(err_msg)
            summary["status"] = "error_invalid_input_geojson"; summary["output_filepath"] = source_geojson_filepath; return summary
        features = geojson_data["features"]; summary["features_processed"] = len(features)
        for i, feature in enumerate(features):
            properties = feature.get("properties", {})
            current_geometry = feature.get("geometry"); has_valid_geometry = False
            if current_geometry:
                coords = current_geometry.get("coordinates"); geoms = current_geometry.get("geometries")
                if (isinstance(coords, list) and len(coords) > 0 and any(c is not None for c in coords)) or \
                   (isinstance(geoms, list) and len(geoms) > 0): has_valid_geometry = True
            if has_valid_geometry: output_features.append(feature); continue
            summary["features_missing_geometry"] += 1
            processed_feature_for_text = False
            if properties is None: properties = {}
            for field_name in text_property_fields:
                if field_name in properties and isinstance(properties[field_name], str) and properties[field_name].strip():
                    summary["features_with_text_fields"] += 1; text_content = properties[field_name].strip()
                    logger.debug(f"{log_prefix} Processing NER for feature {i}, field '{field_name}': '{text_content[:100]}...'")
                    doc = nlp(text_content)
                    found_loc_entities = [ent.text for ent in doc.ents if ent.label_ in ("GPE", "LOC")]
                    if found_loc_entities:
                        summary["locations_extracted_ner"] += len(found_loc_entities)
                        for loc_name in found_loc_entities:
                            if len(loc_name.strip()) < 3 or loc_name.strip().lower() in ["location", "city", "country", "place", "unknown", "n/a", "-"]: continue
                            logger.info(f"{log_prefix} Attempting to geocode extracted NER location for feature {i}: '{loc_name}' from field '{field_name}'")
                            try:
                                time.sleep(1.1); location_result = geolocator.geocode(loc_name, timeout=10)
                                if location_result:
                                    logger.info(f"{log_prefix} NER Geocoded feature {i} (entity: '{loc_name}') to: {location_result.address} ({location_result.latitude}, {location_result.longitude})")
                                    feature["geometry"] = {"type": "Point", "coordinates": [location_result.longitude, location_result.latitude]}
                                    if not isinstance(properties, dict): properties = {}
                                    properties["ner_source_field"] = field_name; properties["ner_extracted_location"] = loc_name
                                    properties["ner_geocoded_address"] = location_result.address; feature["properties"] = properties
                                    summary["locations_geocoded_successfully"] += 1; processed_feature_for_text = True; break
                                else: summary["geocoding_failures_no_match"] += 1
                            except GeocoderTimedOut: logger.error(f"{log_prefix} NER Geocoding service timed out for entity '{loc_name}' (feature {i})"); summary["geocoding_service_errors"] += 1
                            except (GeocoderUnavailable, GeocoderServiceError) as e_geo_ner: logger.error(f"{log_prefix} NER Geocoding service error for entity '{loc_name}' (feature {i}): {str(e_geo_ner)}"); summary["geocoding_service_errors"] += 1
                            if processed_feature_for_text: break
                        if processed_feature_for_text: break
            output_features.append(feature)
        updated_geojson_data = {"type": "FeatureCollection", "features": output_features, "crs": geojson_data.get("crs")}
        if not os.path.exists(output_dir): os.makedirs(output_dir)
        with open(ner_geocoded_filepath, 'w', encoding='utf-8') as f: json.dump(updated_geojson_data, f, ensure_ascii=False, indent=2)
        summary["output_filepath"] = ner_geocoded_filepath
        logger.info(f"{log_prefix} NER and Geocoding from text process complete. Output: {ner_geocoded_filepath}. Summary: {summary}")
    except FileNotFoundError: err_msg = f"Source GeoJSON file not found for NER: {source_geojson_filepath}"; logger.error(f"{log_prefix} {err_msg}"); summary["errors_encountered"].append(err_msg); summary["status"] = "error_source_not_found"; summary["output_filepath"] = source_geojson_filepath
    except json.JSONDecodeError as e_json: err_msg = f"Invalid JSON in source file {source_geojson_filepath} for NER: {str(e_json)}"; logger.error(f"{log_prefix} {err_msg}"); summary["errors_encountered"].append(err_msg); summary["status"] = "error_invalid_input_geojson"; summary["output_filepath"] = source_geojson_filepath
    except Exception as e_main: err_msg = f"Critical error in extract_and_geocode_locations_from_text: {str(e_main)}"; logger.error(f"{log_prefix} {err_msg}", exc_info=True); summary["errors_encountered"].append(err_msg); summary["status"] = "error_critical_failure"; summary["output_filepath"] = source_geojson_filepath
    return summary

# --- Complex Geometry Handling Helper Function ---
def handle_complex_geometries(source_geojson_filepath: str, output_dir: str, job_id: str, log_prefix: str) -> Dict[str, Any]:
    logger.info(f"{log_prefix} Starting complex geometry handling for {source_geojson_filepath}")
    features_by_type = defaultdict(list)
    summary = {
        "status": "no_action_single_type", "output_filepaths": [source_geojson_filepath],
        "geometry_types_found": [], "features_counts_by_type": {}, "files_created": {}, "errors_encountered": []
    }
    all_geom_types = set()
    try:
        with open(source_geojson_filepath, 'r', encoding='utf-8') as f: geojson_data = json.load(f)
        if not isinstance(geojson_data, dict) or geojson_data.get("type") != "FeatureCollection" or not isinstance(geojson_data.get("features"), list):
            err_msg = "Input for complex geometry handling is not a valid GeoJSON FeatureCollection."
            logger.error(f"{log_prefix} {err_msg}"); summary["errors_encountered"].append(err_msg)
            summary["status"] = "error_invalid_input_geojson"; summary["output_filepaths"] = []; return summary
        for feature in geojson_data["features"]:
            geom_type = "Unknown"
            if feature.get("geometry") and feature["geometry"].get("type"):
                try: s_geom = shape(feature['geometry']); geom_type = s_geom.geom_type
                except Exception as e_shape: logger.warning(f"{log_prefix} Could not parse geometry for a feature: {e_shape}. Marking as 'Unknown' type.")
            features_by_type[geom_type].append(feature); all_geom_types.add(geom_type)
        summary["geometry_types_found"] = list(all_geom_types)
        for geom_type, feats in features_by_type.items(): summary["features_counts_by_type"][geom_type] = len(feats)
        logger.info(f"{log_prefix} Found geometry types: {summary['geometry_types_found']} with counts: {summary['features_counts_by_type']}")
        if len(all_geom_types) > 1:
            logger.info(f"{log_prefix} Multiple geometry types found. Splitting dataset...")
            summary["status"] = "split_executed"; summary["output_filepaths"] = []
            if not os.path.exists(output_dir): os.makedirs(output_dir)
            for geom_type, features_list in features_by_type.items():
                if not features_list: continue
                safe_geom_type_fn = geom_type.replace(" ", "_").lower()
                split_filename = f"{job_id}_geomtype_{safe_geom_type_fn}.geojson"
                split_filepath = os.path.join(output_dir, split_filename)
                split_feature_collection = {"type": "FeatureCollection", "crs": geojson_data.get("crs"), "features": features_list}
                try:
                    with open(split_filepath, 'w', encoding='utf-8') as f_split: json.dump(split_feature_collection, f_split, ensure_ascii=False, indent=2)
                    summary["output_filepaths"].append(split_filepath); summary["files_created"][geom_type] = split_filepath
                    logger.info(f"{log_prefix} Created split file for type '{geom_type}': {split_filepath}")
                except Exception as e_write:
                    err_msg = f"Failed to write split file for geometry type '{geom_type}' to {split_filepath}: {e_write}"
                    logger.error(f"{log_prefix} {err_msg}", exc_info=True); summary["errors_encountered"].append(err_msg)
            if not summary["output_filepaths"] and summary["errors_encountered"]: summary["status"] = "error_split_failed_all_types"
        else:
            logger.info(f"{log_prefix} Single geometry type found ('{list(all_geom_types)[0] if all_geom_types else 'None'}') or no features. No splitting needed.")
            summary["output_filepaths"] = [source_geojson_filepath]
    except FileNotFoundError: err_msg = f"Source GeoJSON file not found for complex geometry handling: {source_geojson_filepath}"; logger.error(f"{log_prefix} {err_msg}"); summary["errors_encountered"].append(err_msg); summary["status"] = "error_source_not_found"; summary["output_filepaths"] = []
    except json.JSONDecodeError as e_json: err_msg = f"Invalid JSON in source file {source_geojson_filepath} for complex geometry handling: {str(e_json)}"; logger.error(f"{log_prefix} {err_msg}"); summary["errors_encountered"].append(err_msg); summary["status"] = "error_invalid_input_geojson"; summary["output_filepaths"] = []
    except Exception as e_main: err_msg = f"Critical error in handle_complex_geometries: {str(e_main)}"; logger.error(f"{log_prefix} {err_msg}", exc_info=True); summary["errors_encountered"].append(err_msg); summary["status"] = "error_critical_failure"; summary["output_filepaths"] = []
    logger.info(f"{log_prefix} Complex geometry handling complete. Summary: {summary}")
    return summary

# --- Celery Tasks ---
@shared_task(bind=True, acks_late=True) # bind=True for self, acks_late=True for potential retries
def process_staged_file_task(self, job_id_from_upload: str, staged_file_path: str, original_filename: str, user_id: str) -> str:
    task_id = self.request.id # Get task ID
    log_prefix = f"CeleryTaskID-{task_id} (JobID: {job_id_from_upload}, File: {original_filename}, User: {user_id}):"
    logger.info(f"{log_prefix} Received for processing. Staged path: '{staged_file_path}'.")
    db: Session = database.SessionLocal()

    content_security_passed = True
    content_security_summary_details = None
    format_conversion_success = False
    crs_transform_success = False
    geometric_validation_passed_flag = False
    geocoding_attempted = False
    geocoding_success = False
    ner_processing_attempted = False
    ner_processing_success = False
    complex_geom_handling_attempted = False
    complex_geom_handling_success = False
    schema_validation_passed = True # Placeholder
    content_security_scan_passed = True # Placeholder (distinct from pre-check)

    current_processed_file = staged_file_path
    final_promoted_data_references: Optional[Union[str, List[str]]] = None
    original_crs_wkt = None
    ner_summary_details = None # Initialize ner_summary_details

    try:
        crud.update_ingestion_job_fields(db, job_id_from_upload, {"processing_status": "processing_in_progress", "celery_task_id": str(task_id)})
        crud.update_pipeline_log(db, job_id_from_upload, {"stage": "initialization", "status": "success", "timestamp": datetime.utcnow().isoformat(), "message": "Processing started."})
        db.commit()

        # === Stage 0: Content Security Pre-Check (Zip Bomb) ===
        if original_filename.lower().endswith(".zip"):
            MAX_ZIP_TOTAL_SIZE = 5 * 1024**3  # 5GB
            MAX_ZIP_FILE_COUNT = 10000
            MAX_ZIP_RATIO = 200.0 # Max compression ratio for any single file

            logger.info(f"{log_prefix} Performing content security pre-check for zip file: {original_filename}")
            content_security_summary_details = check_zip_file_security(staged_file_path, MAX_ZIP_TOTAL_SIZE, MAX_ZIP_FILE_COUNT, MAX_ZIP_RATIO, log_prefix)
            crud.update_ingestion_job_fields(db, job_id_from_upload, {"content_security_summary": content_security_summary_details})
            db.commit() # Commit summary early

            if not content_security_summary_details["passed"]:
                content_security_passed = False
                reason = content_security_summary_details["reason"]
                logger.error(f"{log_prefix} Content security pre-check failed: {reason}")
                crud.update_error_logs(db, job_id_from_upload, f"content_security_failed: {reason}")
                crud.update_pipeline_log(db, job_id_from_upload, {"stage": "content_security_pre_check", "status": "failure", "error": reason, "timestamp": datetime.utcnow().isoformat()})
                crud.update_ingestion_job_fields(db, job_id_from_upload, {
                    "processing_status": "quarantined_content_security_failed",
                    "quarantine_timestamp": datetime.utcnow(),
                    "location_of_quarantined_data": staged_file_path
                })
                db.commit()
                return f"Content security pre-check failed for {original_filename}: {reason}"
            else:
                 crud.update_pipeline_log(db, job_id_from_upload, {"stage": "content_security_pre_check", "status": "success", "timestamp": datetime.utcnow().isoformat(), "details": content_security_summary_details})
                 db.commit()
        else:
            content_security_passed = True # Non-zip files pass this pre-check by default
            crud.update_pipeline_log(db, job_id_from_upload, {"stage": "content_security_pre_check", "status": "skipped_not_zip", "timestamp": datetime.utcnow().isoformat()})
            db.commit()


        # === Stage 1: Format Conversion to GeoJSON ===
        if content_security_passed: # Only proceed if pre-check passed
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
                    "processing_status": "format_conversion_failed", "quarantine_timestamp": datetime.utcnow(),
                    "location_of_quarantined_data": staged_file_path
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
                    "processing_status": "crs_transformation_failed", "quarantine_timestamp": datetime.utcnow(),
                    "location_of_quarantined_data": current_processed_file
                })
                db.commit(); return f"Error during CRS Transformation for '{original_filename}'."
            db.commit()

        # === Stage 3: Geometric Validation ===
        if crs_transform_success:
            try:
                crud.update_pipeline_log(db, job_id_from_upload, {"stage": "geometric_validation_started", "status": "running", "timestamp": datetime.utcnow().isoformat(), "input_file": current_processed_file})
                validation_results = validate_geometries(current_processed_file, job_id_from_upload, log_prefix, attempt_repair=True) # ENABLED REPAIR
                crud.update_validation_summary(db, job_id_from_upload, {"geometric_validation": validation_results})
                geometric_validation_passed_flag = validation_results["validation_passed"]
                current_processed_file = validation_results.get("output_filepath", current_processed_file) # Update if repair created new file

                log_status = "success_with_repairs" if (validation_results.get("repaired_features", 0) > 0 and geometric_validation_passed_flag) else \
                             ("success" if geometric_validation_passed_flag else "failure_with_invalids")
                crud.update_pipeline_log(db, job_id_from_upload, {"stage": "geometric_validation_completed", "status": log_status, "timestamp": datetime.utcnow().isoformat(), "details": validation_results})
                if not geometric_validation_passed_flag: logger.warning(f"{log_prefix} Geometric validation found invalid geometries not repairable. Invalid count: {validation_results['invalid_features'] - validation_results.get('repaired_features',0)}")
            except Exception as e:
                error_message = f"Geometric Validation failed critically: {str(e)}"; logger.error(f"{log_prefix} ERROR: {error_message}", exc_info=True)
                crud.update_error_logs(db, job_id_from_upload, f"geometric_validation_critical_error: {error_message}")
                crud.update_pipeline_log(db, job_id_from_upload, {"stage": "geometric_validation_critical_error", "status": "failure", "error": error_message, "timestamp": datetime.utcnow().isoformat()})
                crud.update_ingestion_job_fields(db, job_id_from_upload, {
                    "processing_status": "geometric_validation_critical_error", "quarantine_timestamp": datetime.utcnow(),
                    "location_of_quarantined_data": current_processed_file
                })
                geometric_validation_passed_flag = False
            db.commit()

        # === Stage 4: Conditional Geocoding (Structured Address) ===
        if geometric_validation_passed_flag:
            geocoding_attempted = False
            try: # Heuristic check
                with open(current_processed_file, 'r', encoding='utf-8') as f: temp_geojson_data = json.load(f)
                if temp_geojson_data.get("type") == "FeatureCollection" and temp_geojson_data.get("features") and temp_geojson_data["features"][0].get("properties"):
                    props_lower_keys = [key.lower() for key in temp_geojson_data["features"][0]["properties"].keys()]
                    address_heuristic_fields = ["address", "street", "addr", "city", "location", "beschreibung"]
                    if any(any(ah_field in prop_key for ah_field in address_heuristic_fields) for prop_key in props_lower_keys):
                        geocoding_attempted = True
                if not geocoding_attempted: logger.info(f"{log_prefix} No common address-like fields for structured geocoding. Skipping.")
            except Exception as e_heuristic: logger.warning(f"{log_prefix} Error during geocoding heuristic: {e_heuristic}. Skipping.", exc_info=True); geocoding_attempted = False

            if geocoding_attempted:
                try:
                    crud.update_pipeline_log(db, job_id_from_upload, {"stage": "structured_geocoding_started", "status": "running", "timestamp": datetime.utcnow().isoformat(), "input_file": current_processed_file})
                    geocoding_details = geocode_features(current_processed_file, PROCESSED_AREA_PATH, job_id_from_upload, log_prefix)
                    crud.update_ingestion_job_fields(db, job_id_from_upload, {"geocoding_summary": geocoding_details})
                    if geocoding_details.get("errors_encountered") or geocoding_details.get("output_filepath") is None:
                        geocoding_success = False; crud.update_error_logs(db, job_id_from_upload, f"structured_geocoding_error: {'; '.join(geocoding_details.get('errors_encountered',[]))}")
                    else: geocoding_success = True; current_processed_file = geocoding_details["output_filepath"]
                    log_status_geo = "success" if geocoding_success else ("critical_failure" if geocoding_details.get("output_filepath") is None else "completed_with_errors")
                    crud.update_pipeline_log(db, job_id_from_upload, {"stage": "structured_geocoding_completed", "status": log_status_geo, "timestamp": datetime.utcnow().isoformat(), "details": geocoding_details})
                except Exception as e:
                    error_message = f"Structured Geocoding stage failed critically: {str(e)}"; logger.error(f"{log_prefix} ERROR: {error_message}", exc_info=True)
                    crud.update_error_logs(db, job_id_from_upload, f"structured_geocoding_critical_error: {error_message}")
                    crud.update_pipeline_log(db, job_id_from_upload, {"stage": "structured_geocoding_critical_error", "status": "failure", "error": error_message, "timestamp": datetime.utcnow().isoformat()})
                    crud.update_ingestion_job_fields(db, job_id_from_upload, {"processing_status": "geocoding_failed_critical", "quarantine_timestamp": datetime.utcnow(), "location_of_quarantined_data": current_processed_file})
                    geocoding_success = False
                db.commit()
            else: geocoding_success = True # Skipped, not a failure
        else: geocoding_success = True # Skipped due to prior failure

        # === Stage 5: Unstructured Text Location Processing (NER) ===
        ner_summary_details = None
        if geometric_validation_passed_flag and (geocoding_success if geocoding_attempted else True):
            ner_processing_attempted = True
            try:
                crud.update_pipeline_log(db, job_id_from_upload, {"stage": "ner_text_processing_started", "status": "running", "timestamp": datetime.utcnow().isoformat(), "input_file": current_processed_file})
                text_fields_for_ner = ["description", "notes", "summary", "title", "text", "comments", "narrative"]
                ner_summary_details = extract_and_geocode_locations_from_text(current_processed_file, PROCESSED_AREA_PATH, job_id_from_upload, text_fields_for_ner, log_prefix)
                crud.update_ingestion_job_fields(db, job_id_from_upload, {"ner_summary": ner_summary_details})
                if ner_summary_details.get("status") == "skipped_spacy_model_unavailable":
                    ner_processing_success = True; log_status_ner = "skipped_model_unavailable"
                elif ner_summary_details.get("status", "").startswith("error_") or ner_summary_details.get("output_filepath") is None or \
                     (ner_summary_details.get("output_filepath") == current_processed_file and ner_summary_details.get("errors_encountered")):
                    ner_processing_success = False; crud.update_error_logs(db, job_id_from_upload, f"ner_text_processing_error: {'; '.join(ner_summary_details.get('errors_encountered',[]))}"); log_status_ner = "critical_failure"
                else:
                    ner_processing_success = True
                    if ner_summary_details.get("output_filepath") and ner_summary_details["output_filepath"] != current_processed_file:
                        current_processed_file = ner_summary_details["output_filepath"]
                    log_status_ner = "completed_with_errors" if ner_summary_details.get("errors_encountered") else "success"
                crud.update_pipeline_log(db, job_id_from_upload, {"stage": "ner_text_processing_completed", "status": log_status_ner, "timestamp": datetime.utcnow().isoformat(), "details": ner_summary_details})
            except Exception as e:
                error_message = f"NER Text Processing stage failed critically: {str(e)}"; logger.error(f"{log_prefix} ERROR: {error_message}", exc_info=True)
                crud.update_error_logs(db, job_id_from_upload, f"ner_text_processing_critical_error: {error_message}")
                crud.update_pipeline_log(db, job_id_from_upload, {"stage": "ner_text_processing_critical_error", "status": "failure", "error": error_message, "timestamp": datetime.utcnow().isoformat()})
                ner_processing_success = False
            db.commit()
        else: ner_processing_success = True

        # === Stage 6: Complex Geometry Handling ===
        complex_geom_details_summary = None
        prior_stages_ok_for_complex_geom = (geometric_validation_passed_flag and (geocoding_success if geocoding_attempted else True) and (ner_processing_success if ner_processing_attempted else True))
        if prior_stages_ok_for_complex_geom:
            complex_geom_handling_attempted = True
            try:
                crud.update_pipeline_log(db, job_id_from_upload, {"stage": "complex_geometry_handling_started", "status": "running", "timestamp": datetime.utcnow().isoformat(), "input_file": current_processed_file})
                complex_geom_details = handle_complex_geometries(current_processed_file, PROCESSED_AREA_PATH, job_id_from_upload, log_prefix)
                complex_geom_details_summary = complex_geom_details
                crud.update_ingestion_job_fields(db, job_id_from_upload, {"complex_geometry_summary": complex_geom_details})
                if complex_geom_details.get("status", "").startswith("error_") or complex_geom_details.get("errors_encountered"):
                    complex_geom_handling_success = False; crud.update_error_logs(db, job_id_from_upload, f"complex_geometry_handling_error: {'; '.join(complex_geom_details.get('errors_encountered',[]))}")
                    crud.update_ingestion_job_fields(db, job_id_from_upload, {"processing_status": f"complex_geom_handling_failed_critical", "quarantine_timestamp": datetime.utcnow(), "location_of_quarantined_data": current_processed_file})
                else:
                    complex_geom_handling_success = True
                    if complex_geom_details.get("output_filepaths"):
                        final_promoted_data_references = complex_geom_details["output_filepaths"]
                        if final_promoted_data_references: current_processed_file = final_promoted_data_references[0]
                log_status_complex = "success" if complex_geom_handling_success else ("critical_failure" if complex_geom_details.get("status","").startswith("error_") else "completed_with_errors")
                if complex_geom_details.get("status") == "split_executed" and complex_geom_handling_success: log_status_complex = "split_executed"
                crud.update_pipeline_log(db, job_id_from_upload, {"stage": "complex_geometry_handling_completed", "status": log_status_complex, "timestamp": datetime.utcnow().isoformat(), "details": complex_geom_details})
            except Exception as e:
                error_message = f"Complex Geometry Handling stage failed critically: {str(e)}"; logger.error(f"{log_prefix} ERROR: {error_message}", exc_info=True)
                crud.update_error_logs(db, job_id_from_upload, f"complex_geometry_handling_critical_error: {error_message}")
                crud.update_pipeline_log(db, job_id_from_upload, {"stage": "complex_geometry_handling_critical_error", "status": "failure", "error": error_message, "timestamp": datetime.utcnow().isoformat()})
                crud.update_ingestion_job_fields(db, job_id_from_upload, { "processing_status": "complex_geom_handling_failed_critical", "quarantine_timestamp": datetime.utcnow(), "location_of_quarantined_data": current_processed_file })
                complex_geom_handling_success = False
            db.commit()
        else: complex_geom_handling_success = True

        # === Final Status Update (Promotion/Quarantine Logic) ===
        final_status_fields = {}
        overall_success = (
            content_security_passed and
            format_conversion_success and
            crs_transform_success and
            geometric_validation_passed_flag and
            (geocoding_success if geocoding_attempted else True) and
            (ner_processing_success if ner_processing_attempted else True) and
            (complex_geom_handling_success if complex_geom_handling_attempted else True) and
            schema_validation_passed and # Placeholder
            content_security_scan_passed # Placeholder
        )

        if overall_success:
            final_status_fields["processing_status"] = "validation_successful_ready_for_promotion"
            final_status_fields["promotion_timestamp"] = datetime.utcnow()
            if final_promoted_data_references:
                if isinstance(final_promoted_data_references, list):
                    if len(final_promoted_data_references) == 1: final_status_fields["location_of_promoted_data"] = final_promoted_data_references[0]
                    else: final_status_fields["location_of_promoted_data"] = json.dumps(final_promoted_data_references)
                else: final_status_fields["location_of_promoted_data"] = final_promoted_data_references
            else: final_status_fields["location_of_promoted_data"] = current_processed_file
            result_message = f"File '{original_filename}' processed successfully. Promoted data at: {final_status_fields['location_of_promoted_data']}"; logger.info(f"{log_prefix} {result_message}")
        else:
            final_status_fields["processing_status"] = "quarantined_validation_failed"
            final_status_fields["quarantine_timestamp"] = datetime.utcnow()
            final_status_fields["location_of_quarantined_data"] = current_processed_file
            error_list = []
            if not content_security_passed: error_list.append("ContentSecurityPreCheckFailed")
            elif not format_conversion_success: error_list.append("FormatConversionFailed")
            elif not crs_transform_success: error_list.append("CRSTransformationFailed")
            elif not geometric_validation_passed_flag: error_list.append("GeometricValidationFailed")
            elif geocoding_attempted and not geocoding_success: error_list.append("GeocodingModuleFailedCritically")
            elif ner_processing_attempted and not ner_processing_success:
                if not (ner_summary_details and ner_summary_details.get("status") == "skipped_spacy_model_unavailable"):
                    error_list.append("NERTextProcessingFailedCritically")
            elif complex_geom_handling_attempted and not complex_geom_handling_success: error_list.append("ComplexGeomHandlingFailedCritically")
            error_detail = "; ".join(error_list) if error_list else "Unknown validation failure or non-critical processing issue."
            crud.update_error_logs(db, job_id_from_upload, f"overall_pipeline_failure_leading_to_quarantine: {error_detail}")
            result_message = f"File '{original_filename}' failed validation or a critical processing stage and was quarantined. Details: {error_detail}"; logger.warning(f"{log_prefix} {result_message}")

        crud.update_ingestion_job_fields(db, job_id_from_upload, final_status_fields)
        db.commit()
        return result_message

    except Exception as e:
        logger.error(f"{log_prefix} Unhandled exception in Celery task: {str(e)}", exc_info=True)
        if db.is_active: db.rollback()
        try:
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
    # Example of how a Celery task might be triggered (not directly used by AEGIS pipeline)
    # task = sample_celery_task.delay(payload.param_x, payload.param_y)
    # return {"task_id": task.id, "message": "Sample task submitted successfully."}
    raise HTTPException(status_code=status.HTTP_501_NOT_IMPLEMENTED, detail="Sample task endpoint not fully implemented.")


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
        logger.warning(f"Authorization failed: User '{current_user.username}' attempted to upload data without 'upload_data' permission.")
        raise HTTPException(status_code=status.HTTP_403_FORBIDDEN, detail="User does not have permission to upload data.")

    if not file.filename:
        logger.error(f"File upload attempt by user '{current_user.username}' failed: No filename provided.")
        raise HTTPException(status_code=status.HTTP_400_BAD_REQUEST, detail="No filename provided.")

    if file.size is None or file.size > MAX_FILE_SIZE:
        actual_size = file.size if file.size is not None else "unknown"
        logger.warning(f"File '{file.filename}' from user '{current_user.username}' rejected: size ({actual_size} bytes) exceeds MAX_FILE_SIZE ({MAX_FILE_SIZE} bytes).")
        raise HTTPException(status_code=status.HTTP_413_REQUEST_ENTITY_TOO_LARGE, detail=f"File size {actual_size} bytes exceeds maximum allowed size of {MAX_FILE_SIZE} bytes.")

    if file.content_type not in ALLOWED_MIME_TYPES:
        logger.warning(f"File '{file.filename}' from user '{current_user.username}' rejected: MIME type '{file.content_type}' not in ALLOWED_MIME_TYPES ({', '.join(ALLOWED_MIME_TYPES)}).")
        raise HTTPException(status_code=status.HTTP_415_UNSUPPORTED_MEDIA_TYPE, detail=f"File MIME type '{file.content_type}' is not allowed. Allowed types: {', '.join(ALLOWED_MIME_TYPES)}")

    logger.info(f"File '{file.filename}' (size: {file.size}, type: {file.content_type}) passed initial validation for user '{current_user.username}'.")

    os.makedirs(STAGING_AREA_PATH, exist_ok=True)
    original_filename = file.filename
    file_extension = os.path.splitext(original_filename)[1]
    unique_staged_filename = f"{uuid.uuid4()}{file_extension}"
    staged_file_path = os.path.join(STAGING_AREA_PATH, unique_staged_filename)

    try:
        with open(staged_file_path, "wb") as buffer: shutil.copyfileobj(file.file, buffer)
        logger.info(f"File '{original_filename}' from user '{current_user.username}' successfully saved to staging area: '{staged_file_path}'.")
    except Exception as e:
        logger.error(f"Could not save uploaded file '{original_filename}' from user '{current_user.username}' to staging area '{staged_file_path}': {str(e)}", exc_info=True)
        if os.path.exists(staged_file_path):
            try: os.remove(staged_file_path); logger.info(f"Successfully removed partially written file: {staged_file_path}")
            except Exception as rm_e: logger.error(f"Failed to remove partially written file {staged_file_path} after save error: {rm_e}", exc_info=True)
        raise HTTPException(status_code=status.HTTP_500_INTERNAL_SERVER_ERROR, detail=f"Could not save uploaded file. An internal error occurred.")
    finally:
        if file and hasattr(file, 'file') and not file.file.closed: file.file.close()

    job_id = str(uuid.uuid4())
    celery_task_obj = process_staged_file_task.delay(job_id, staged_file_path, original_filename, current_user.username)
    logger.info(f"Celery task '{celery_task_obj.id}' dispatched for file '{original_filename}' (user: {current_user.username}), associated with DB job_id '{job_id}'.")

    job_data_dict = {
        "job_id":job_id, "celery_task_id":str(celery_task_obj.id), "original_filename":original_filename,
        "upload_timestamp":datetime.utcnow(), "user_id":current_user.username, "source_type":"file_upload",
        "processing_status":"staged_and_queued", "staged_file_path":staged_file_path,
        "pipeline_stages_executed_log":[{"stage": "upload_complete_queued", "status": "success", "timestamp": datetime.utcnow().isoformat(), "detail": f"File received from user {current_user.username}"}]
    }
    crud.create_ingestion_job(db=db, job_data=job_data_dict)
    logger.info(f"Initial metadata record created in DB for job_id '{job_id}' (Celery Task ID: {celery_task_obj.id}, user: {current_user.username}).")

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
