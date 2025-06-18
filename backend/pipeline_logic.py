# backend/pipeline_logic.py
import os
import json
import logging
import time # For geocoding time.sleep
from datetime import datetime
import shutil # Not directly used in the moved logic, but often useful with file paths
import copy
from collections import defaultdict
from typing import List, Dict, Any, Optional

from sqlalchemy.orm import Session
from backend.models import IngestionJobDB
from backend.security_scanner import scan_file # For security scanning

# --- Data Processing & Scientific Computing Imports ---
from osgeo import gdal, ogr, osr # type: ignore
from shapely.geometry import shape, Point as ShapelyPoint, mapping as shapely_mapping # type: ignore
# from shapely.validation import is_valid, make_valid # Not used in the moved functions

# --- External Service Imports ---
from geopy.geocoders import Nominatim # type: ignore
from geopy.exc import GeocoderTimedOut, GeocoderUnavailable, GeocoderServiceError # type: ignore

# --- Constants (moved or redefined from main.py) ---
PROCESSED_AREA_PATH = os.getenv("PROCESSED_AREA_PATH", "processed_area") # Make configurable like others
TARGET_CRS_EPSG = 4326
NOMINATIM_USER_AGENT = "spindex-aegis-pipeline/0.1 (developer.contact@example.org)" # Consider making this part of a config object

# Security Scanner Configuration
SCANNER_MAX_TOTAL_UNCOMPRESSED_SIZE = int(os.getenv("SCANNER_MAX_TOTAL_UNCOMPRESSED_SIZE", str(1 * 1024 * 1024 * 1024))) # 1 GB
SCANNER_MAX_FILE_COUNT_IN_ZIP = int(os.getenv("SCANNER_MAX_FILE_COUNT_IN_ZIP", "10000"))
SCANNER_MAX_COMPRESSION_RATIO_SINGLE_FILE = float(os.getenv("SCANNER_MAX_COMPRESSION_RATIO_SINGLE_FILE", "200.0"))
SCANNER_MAX_COMPRESSION_RATIO_TOTAL = float(os.getenv("SCANNER_MAX_COMPRESSION_RATIO_TOTAL", "100.0"))
GEOCODING_ADDRESS_FIELDS: List[str] = [
    "address", "Address", "ADDRESS",
    "location", "Location", "LOCATION",
    "street_address", "full_address", "addr",
    "שדה כתובת"
]
COMPLEX_GEOMETRY_STRATEGY: str = os.getenv("COMPLEX_GEOMETRY_STRATEGY", "split_all")
CGH_FILTER_TYPES_TO_KEEP: List[str] = json.loads(os.getenv("CGH_FILTER_TYPES_TO_KEEP", '["Point", "LineString", "Polygon", "MultiPoint", "MultiLineString", "MultiPolygon"]'))
CGH_REJECT_IF_TYPES_FOUND: List[str] = json.loads(os.getenv("CGH_REJECT_IF_TYPES_FOUND", "[]"))


# Ensure PROCESSED_AREA_PATH exists when this module is loaded or used.
# This was previously in main.py's startup sequence.
# It's better if the main application ensures directories exist.
# For now, adding it here for completeness of moved logic.
os.makedirs(PROCESSED_AREA_PATH, exist_ok=True)


# --- Helper Functions (moved from main.py) ---

def geocode_and_update_features(features: list, address_fields: list, nominatim_user_agent: str, logger_instance: logging.Logger) -> tuple[list, dict]:
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
        current_geometry = feature.get("geometry")
        if current_geometry is not None and (not isinstance(current_geometry, dict) or current_geometry):
            continue
        summary["features_without_geometry_count"] += 1
        address_to_geocode = None
        for field_name in address_fields:
            potential_address = feature.get("properties", {}).get(field_name)
            if potential_address and isinstance(potential_address, str) and potential_address.strip():
                address_to_geocode = potential_address.strip()
                summary["addresses_found_for_geocoding"] += 1
                break
        if address_to_geocode:
            logger_instance.info(f"Attempting to geocode address: '{address_to_geocode}'")
            summary["geocoding_requests_sent"] += 1
            try:
                location = geolocator.geocode(address_to_geocode)
                time.sleep(1.0)
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
                time.sleep(1.0)
            except GeocoderUnavailable:
                summary["geocoding_service_errors"] += 1
                logger_instance.error(f"Geocoding service unavailable for address: '{address_to_geocode}'")
                time.sleep(1.0)
            except GeocoderServiceError as e_service:
                summary["geocoding_service_errors"] += 1
                logger_instance.error(f"Geocoding service error for address '{address_to_geocode}': {e_service}")
                time.sleep(1.0)
            except Exception as e_generic:
                summary["geocoding_unexpected_errors"] += 1
                logger_instance.error(f"Unexpected error during geocoding for address '{address_to_geocode}': {e_generic}")
        else:
            logger_instance.info(f"No address found in specified fields for feature {feature.get('id', '(no ID)')} to attempt geocoding.")
    return features, summary

def handle_complex_geometries(features: list, strategy: str, strategy_config: dict, logger_instance: logging.Logger) -> tuple[list, dict]:
    summary = {
        "strategy_applied": strategy, "strategy_config_used": strategy_config,
        "feature_count_before": 0, "feature_count_after": 0,
        "geometries_by_type_before": defaultdict(int), "geometries_by_type_after": defaultdict(int),
        "geometry_collections_exploded_count": 0, "multi_geometries_split_count": 0,
        "new_features_from_splits": 0, "features_filtered_out_count": 0,
        "job_failed_due_to_rejected_type": False, "unsupported_strategy_used": False,
        "malformed_geometries_encountered": 0,
    }
    summary['feature_count_before'] = len(features)
    for feature in features:
        geom_dict = feature.get('geometry')
        if geom_dict:
            try:
                geom = shape(geom_dict)
                summary['geometries_by_type_before'][geom.geom_type] += 1
            except Exception:
                summary['geometries_by_type_before']['Malformed/Invalid'] += 1
                summary['malformed_geometries_encountered'] += 1
        else:
            summary['geometries_by_type_before']['None'] += 1
    processed_features = []
    if strategy == "pass_through":
        processed_features = features
        logger_instance.info("Complex geometry strategy: pass_through.")
    elif strategy == "filter_types":
        filter_types_kept = strategy_config.get('filter_types_kept', [])
        logger_instance.info(f"Complex geometry strategy: filter_types. Keeping only: {filter_types_kept}")
        for feature in features:
            geom_dict = feature.get('geometry')
            if not geom_dict: processed_features.append(feature); continue
            try:
                geom = shape(geom_dict)
                if geom.geom_type in filter_types_kept: processed_features.append(feature)
                else: summary['features_filtered_out_count'] += 1
            except Exception: summary['features_filtered_out_count'] += 1; logger_instance.warning(f"Malformed geometry filtered: {feature.get('id', 'N/A')}")
    elif strategy == "reject_on_types":
        reject_if_types_found = strategy_config.get('reject_if_types_found', [])
        logger_instance.info(f"Complex geometry strategy: reject_on_types. Rejecting if any of: {reject_if_types_found} found.")
        for feature in features:
            geom_dict = feature.get('geometry')
            if not geom_dict: continue # Or append if features without geometry are allowed
            try:
                geom = shape(geom_dict)
                if geom.geom_type in reject_if_types_found:
                    summary['job_failed_due_to_rejected_type'] = True; logger_instance.error(f"Rejected type '{geom.geom_type}' found."); processed_features = []; break
                processed_features.append(feature)
            except Exception:
                if "Malformed/Invalid" in reject_if_types_found: summary['job_failed_due_to_rejected_type'] = True; logger_instance.error("Malformed geometry rejected."); processed_features = []; break
        if summary['job_failed_due_to_rejected_type']: processed_features = []
    elif strategy == "explode_collections":
        logger_instance.info("Complex geometry strategy: explode_collections.")
        for feature in features:
            geom_dict = feature.get('geometry')
            if not geom_dict: processed_features.append(feature); continue
            try:
                geom = shape(geom_dict)
                if geom.geom_type == "GeometryCollection":
                    summary['geometry_collections_exploded_count'] += 1
                    for i, sub_geom in enumerate(geom.geoms):
                        new_feature = copy.deepcopy(feature); new_feature['geometry'] = shapely_mapping(sub_geom)
                        if 'properties' not in new_feature: new_feature['properties'] = {}
                        new_feature['properties']['original_feature_id'] = feature.get('id', 'N/A')
                        processed_features.append(new_feature); summary['new_features_from_splits'] += 1
                else: processed_features.append(feature)
            except Exception: logger_instance.warning(f"Malformed geometry passed through: {feature.get('id', 'N/A')}"); processed_features.append(feature)
    elif strategy == "split_multi":
        logger_instance.info("Complex geometry strategy: split_multi.")
        multi_types = ["MultiPoint", "MultiLineString", "MultiPolygon"]
        for feature in features:
            geom_dict = feature.get('geometry')
            if not geom_dict: processed_features.append(feature); continue
            try:
                geom = shape(geom_dict)
                if geom.geom_type in multi_types:
                    summary['multi_geometries_split_count'] += 1
                    for i, sub_geom in enumerate(geom.geoms):
                        new_feature = copy.deepcopy(feature); new_feature['geometry'] = shapely_mapping(sub_geom)
                        if 'properties' not in new_feature: new_feature['properties'] = {}
                        new_feature['properties']['original_feature_id'] = feature.get('id', 'N/A')
                        processed_features.append(new_feature); summary['new_features_from_splits'] += 1
                else: processed_features.append(feature)
            except Exception: logger_instance.warning(f"Malformed geometry passed through: {feature.get('id', 'N/A')}"); processed_features.append(feature)
    else: # Unknown or unsupported strategy
        summary["unsupported_strategy_used"] = True; logger_instance.warning(f"Unsupported strategy: {strategy}. Passing through."); processed_features = features
    summary['feature_count_after'] = len(processed_features)
    for feature in processed_features:
        geom_dict = feature.get('geometry')
        if geom_dict:
            try: summary['geometries_by_type_after'][shape(geom_dict).geom_type] += 1
            except Exception: summary['geometries_by_type_after']['Malformed/Invalid'] += 1
        else: summary['geometries_by_type_after']['None'] += 1
    return processed_features, summary

# --- Main Pipeline Logic ---
def execute_aegis_pipeline(job_id: str, db: Session, logger_instance: logging.Logger) -> bool:
    # This is the core logic from process_ingestion_job_task
    logger_instance.info(f"Pipeline execution started for job_id: {job_id}")
    job = db.query(IngestionJobDB).filter(IngestionJobDB.job_id == job_id).first()

    if not job:
        logger_instance.error(f"Job ID {job_id} not found in the database for pipeline execution.")
        return False # Indicates failure or job not found

    current_log = list(job.pipeline_stages_executed_log or [])

    # --- Security Scan Stage ---
    logger_instance.info(f"Starting security scan for job {job_id}, file: {job.staged_file_path}")
    module_start_time = datetime.utcnow()
    current_log.append({
        "timestamp": module_start_time.isoformat(),
        "stage": "SecurityScan",
        "status": "started",
        "message": f"Initiated security scan for staged file: {job.staged_file_path}"
    })
    job.pipeline_stages_executed_log = current_log
    # db.commit() # Optional: commit start of stage

    scan_config = {
        "max_total_uncompressed_size": SCANNER_MAX_TOTAL_UNCOMPRESSED_SIZE,
        "max_file_count_in_zip": SCANNER_MAX_FILE_COUNT_IN_ZIP,
        "max_compression_ratio_single_file": SCANNER_MAX_COMPRESSION_RATIO_SINGLE_FILE,
        "max_compression_ratio_total": SCANNER_MAX_COMPRESSION_RATIO_TOTAL
    }
    scan_results = scan_file(
        file_path=job.staged_file_path, # type: ignore
        original_filename=job.original_filename, # type: ignore
        content_type=None, # TODO: Pass content_type if available on job model
        config=scan_config
    )

    scan_stage_log_message = f"Security scan completed. Status: {scan_results['scan_status']}."
    scan_stage_log_status = "success"

    if scan_results["scan_status"] == "failure_threat_detected":
        logger_instance.error(f"Security threats detected for job {job_id}: {scan_results['threats_found']}")
        job.processing_status = "FAILED_SECURITY_SCAN"
        job.validation_summary = {**(job.validation_summary or {}), "security_scan": scan_results}
        scan_stage_log_status = "failed_threat"
        scan_stage_log_message += f" Threats: {scan_results['threats_found']}"
        current_log.append({
            "timestamp": datetime.utcnow().isoformat(), "stage": "SecurityScan", "status": scan_stage_log_status,
            "message": scan_stage_log_message, "details": scan_results
        })
        job.pipeline_stages_executed_log = current_log
        db.commit()
        return False # Stop pipeline

    elif scan_results["scan_status"] == "failure_processing_error":
        logger_instance.error(f"Error during security scan for job {job_id}: {scan_results['errors']}")
        job.processing_status = "ERROR_SECURITY_SCAN"
        job.validation_summary = {**(job.validation_summary or {}), "security_scan": scan_results}
        scan_stage_log_status = "failed_internal_error"
        scan_stage_log_message += f" Errors: {scan_results['errors']}"
        current_log.append({
            "timestamp": datetime.utcnow().isoformat(), "stage": "SecurityScan", "status": scan_stage_log_status,
            "message": scan_stage_log_message, "details": scan_results
        })
        job.pipeline_stages_executed_log = current_log
        db.commit()
        return False # Stop pipeline

    logger_instance.info(f"Security scan successful for job {job_id}.")
    job.validation_summary = {**(job.validation_summary or {}), "security_scan_report": scan_results}
    current_log.append({
        "timestamp": datetime.utcnow().isoformat(), "stage": "SecurityScan", "status": scan_stage_log_status,
        "message": scan_stage_log_message, "details": scan_results
    })
    job.pipeline_stages_executed_log = current_log
    # db.commit() # Commit this stage's success, or let it be part of a larger transaction
    # --- End of Security Scan Stage ---

    # Stage: Format Detection
    logger_instance.info(f"Starting format detection for job {job_id}, file: {job.staged_file_path}")
    job.processing_status = "processing_format_detection" # Set status for the next stage
    # current_log is already initialized from above
    current_log.append({"timestamp": datetime.utcnow().isoformat(), "stage": "Format Detection", "status": "started", "message": f"Initiated format detection for staged file: {job.staged_file_path}"})
    job.pipeline_stages_executed_log = current_log
    db.commit() # Commit status update before starting potentially long operation

    detected_driver_name = None
    try:
        dataSource = ogr.Open(job.staged_file_path, 0)
        if dataSource is None:
            job.processing_status = "failed_format_detection"; job.validation_summary = {"format_detection_error": "GDAL/OGR could not open file."}
            log_message = f"GDAL/OGR failed to open/recognize file: {job.staged_file_path}"
        else:
            driver = dataSource.GetDriver()
            if driver: detected_driver_name = driver.GetName(); job.detected_format = detected_driver_name; job.processing_status = "format_detected_pending_conversion"; log_message = f"Detected format: {detected_driver_name}"
            else: job.processing_status = "failed_format_detection"; job.validation_summary = {"format_detection_error": "GDAL/OGR could not determine driver."}; log_message = "GDAL/OGR could not determine driver."
            dataSource = None
        current_log = list(job.pipeline_stages_executed_log or [])
        log_entry_status = "success" if detected_driver_name else "failed"
        current_log.append({"timestamp": datetime.utcnow().isoformat(), "stage": "Format Detection", "status": log_entry_status, "message": log_message})
        job.pipeline_stages_executed_log = current_log
        db.commit()
    except Exception as e_gdal:
        logger_instance.error(f"Exception during GDAL/OGR format detection for job {job_id}: {e_gdal}")
        job.processing_status = "failed_format_detection"; job.validation_summary = {"format_detection_error": f"GDAL/OGR Exception: {str(e_gdal)}"}
        current_log = list(job.pipeline_stages_executed_log or []); current_log.append({"timestamp": datetime.utcnow().isoformat(), "stage": "Format Detection", "status": "failed", "message": f"Exception: {str(e_gdal)}"})
        job.pipeline_stages_executed_log = current_log; db.commit()
        return False # Critical failure

    if not detected_driver_name: return False # Stop if format detection failed

    # Stage: Format Conversion (to GeoJSON)
    if job.processing_status == "format_detected_pending_conversion":
        geojson_filename = f"{job_id}.geojson"; geojson_output_path = os.path.join(PROCESSED_AREA_PATH, geojson_filename)
        logger_instance.info(f"Converting {job.staged_file_path} to GeoJSON at {geojson_output_path} for job {job_id}.")
        try:
            result = gdal.VectorTranslate(destName=geojson_output_path, srcDS=job.staged_file_path, options=["-f", "GeoJSON", "-skipfailures"])
            if result is None or (not os.path.exists(geojson_output_path) or os.path.getsize(geojson_output_path) == 0):
                gdal_error_msg = gdal.GetLastErrorMsg() or "GDAL VectorTranslate failed, output missing/empty."
                raise Exception(gdal_error_msg)
            job.processing_status = "completed_format_conversion"; job.intermediate_geojson_path = geojson_output_path
            log_message = f"Successfully converted to GeoJSON: {geojson_output_path}"
            current_log = list(job.pipeline_stages_executed_log or []); current_log.append({"timestamp": datetime.utcnow().isoformat(), "stage": "Format Conversion (GeoJSON)", "status": "success", "message": log_message, "output_path": geojson_output_path})
            job.pipeline_stages_executed_log = current_log; db.commit()
        except Exception as e_convert:
            logger_instance.error(f"Exception during GeoJSON conversion for job {job_id}: {e_convert}")
            job.processing_status = "failed_format_conversion"; job.validation_summary = {**(job.validation_summary or {}), "format_conversion_error": str(e_convert)}
            current_log = list(job.pipeline_stages_executed_log or []); current_log.append({"timestamp": datetime.utcnow().isoformat(), "stage": "Format Conversion (GeoJSON)", "status": "failed", "message": f"Exception: {str(e_convert)}"})
            job.pipeline_stages_executed_log = current_log; db.commit()
            return False # Critical failure

    # Stage: CRS Detection
    if job.processing_status == "completed_format_conversion":
        logger_instance.info(f"Starting CRS detection for job {job_id}, file: {job.intermediate_geojson_path}")
        job.processing_status = "processing_crs_detection"; db.commit()
        detected_crs_wkt = None
        try:
            datasource = ogr.Open(job.intermediate_geojson_path, 0)
            if not datasource or datasource.GetLayerCount() == 0: raise Exception("Cannot open intermediate GeoJSON or no layers found.")
            layer = datasource.GetLayer(); spatial_ref = layer.GetSpatialRef()
            if spatial_ref: detected_crs_wkt = spatial_ref.ExportToWkt(); job.original_crs = detected_crs_wkt; job.processing_status = "crs_detected_pending_transformation"; log_message = f"Detected CRS: {detected_crs_wkt[:100]}..."
            else: job.processing_status = "failed_crs_detection"; log_message = "CRS not found in GeoJSON."; job.validation_summary = {**(job.validation_summary or {}), "crs_detection_error": log_message}
            datasource = None
            current_log = list(job.pipeline_stages_executed_log or []); log_status = "success" if job.processing_status == "crs_detected_pending_transformation" else "failed"
            current_log.append({"timestamp": datetime.utcnow().isoformat(), "stage": "CRS Detection", "status": log_status, "message": log_message, "detected_crs_wkt": detected_crs_wkt or "N/A"})
            job.pipeline_stages_executed_log = current_log; db.commit()
            if job.processing_status == "failed_crs_detection": return False # Stop if failed
        except Exception as e_crs_detect:
            logger_instance.error(f"Exception during CRS detection for job {job_id}: {e_crs_detect}")
            job.processing_status = "failed_crs_detection"; job.validation_summary = {**(job.validation_summary or {}), "crs_detection_error": str(e_crs_detect)}
            current_log = list(job.pipeline_stages_executed_log or []); current_log.append({"timestamp": datetime.utcnow().isoformat(), "stage": "CRS Detection", "status": "failed", "message": f"Exception: {str(e_crs_detect)}"})
            job.pipeline_stages_executed_log = current_log; db.commit()
            return False # Critical failure

    # Stage: CRS Transformation
    if job.processing_status == "crs_detected_pending_transformation":
        logger_instance.info(f"Starting CRS transformation for job {job_id}")
        job.processing_status = "processing_crs_transformation"; db.commit()
        source_srs = osr.SpatialReference(); source_srs.ImportFromWkt(job.original_crs); target_srs = osr.SpatialReference(); target_srs.ImportFromEPSG(TARGET_CRS_EPSG)
        needs_reprojection = not (source_srs.GetAuthorityName(None) == "EPSG" and str(source_srs.GetAuthorityCode(None)) == str(TARGET_CRS_EPSG))
        if not needs_reprojection:
            job.processing_status = "completed_crs_transformation"; job.target_crs = f"EPSG:{TARGET_CRS_EPSG}"; log_message = "CRS is already target CRS."
            current_log = list(job.pipeline_stages_executed_log or []); current_log.append({"timestamp": datetime.utcnow().isoformat(), "stage": "CRS Transformation", "status": "success", "message": log_message})
            job.pipeline_stages_executed_log = current_log; db.commit()
        else:
            reprojected_filename = f"{job_id}_epsg{TARGET_CRS_EPSG}.geojson"; reprojected_geojson_path = os.path.join(PROCESSED_AREA_PATH, reprojected_filename)
            try:
                result = gdal.VectorTranslate(destName=reprojected_geojson_path, srcDS=job.intermediate_geojson_path, options=["-f", "GeoJSON", "-skipfailures", f"-t_srs", f"EPSG:{TARGET_CRS_EPSG}"])
                if result is None or (not os.path.exists(reprojected_geojson_path) or os.path.getsize(reprojected_geojson_path) == 0):
                     gdal_error_msg = gdal.GetLastErrorMsg() or "GDAL VectorTranslate for reprojection failed, output missing/empty."
                     raise Exception(gdal_error_msg)
                job.intermediate_geojson_path = reprojected_geojson_path; job.target_crs = f"EPSG:{TARGET_CRS_EPSG}"; job.processing_status = "completed_crs_transformation"
                log_message = f"Successfully reprojected to {job.target_crs}. New file: {reprojected_geojson_path}"
                current_log = list(job.pipeline_stages_executed_log or []); current_log.append({"timestamp": datetime.utcnow().isoformat(), "stage": "CRS Transformation", "status": "success", "message": log_message, "output_path": reprojected_geojson_path})
                job.pipeline_stages_executed_log = current_log; db.commit()
            except Exception as e_crs_transform:
                logger_instance.error(f"Exception during CRS transformation for job {job_id}: {e_crs_transform}")
                job.processing_status = "failed_crs_transformation"; job.validation_summary = {**(job.validation_summary or {}), "crs_transformation_error": str(e_crs_transform)}
                current_log = list(job.pipeline_stages_executed_log or []); current_log.append({"timestamp": datetime.utcnow().isoformat(), "stage": "CRS Transformation", "status": "failed", "message": f"Exception: {str(e_crs_transform)}"})
                job.pipeline_stages_executed_log = current_log; db.commit()
                return False # Critical failure

    # Stage: Geocoding
    if job.processing_status == "completed_crs_transformation":
        logger_instance.info(f"Starting geocoding for job {job_id}")
        job.processing_status = "processing_geocoding"; current_log = list(job.pipeline_stages_executed_log or []); current_log.append({"timestamp": datetime.utcnow().isoformat(), "stage": "Geocoding", "status": "started", "message": "Initiated geocoding."}); job.pipeline_stages_executed_log = current_log; db.commit()
        try:
            if not job.intermediate_geojson_path or not os.path.exists(job.intermediate_geojson_path): raise FileNotFoundError("Intermediate GeoJSON not found for geocoding.")
            with open(job.intermediate_geojson_path, 'r+') as f:
                geojson_data = json.load(f)
                if 'features' not in geojson_data or not isinstance(geojson_data['features'], list): raise ValueError("Invalid GeoJSON features list.")
                updated_features, geocoding_summary_dict = geocode_and_update_features(geojson_data['features'], GEOCODING_ADDRESS_FIELDS, NOMINATIM_USER_AGENT, logger_instance)
                job.geocoding_summary = geocoding_summary_dict; geojson_data['features'] = updated_features
                f.seek(0); json.dump(geojson_data, f); f.truncate()
            job.processing_status = "completed_geocoding"; log_message = f"Geocoding completed. Summary: {geocoding_summary_dict}"
            current_log = list(job.pipeline_stages_executed_log or []); current_log.append({"timestamp": datetime.utcnow().isoformat(), "stage": "Geocoding", "status": "success", "message": log_message, "summary": geocoding_summary_dict})
            job.pipeline_stages_executed_log = current_log; db.commit()
        except Exception as e_geocode_step:
            logger_instance.error(f"Exception during geocoding step for job {job_id}: {e_geocode_step}")
            job.processing_status = "failed_geocoding_step"; job.validation_summary = {**(job.validation_summary or {}), "geocoding_pipeline_error": str(e_geocode_step)}
            current_log = list(job.pipeline_stages_executed_log or []); current_log.append({"timestamp": datetime.utcnow().isoformat(), "stage": "Geocoding", "status": "failed", "message": f"Exception: {str(e_geocode_step)}"})
            job.pipeline_stages_executed_log = current_log; db.commit()
            return False # Potentially not critical, depends on requirements

    # Stage: Complex Geometry Handling
    if job.processing_status == "completed_geocoding":
        logger_instance.info(f"Starting complex geometry handling for job {job_id}")
        job.processing_status = "processing_complex_geometry"; current_log = list(job.pipeline_stages_executed_log or []); current_log.append({"timestamp": datetime.utcnow().isoformat(), "stage": "Complex Geometry Handling", "status": "started", "message": "Initiated complex geometry handling."}); job.pipeline_stages_executed_log = current_log; db.commit()
        strategy_config = {'filter_types_kept': CGH_FILTER_TYPES_TO_KEEP, 'reject_if_types_found': CGH_REJECT_IF_TYPES_FOUND}
        try:
            if not job.intermediate_geojson_path or not os.path.exists(job.intermediate_geojson_path): raise FileNotFoundError("Intermediate GeoJSON not found for complex geometry handling.")
            with open(job.intermediate_geojson_path, 'r+') as f:
                geojson_data = json.load(f)
                if 'features' not in geojson_data or not isinstance(geojson_data['features'], list): raise ValueError("Invalid GeoJSON features list.")

                current_features = geojson_data['features']
                if COMPLEX_GEOMETRY_STRATEGY == "split_all":
                    features_after_explode, summary_explode = handle_complex_geometries(current_features, "explode_collections", strategy_config, logger_instance)
                    if summary_explode.get('job_failed_due_to_rejected_type'): raise Exception(f"Rejected during explode stage of split_all. Summary: {summary_explode}")
                    updated_features, summary_split = handle_complex_geometries(features_after_explode, "split_multi", strategy_config, logger_instance)
                    if summary_split.get('job_failed_due_to_rejected_type'): raise Exception(f"Rejected during split_multi stage of split_all. Summary: {summary_split}")
                    complex_geom_summary_dict = { # Simplified combined summary for split_all
                        "strategy_applied": "split_all", "feature_count_before": summary_explode['feature_count_before'], "feature_count_after": summary_split['feature_count_after'],
                        "collections_exploded": summary_explode.get('geometry_collections_exploded_count',0), "multis_split": summary_split.get('multi_geometries_split_count',0)
                    }
                else:
                    updated_features, complex_geom_summary_dict = handle_complex_geometries(current_features, COMPLEX_GEOMETRY_STRATEGY, strategy_config, logger_instance)

                if complex_geom_summary_dict.get('job_failed_due_to_rejected_type'): raise Exception(f"Rejected due to geometry type as per strategy '{COMPLEX_GEOMETRY_STRATEGY}'. Summary: {complex_geom_summary_dict}")

                job.complex_geometry_summary = complex_geom_summary_dict; geojson_data['features'] = updated_features
                f.seek(0); json.dump(geojson_data, f); f.truncate()
            job.processing_status = "completed_complex_geometry"; log_message = f"Complex geometry handling completed. Summary: {complex_geom_summary_dict}"
            current_log = list(job.pipeline_stages_executed_log or []); current_log.append({"timestamp": datetime.utcnow().isoformat(), "stage": f"Complex Geometry Handling ({COMPLEX_GEOMETRY_STRATEGY})", "status": "success", "message": log_message, "summary": complex_geom_summary_dict})
            job.pipeline_stages_executed_log = current_log; db.commit()
        except Exception as e_complex_geom_step:
            logger_instance.error(f"Exception during complex geometry handling for job {job_id}: {e_complex_geom_step}")
            job.processing_status = "failed_complex_geometry_step"; job.validation_summary = {**(job.validation_summary or {}), "complex_geometry_error": str(e_complex_geom_step)}
            current_log = list(job.pipeline_stages_executed_log or []); current_log.append({"timestamp": datetime.utcnow().isoformat(), "stage": f"Complex Geometry Handling ({COMPLEX_GEOMETRY_STRATEGY})", "status": "failed", "message": f"Exception: {str(e_complex_geom_step)}"})
            job.pipeline_stages_executed_log = current_log; db.commit()
            return False # Potentially not critical, depends on requirements

    logger_instance.info(f"Pipeline execution finished for job_id: {job_id} with status {job.processing_status}")
    # Final check: if any "failed_" status was set, overall success is False.
    # Otherwise, if it reached a "completed_" state for the last executed step, it's True.
    return not job.processing_status.startswith("failed_")
