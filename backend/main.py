from fastapi import FastAPI, Depends, HTTPException, status, File, UploadFile
from fastapi.security import OAuth2PasswordBearer
from pydantic import BaseModel, Field
from typing import List, Optional, Dict, Any
import time # Added for Celery task simulation
from datetime import datetime # For timestamps
from celery import Celery, shared_task # Added for Celery
from celery.result import AsyncResult # Added for Celery task status
import shutil # For file operations
import os # For path operations
import uuid # For generating unique filenames
from osgeo import ogr, osr # Import GDAL components
import json # For reading GeoJSON output for Shapely validation
from shapely.geometry import shape, Point as ShapelyPoint # For converting GeoJSON geometry to Shapely object
from shapely.validation import is_valid # For checking geometry validity
import requests # For geopy/Nominatim HTTP requests, though geopy handles it. Good to have if direct calls needed.
from geopy.geocoders import Nominatim # For geocoding
from geopy.exc import GeocoderTimedOut, GeocoderUnavailable, GeocoderServiceError

# --- Constants ---
STAGING_AREA_PATH = "staging_area"
PROCESSED_AREA_PATH = "processed_area" # For successfully processed files
TARGET_CRS_EPSG = 4326 # WGS 84

# --- Celery Configuration ---
# The broker URL specifies the Redis instance Celery will use to send and receive messages.
# The backend URL specifies the Redis instance Celery will use to store task states and results.
# Using /0 and /1 to use different Redis databases if desired, or keep them same.
celery_app = Celery(
    __name__, # Using __name__ will make task names relative to this module
    broker='redis://localhost:6379/0',
    backend='redis://localhost:6379/1' # Using a different DB for backend results
)

# Optional: More detailed Celery configuration if needed
celery_app.conf.update(
    task_serializer='json',
    accept_content=['json'],  # Ignore other content
    result_serializer='json',
    timezone='UTC',
    enable_utc=True,
    # task_track_started=True, # Uncomment if you want to track 'STARTED' state
)


# --- Pydantic Models (Data Structures) ---
class Role(BaseModel):
    name: str
    permissions: List[str] = []

class User(BaseModel):
    username: str
    email: Optional[str] = None
    full_name: Optional[str] = None
    roles: List[Role] = []
    # In a real app, this would store hashed passwords, not plain text
    # hashed_password: str

# --- Metadata Catalogue Models ---
class IngestionJobMetadata(BaseModel):
    job_id: str = Field(default_factory=lambda: str(uuid.uuid4())) # The Celery task ID will serve as job_id
    original_filename: Optional[str] = None
    upload_timestamp: datetime = Field(default_factory=datetime.utcnow)
    user_id: Optional[str] = None
    source_type: Optional[str] = "file_upload" # Default for current implementation
    detected_format: Optional[str] = None
    original_crs: Optional[str] = None
    target_crs: Optional[str] = None # System's standard CRS, e.g., EPSG:4326
    processing_status: str = "pending_staging" # e.g., pending_staging, staged, processing, promoted, quarantined, failed
    staged_file_path: Optional[str] = None # Path within the staging area
    promotion_timestamp: Optional[datetime] = None
    quarantine_timestamp: Optional[datetime] = None
    location_of_promoted_data: Optional[str] = None
    location_of_quarantined_data: Optional[str] = None
    pipeline_stages_executed_log: List[Dict[str, Any]] = [] # List of dicts e.g. {"stage": "validation", "status": "success", "timestamp": "..."}
    validation_summary: Optional[Dict[str, Any]] = None # e.g., {"geometry_errors": 0, "schema_violations": 0}
    error_log_references: List[str] = [] # e.g., links or IDs to detailed error logs
    data_hash_original: Optional[str] = None
    data_hash_processed: Optional[str] = None
    celery_task_id: Optional[str] = None # To link back to the Celery task if job_id is different

    class Config:
        orm_mode = True # For easier conversion from ORM objects if we use one later
        json_encoders = {
            datetime: lambda v: v.isoformat() if v else None
        }


# --- In-memory "Database" for Metadata Catalogue ---
metadata_catalogue_db: List[IngestionJobMetadata] = []


# --- Index Management Pydantic Models ---
class IndexCreationRequest(BaseModel):
    promoted_data_path: str
    index_name: str
    index_type: str # e.g., "R-Tree", "Quadtree"
    index_parameters: Optional[Dict[str, Any]] = None

class IndexMetadata(BaseModel):
    index_id: str = Field(default_factory=lambda: str(uuid.uuid4()))
    index_name: str
    index_type: str
    promoted_data_path: str # Path to the data file used for this index version
    version: int = 1
    status: str = "creating" # e.g., 'creating', 'ready', 'failed', 'updating'
    created_at: datetime = Field(default_factory=datetime.utcnow)
    updated_at: Optional[datetime] = None
    celery_task_id: Optional[str] = None # For tracking the creation/update task
    # Additional fields like description, specific parameters used, item_count, etc. can be added

    class Config:
        json_encoders = {
            datetime: lambda v: v.isoformat() if v else None
        }

# --- In-memory "Database" for Index Metadata ---
index_metadata_db: List[IndexMetadata] = []


# --- Query Pydantic Models ---
class QueryRequest(BaseModel):
    index_id: str
    query_type: str  # e.g., 'bbox', 'point', 'knn'
    parameters: Dict[str, Any] # e.g., {"bbox": [minx, miny, maxx, maxy]}, {"point": [x,y], "k": 5}

class QueryResponse(BaseModel):
    results: List[Dict[str, Any]] # Each dict could represent a GeoJSON Feature
    count: int
    query_time_ms: int
    message: Optional[str] = None
    # Could also include query_parameters_received, warnings, etc.

class QueryHistoryItem(BaseModel):
    query_id: str = Field(default_factory=lambda: str(uuid.uuid4()))
    timestamp: datetime = Field(default_factory=datetime.utcnow)
    user_id: str
    request_data: QueryRequest
    # response_summary: Optional[Dict[str,Any]] # e.g. count, time_ms. Avoid storing full results in history for now.

    class Config:
        json_encoders = {
            datetime: lambda v: v.isoformat() if v else None
        }

# --- In-memory "Database" for Query History ---
query_history_db: List[QueryHistoryItem] = []


# --- Task Management Pydantic Models ---
class TaskStatusResponse(BaseModel):
    task_id: str
    status: str
    result: Optional[Any] = None # Can be any type depending on task

class TaskCreationRequest(BaseModel):
    param_x: int
    param_y: int

class TaskCreationResponse(BaseModel):
    task_id: str
    message: str

class FileUploadResponse(BaseModel):
    job_id: str
    filename: str
    message: str
    staged_filepath: Optional[str] = None # For debugging/confirmation, might remove in prod

# --- Dummy Data & OAuth2 Scheme ---
# This is a dummy user for now. In a real app, this would come from a database.
dummy_admin_role = Role(name="Admin", permissions=["create_index", "delete_index", "query_data", "manage_users"])
dummy_analyst_role = Role(name="Analyst", permissions=["create_index", "query_data"])
dummy_viewer_role = Role(name="Viewer", permissions=["query_data"])

dummy_users_db = {
    "admin": User(username="admin", email="admin@example.com", full_name="Admin User", roles=[dummy_admin_role]),
    "analyst": User(username="analyst", email="analyst@example.com", full_name="Analyst User", roles=[dummy_analyst_role]),
    "viewer": User(username="viewer", email="viewer@example.com", full_name="Viewer User", roles=[dummy_viewer_role]),
}

# OAuth2PasswordBearer is a utility to get the token from the Authorization header
# tokenUrl is a placeholder, would point to the actual token endpoint in a real auth system
oauth2_scheme = OAuth2PasswordBearer(tokenUrl="token")

# --- FastAPI App Instance ---
app = FastAPI(title="SPINDEX Backend", version="0.1.0")

# --- RBAC Stub Functions ---
async def get_current_user(token: str = Depends(oauth2_scheme)) -> User:
    """
    Stub function to get the current user.
    In a real application, this would:
    1. Decode the JWT token.
    2. Validate the token (signature, expiry, etc.).
    3. Extract user information (e.g., user ID or username).
    4. Fetch the user from the database.
    For now, it returns a dummy user based on a simple token string.
    """
    # Simulate token validation - extremely simplified!
    # In a real app, use python-jose to decode and validate.
    if token == "faketoken_admin":
        user = dummy_users_db.get("admin")
    elif token == "faketoken_analyst":
        user = dummy_users_db.get("analyst")
    elif token == "faketoken_viewer":
        user = dummy_users_db.get("viewer")
    else:
        # This mimics an invalid or expired token
        raise HTTPException(
            status_code=status.HTTP_401_UNAUTHORIZED,
            detail="Invalid authentication credentials",
            headers={"WWW-Authenticate": "Bearer"},
        )
    if user is None: # Should not happen if token is valid based on above logic
        raise HTTPException(status_code=status.HTTP_401_UNAUTHORIZED, detail="User not found")
    return user

def user_has_permission(user: User, permission_needed: str) -> bool:
    """
    Stub function to check if a user has a specific permission.
    In a real application, this would iterate through the user's roles
    and check if any role contains the required permission.
    For now, it always returns True for simplicity in this subtask.
    """
    # Real implementation would be:
    # for role in user.roles:
    #     if permission_needed in role.permissions:
    #         return True
    # return False
    return True # Stub: always return True

# --- API Endpoints ---
@app.get("/health", summary="Health Check", tags=["General"])
async def health_check():
    """
    Simple health check endpoint to confirm the API is running.
    """
    return {"status": "success", "message": "SPINDEX API is healthy"}

@app.get("/users/me", response_model=User, summary="Get Current User Details", tags=["Users"])
async def read_users_me(current_user: User = Depends(get_current_user)):
    """
    Returns the details of the currently authenticated user.
    This endpoint is protected and requires authentication.
    """
    # Example of how to use user_has_permission (though it's stubbed to True)
    # if not user_has_permission(current_user, "view_profile"):
    #     raise HTTPException(
    #         status_code=status.HTTP_403_FORBIDDEN,
    #         detail="You do not have permission to view this profile.",
    #     )
    return current_user

@app.get("/secure-data", summary="Access Secure Data (Example)", tags=["Data"])
async def get_secure_data(current_user: User = Depends(get_current_user)):
    """
    Example of a secure endpoint requiring a specific permission.
    """
    if not user_has_permission(current_user, "query_data"): # This permission is in dummy roles
        raise HTTPException(
            status_code=status.HTTP_403_FORBIDDEN,
            detail="You do not have permission to access this data.",
        )
    return {"message": f"Hello {current_user.username}, you have permission to see this secure data!"}

# --- Placeholder for future endpoints ---
# Example: Index Management (would require Admin or Analyst roles)
# @app.post("/indexes", summary="Create Index", tags=["Indexes"])
# async def create_index(index_data: dict, current_user: User = Depends(get_current_user)):
#     if not user_has_permission(current_user, "create_index"):
#         raise HTTPException(status.HTTP_403_FORBIDDEN, detail="Not enough permissions")
#     return {"message": "Index creation initiated (stub)", "data": index_data}


# --- Celery Tasks ---
@shared_task # Use @shared_task if you might define tasks in other files too
def sample_celery_task(x: int, y: int) -> int:
    """
    A simple Celery task that simulates some work and returns a result.
    """
    time.sleep(10)  # Simulate a long-running operation
    return x + y

@shared_task
def process_staged_file_task(staged_file_path: str, original_filename: str, user_id: str) -> str:
    """
    Performs CRS transformation, geometric validation, geocoding, and placeholder security/schema checks.
    Updates metadata catalogue with processing details.
    """
    task_id = process_staged_file_task.request.id
    log_prefix = f"Celery Task {task_id} (File: {original_filename}, User: {user_id}):"
    print(f"{log_prefix} Received for processing. Staged path: '{staged_file_path}'.")

    metadata_record = next((job for job in metadata_catalogue_db if job.celery_task_id == task_id), None)

    if not metadata_record:
        print(f"{log_prefix} CRITICAL_ERROR: Metadata record not found for task ID {task_id}.")
        return f"Error: Metadata record for task {task_id} not found. Processing aborted."

    metadata_record.processing_status = "processing_in_progress"
    metadata_record.pipeline_stages_executed_log.append({
        "stage": "initialization", "status": "success", "timestamp": datetime.utcnow().isoformat(),
        "message": "Processing started."
    })

    os.makedirs(PROCESSED_AREA_PATH, exist_ok=True)
    # Path for GDAL processed file (CRS transformed, initial validation)
    gdal_output_filename = f"{metadata_record.job_id}_crs_transformed.geojson" # Use job_id for unique naming
    gdal_output_filepath = os.path.join(PROCESSED_AREA_PATH, gdal_output_filename)

    # Path for Geocoded file (if geocoding happens)
    geocoded_output_filename = f"{metadata_record.job_id}_geocoded.geojson"
    geocoded_output_filepath = os.path.join(PROCESSED_AREA_PATH, geocoded_output_filename)

    in_ds = None
    out_ds = None
    gdal_success = False
    geometric_validation_passed = False
    geocoding_stats = {"attempted": 0, "successful": 0, "failed": 0}
    schema_validation_passed = True # Placeholder
    content_security_passed = True # Placeholder
    current_processed_file = staged_file_path # Start with the original staged file

    try:
        # === Stage 1: GDAL/OGR CRS Transformation ===
        metadata_record.pipeline_stages_executed_log.append({
            "stage": "gdal_crs_transform_started", "status": "running", "timestamp": datetime.utcnow().isoformat()
        })
        # ... (GDAL CRS transformation logic as before, saves to gdal_output_filepath) ...
        # Ensure gdal_output_filepath is correctly used and gdal_success is set
        # For brevity, assuming the GDAL logic from previous step is here
        # On success, set:
        # current_processed_file = gdal_output_filepath
        # metadata_record.location_of_promoted_data = gdal_output_filepath # Tentative promotion path
        # (Full GDAL logic from previous step needs to be here)
        # This is a simplified representation of the GDAL block:
        in_ds = ogr.Open(staged_file_path, 0);
        if in_ds is None: raise Exception(f"GDAL: Could not open file '{staged_file_path}'.")
        metadata_record.detected_format = in_ds.GetDriver().GetName()
        target_srs = osr.SpatialReference(); target_srs.ImportFromEPSG(TARGET_CRS_EPSG)
        metadata_record.target_crs = f"EPSG:{TARGET_CRS_EPSG}"
        out_driver = ogr.GetDriverByName("GeoJSON");
        if out_driver is None: raise Exception("GDAL: GeoJSON driver not available.")
        if os.path.exists(gdal_output_filepath): out_driver.DeleteDataSource(gdal_output_filepath)
        out_ds = out_driver.CreateDataSource(gdal_output_filepath)
        if out_ds is None: raise Exception(f"GDAL: Could not create output file '{gdal_output_filepath}'.")
        total_features_processed = 0
        for i in range(in_ds.GetLayerCount()):
            in_layer = in_ds.GetLayer(i); in_srs = in_layer.GetSpatialRef()
            transform = osr.CoordinateTransformation(in_srs, target_srs) if in_srs else None
            if in_srs: metadata_record.original_crs = in_srs.ExportToWkt()
            else: metadata_record.original_crs = "Unknown"
            out_layer_name = os.path.splitext(gdal_output_filename)[0]
            out_layer = out_ds.CreateLayer(out_layer_name, geom_type=in_layer.GetGeomType(), srs=target_srs)
            layer_defn = in_layer.GetLayerDefn()
            for j in range(layer_defn.GetFieldCount()): out_layer.CreateField(layer_defn.GetFieldDefn(j))
            for in_feature in in_layer:
                out_feature = ogr.Feature(out_layer.GetLayerDefn()); out_feature.SetFrom(in_feature)
                geom = out_feature.GetGeometryRef()
                if geom and transform: geom.Transform(transform)
                out_layer.CreateFeature(out_feature); out_feature.Destroy()
                total_features_processed +=1
        gdal_success = True
        current_processed_file = gdal_output_filepath # Update current file path
        metadata_record.pipeline_stages_executed_log.append({
            "stage": "gdal_crs_transform_completed", "status": "success", "timestamp": datetime.utcnow().isoformat(),
            "output_path": gdal_output_filepath, "features_total": total_features_processed
        })
        print(f"{log_prefix} GDAL CRS transformation successful. Output: {gdal_output_filepath}")
        # End of simplified GDAL block

    except Exception as e:
        # ... (error handling as before) ...
        error_message = f"GDAL/OGR processing failed: {str(e)}"
        print(f"{log_prefix} ERROR: {error_message}")
        metadata_record.error_log_references.append(f"gdal_error: {error_message}")
        metadata_record.pipeline_stages_executed_log.append({
            "stage": "gdal_crs_transform_error", "status": "failure", "error": error_message, "timestamp": datetime.utcnow().isoformat()
        })
    finally:
        if in_ds: in_ds.Destroy()
        if out_ds: out_ds.Destroy()

    if not gdal_success:
        metadata_record.processing_status = "crs_transformed_failed"
        metadata_record.quarantine_timestamp = datetime.utcnow()
        return f"Error during GDAL processing for '{original_filename}'. No further processing."

    # === Stage 2: Geometric Validation (using Shapely on GDAL-processed GeoJSON) ===
    # ... (geometric validation logic as before, using 'current_processed_file' as input) ...
    # This is a simplified representation of the validation block:
    metadata_record.pipeline_stages_executed_log.append({
        "stage": "geometric_validation_started", "status": "running", "timestamp": datetime.utcnow().isoformat(), "input_file": current_processed_file
    })
    valid_geoms = 0; invalid_geoms = 0
    try:
        with open(current_processed_file, 'r') as f: geojson_data_for_validation = json.load(f)
        if geojson_data_for_validation.get("type") == "FeatureCollection":
            for feature in geojson_data_for_validation.get("features", []):
                if 'geometry' in feature and feature['geometry']:
                    geom = shape(feature['geometry']);
                    if is_valid(geom): valid_geoms += 1
                    else: invalid_geoms += 1
                else: valid_geoms +=1
        geometric_validation_passed = (invalid_geoms == 0)
        metadata_record.validation_summary = {**(metadata_record.validation_summary or {}), "geometric_validation": {"valid_features": valid_geoms, "invalid_features": invalid_geoms, "passed": geometric_validation_passed}}
        metadata_record.pipeline_stages_executed_log.append({"stage": "geometric_validation_completed", "status": "success" if geometric_validation_passed else "failure", "valid_geoms": valid_geoms, "invalid_geoms": invalid_geoms, "timestamp": datetime.utcnow().isoformat()})
    except Exception as e:
        error_message = f"Shapely geometric validation failed: {str(e)}"; print(f"{log_prefix} ERROR: {error_message}")
        metadata_record.error_log_references.append(f"shapely_error: {error_message}"); geometric_validation_passed = False
        metadata_record.pipeline_stages_executed_log.append({"stage": "geometric_validation_error", "status": "failure", "error": error_message, "timestamp": datetime.utcnow().isoformat()})
    # End of simplified validation block

    # === Stage 3: Geocoding (if applicable, on the GeoJSON from previous stage) ===
    # This stage assumes the input is the 'current_processed_file' (which is GeoJSON)
    # TODO: Make address_field_name configurable
    address_field_name = 'address'
    geocoding_applied = False
    temp_geocoded_features = []

    try:
        with open(current_processed_file, 'r') as f:
            geojson_data_for_geocoding = json.load(f)

        if geojson_data_for_geocoding.get("type") == "FeatureCollection":
            geolocator = Nominatim(user_agent=f"spindex_pipeline_processor_v1_{uuid.uuid4()}") # Unique user agent per task run

            for feature in geojson_data_for_geocoding.get("features", []):
                properties = feature.get('properties', {})
                geometry = feature.get('geometry') # Could be None or an existing geometry dict

                # Condition: address exists and geometry is None or explicitly marked for update (e.g. point (0,0) from bad source)
                if properties and address_field_name in properties and (not geometry or not is_valid(shape(geometry))): # Basic check for missing/invalid geom
                    address_to_geocode = properties[address_field_name]
                    if address_to_geocode and isinstance(address_to_geocode, str):
                        geocoding_stats["attempted"] += 1
                        print(f"{log_prefix} Attempting to geocode address: '{address_to_geocode}'")
                        try:
                            location = geolocator.geocode(address_to_geocode, timeout=10)
                            time.sleep(1) # IMPORTANT: Nominatim usage policy (1 req/sec)
                            if location:
                                feature['geometry'] = {'type': 'Point', 'coordinates': [location.longitude, location.latitude]}
                                properties['geocoding_status'] = 'success'
                                properties['geocoded_lat'] = location.latitude
                                properties['geocoded_lon'] = location.longitude
                                geocoding_stats["successful"] += 1
                                print(f"{log_prefix} Geocoded successfully: {location.address} -> ({location.latitude}, {location.longitude})")
                                geocoding_applied = True
                            else:
                                properties['geocoding_status'] = 'failed_not_found'
                                geocoding_stats["failed"] += 1
                                print(f"{log_prefix} Address not found by geocoder: '{address_to_geocode}'")
                        except GeocoderTimedOut:
                            properties['geocoding_status'] = 'failed_timeout'
                            geocoding_stats["failed"] += 1
                            print(f"{log_prefix} Geocoding timed out for address: '{address_to_geocode}'")
                            time.sleep(1) # Wait after timeout as well
                        except (GeocoderUnavailable, GeocoderServiceError) as e:
                            properties['geocoding_status'] = f'failed_service_error: {str(e)}'
                            geocoding_stats["failed"] += 1
                            print(f"{log_prefix} Geocoding service error for '{address_to_geocode}': {e}")
                            time.sleep(1) # Wait after service error
                    else:
                        properties['geocoding_status'] = 'skipped_no_valid_address_field'
                temp_geocoded_features.append(feature)

            if geocoding_applied: # Save to a new file if any geocoding was done
                geojson_data_for_geocoding['features'] = temp_geocoded_features
                with open(geocoded_output_filepath, 'w') as f:
                    json.dump(geojson_data_for_geocoding, f)
                current_processed_file = geocoded_output_filepath # Update current file to this new geocoded version
                print(f"{log_prefix} Saved geocoded data to: {geocoded_output_filepath}")

        metadata_record.pipeline_stages_executed_log.append({
            "stage": "geocoding_completed", "status": "success", "timestamp": datetime.utcnow().isoformat(),
            "stats": geocoding_stats
        })
        if metadata_record.validation_summary: metadata_record.validation_summary["geocoding_stats"] = geocoding_stats
        else: metadata_record.validation_summary = {"geocoding_stats": geocoding_stats}

    except Exception as e:
        error_message = f"Geocoding stage failed: {str(e)}"
        print(f"{log_prefix} ERROR: {error_message}")
        metadata_record.error_log_references.append(f"geocoding_error: {error_message}")
        metadata_record.pipeline_stages_executed_log.append({
            "stage": "geocoding_error", "status": "failure", "error": error_message, "timestamp": datetime.utcnow().isoformat()
        })
        # Depending on policy, a geocoding failure might or might not be critical
        # For now, we'll let it proceed to overall validation status check.

    # === Stage 4: Content-Level Security Scanning (Placeholder) ===
    # ... (as before) ...

    # === Stage 5: Schema & Value-Domain Validation (Placeholder) ===
    # ... (as before) ...

    # === Stage 6: Complex Geometry Handling (Filtering) ===
    metadata_record.pipeline_stages_executed_log.append({
        "stage": "complex_geometry_handling_started", "status": "running",
        "timestamp": datetime.utcnow().isoformat(), "input_file": current_processed_file
    })
    geom_filtering_success = False
    try:
        if not os.path.exists(current_processed_file):
            raise FileNotFoundError(f"Input file for geometry filtering not found: {current_processed_file}")

        with open(current_processed_file, 'r') as f:
            source_geojson_data = json.load(f)

        if not isinstance(source_geojson_data, dict) or source_geojson_data.get("type") != "FeatureCollection" or not isinstance(source_geojson_data.get("features"), list):
            raise ValueError("Input for geometry filtering is not a valid GeoJSON FeatureCollection.")

        features = source_geojson_data["features"]
        original_feature_count = len(features)
        target_geometry_type = None

        # Determine target_geometry_type from the first few valid geometries
        # TODO: Allow desired_geometry_type as a parameter to the task for user control.
        for i, feature_to_inspect in enumerate(features[:10]): # Check up to first 10 features
            if feature_to_inspect and isinstance(feature_to_inspect.get('geometry'), dict) and feature_to_inspect['geometry'].get('type'):
                # Basic check for valid geometry structure before trying to use it
                try:
                    # Use Shapely to ensure it's a valid geometry object before trusting its type
                    temp_shape = shape(feature_to_inspect['geometry'])
                    if is_valid(temp_shape): # Only consider valid geometries for type determination
                        target_geometry_type = feature_to_inspect['geometry']['type']
                        print(f"{log_prefix} Determined target geometry type: {target_geometry_type} from feature {i}.")
                        break
                except Exception: # Catch errors from `shape()` if geometry is malformed
                    print(f"{log_prefix} Malformed geometry encountered while determining target type at feature {i}. Skipping.")
                    continue # Try next feature

        if not target_geometry_type:
            # If no valid geometry found in first 10, or all were null/malformed, make a decision.
            # For now, if no type determined, we might not filter, or filter out everything if strict.
            # Let's assume if no dominant type found, we keep all valid geometries from previous step.
            # This means this stage effectively does nothing if target_geometry_type remains None.
            # OR, if the previous stage (geometric_validation) already ensures homogeneity or a single type,
            # then this step is more about confirming that.
            # For this subtask, if no target_geometry_type is found, we'll just pass all features through.
            # This means if all geometries were null/invalid, they will pass here but might have failed geometric_validation_passed.
            print(f"{log_prefix} No dominant valid geometry type found in first 10 features. Passing all features from previous step.")
            filtered_features = features # Pass through if no specific type determined
        else:
            filtered_features = [
                f for f in features if f.get('geometry') and f['geometry'].get('type') == target_geometry_type
            ]

        filtered_feature_count = len(filtered_features)

        geom_filter_log_message = f"Geometry Handling: Target type '{target_geometry_type}'. Original: {original_feature_count} features, Filtered: {filtered_feature_count} features."
        print(f"{log_prefix} {geom_filter_log_message}")

        if filtered_feature_count < original_feature_count or geocoding_applied : # Only re-save if features were filtered or geocoding happened
            # Create new GeoJSON with filtered features
            filtered_geojson_data = {
                "type": "FeatureCollection",
                "features": filtered_features
            }
            geomfiltered_output_filename = f"{metadata_record.job_id}_geomfiltered.geojson"
            geomfiltered_output_filepath = os.path.join(PROCESSED_AREA_PATH, geomfiltered_output_filename)

            with open(geomfiltered_output_filepath, 'w') as f:
                json.dump(filtered_geojson_data, f)

            current_processed_file = geomfiltered_output_filepath # Update current file to this new filtered version
            print(f"{log_prefix} Saved geometry-filtered data to: {geomfiltered_output_filepath}")
            metadata_record.pipeline_stages_executed_log.append({
                "stage": "complex_geometry_filtered", "status": "success", "timestamp": datetime.utcnow().isoformat(),
                "details": geom_filter_log_message, "output_path": current_processed_file
            })
        else:
             metadata_record.pipeline_stages_executed_log.append({
                "stage": "complex_geometry_no_filter_needed", "status": "success", "timestamp": datetime.utcnow().isoformat(),
                "details": geom_filter_log_message
            })
        geom_filtering_success = True

    except Exception as e:
        error_message = f"Complex Geometry Handling stage failed: {str(e)}"
        print(f"{log_prefix} ERROR: {error_message}")
        metadata_record.error_log_references.append(f"geometry_handling_error: {error_message}")
        metadata_record.pipeline_stages_executed_log.append({
            "stage": "complex_geometry_handling_error", "status": "failure", "error": error_message, "timestamp": datetime.utcnow().isoformat()
        })
        # This failure might lead to quarantine depending on overall policy

    # === Stage 7: Data Promotion/Quarantine Logic ===
    # Ensure current_processed_file path is updated in metadata if any processing stage created a new file
    metadata_record.location_of_promoted_data = current_processed_file # Default to last known good file

    if gdal_success and geometric_validation_passed and geom_filtering_success and schema_validation_passed and content_security_passed:
        metadata_record.processing_status = "validation_successful_ready_for_promotion"
        metadata_record.promotion_timestamp = datetime.utcnow()
        result_message = f"File '{original_filename}' processed and validated successfully. Ready for promotion from: {current_processed_file}"
        print(f"{log_prefix} {result_message}")
    else:
        metadata_record.processing_status = "quarantined_validation_failed"
        metadata_record.quarantine_timestamp = datetime.utcnow()
        metadata_record.location_of_quarantined_data = current_processed_file # The last successfully written file

        error_summary = []
        if not geometric_validation_passed: error_summary.append(f"Geometric validation failed (invalid geoms: {invalid_geoms})")
        # Add other failure reasons
        full_error_message = "; ".join(error_summary) if error_summary else "Validation failed due to unspecified reasons (check logs)."
        metadata_record.error_log_references.append(f"validation_failure: {full_error_message}")

        result_message = f"File '{original_filename}' failed validation and was quarantined. Details: {full_error_message}"
        print(f"{log_prefix} {result_message}")

    return result_message

@shared_task
def create_spindex_task(index_id: str, promoted_data_path: str, index_name: str, index_type: str, index_parameters: Optional[dict]):
    """
    Placeholder Celery task to simulate the actual creation of a SPINDEX index.
    Updates the IndexMetadata record upon completion or failure.
    """
    task_id = create_spindex_task.request.id
    log_prefix = f"Celery Task {task_id} (Index Creation for ID: {index_id}):"
    print(f"{log_prefix} Received. Index Name: {index_name}, Type: {index_type}, Data: {promoted_data_path}, Params: {index_parameters}")

    # Find metadata record to update its celery_task_id
    metadata_record = next((idx for idx in index_metadata_db if idx.index_id == index_id), None)
    if metadata_record:
        metadata_record.celery_task_id = str(task_id) # Log current celery task id
    else:
        print(f"{log_prefix} CRITICAL_ERROR: IndexMetadata record not found for index_id {index_id} at task start.")
        # This indicates a potential issue, as the record should have been created by the API before dispatching.
        # Depending on desired robustness, could create one here, but it suggests an inconsistent state.

    # Simulate index creation work with SPINDEX core
    print(f"{log_prefix} SPINDEX core simulation: Creating {index_name} ({index_type}) from {promoted_data_path}...")
    time.sleep(15) # Simulate long processing time for index creation

    # Update metadata status after "creation"
    # This direct modification of a global list from Celery is a simplification for this subtask.
    # In a real system, this update would happen via a database transaction or an API call to the main app
    # to ensure data consistency and avoid race conditions, especially if Celery workers are separate processes/machines.
    if metadata_record:
        # Simulate success/failure of actual index creation
        creation_succeeded = True # Toggle this to test failure path
        if creation_succeeded:
            metadata_record.status = "ready"
            metadata_record.updated_at = datetime.utcnow()
            # metadata_record.celery_task_id = None # Clear task ID once completed (optional)
            print(f"{log_prefix} Index '{index_name}' marked as 'ready'.")
            return f"Index '{index_name}' (ID: {index_id}) created successfully."
        else:
            metadata_record.status = "failed"
            metadata_record.updated_at = datetime.utcnow()
            # metadata_record.celery_task_id = None
            print(f"{log_prefix} Index '{index_name}' marked as 'failed'.")
            return f"Failed to create index '{index_name}' (ID: {index_id})."
    else:
        # This case should ideally not be reached if the initial metadata creation in the API is robust.
        print(f"{log_prefix} CRITICAL_ERROR: IndexMetadata record not found for index_id {index_id} at task completion.")
        return f"Critical error: Metadata for index {index_id} not found upon task completion."


# --- Task Management API Endpoints ---
@app.post("/tasks/run_sample_task", response_model=TaskCreationResponse, status_code=status.HTTP_202_ACCEPTED, summary="Run Sample Celery Task", tags=["Tasks"])
async def run_sample_task(payload: TaskCreationRequest, current_user: User = Depends(get_current_user)):
    """
    Submits the `sample_celery_task` to the Celery worker.
    Requires authentication.
    """
    # Example of permission check for task submission (optional)
    # if not user_has_permission(current_user, "submit_tasks"):
    #     raise HTTPException(status.HTTP_403_FORBIDDEN, detail="Not allowed to submit tasks")

    task = sample_celery_task.delay(payload.param_x, payload.param_y)
    return {"task_id": task.id, "message": "Sample task submitted successfully."}

@app.get("/tasks/status/{task_id}", response_model=TaskStatusResponse, summary="Get Task Status and Result", tags=["Tasks"])
async def get_task_status(task_id: str, current_user: User = Depends(get_current_user)):
    """
    Retrieves the status and result of a Celery task given its ID.
    Requires authentication.
    """
    task_result = AsyncResult(task_id, app=celery_app)

    response_data = {
        "task_id": task_id,
        "status": task_result.status,
    }

    if task_result.successful():
        response_data["result"] = task_result.result
    elif task_result.failed():
        response_data["result"] = {"error": str(task_result.info)} # task_result.info contains the exception
    # Other states: PENDING, STARTED, RETRY, REVOKED
    # You might want to handle these explicitly if needed.

    return response_data


if __name__ == "__main__":
    import uvicorn
    # This is for local development. For deployment, use Docker or a process manager.
    # To run celery worker locally:
    # celery -A main.celery_app worker -l info
    # Ensure Redis server is running.
    uvicorn.run(app, host="0.0.0.0", port=8000)


# --- File Upload Endpoint ---
@app.post("/upload/data", response_model=FileUploadResponse, status_code=status.HTTP_202_ACCEPTED, summary="Upload Data for Processing", tags=["Data Upload"])
async def upload_data_for_processing(
    current_user: User = Depends(get_current_user),
    file: UploadFile = File(...)
):
    """
    Accepts a file upload, saves it to a staging area, and dispatches a Celery task
    for asynchronous processing via the AEGIS-ECHO pipeline.
    """
    if not file.filename:
        raise HTTPException(status_code=status.HTTP_400_BAD_REQUEST, detail="No filename provided.")

    # Create staging directory if it doesn't exist
    os.makedirs(STAGING_AREA_PATH, exist_ok=True)

    # Generate a unique filename for staging to avoid conflicts, preserving original extension
    original_filename = file.filename
    file_extension = os.path.splitext(original_filename)[1]
    unique_staged_filename = f"{uuid.uuid4()}{file_extension}"
    staged_file_path = os.path.join(STAGING_AREA_PATH, unique_staged_filename)

    try:
        # Save the uploaded file to the staging area
        with open(staged_file_path, "wb") as buffer:
            shutil.copyfileobj(file.file, buffer)
    except Exception as e:
        # Log the error appropriately in a real app
        raise HTTPException(
            status_code=status.HTTP_500_INTERNAL_SERVER_ERROR,
            detail=f"Could not save uploaded file: {str(e)}"
        )
    finally:
        file.file.close() # Ensure the file is closed

    # Dispatch Celery task for processing
    # Pass the absolute path or a path recognizable by the Celery worker
    # For simplicity, using relative path here, assuming Celery worker has same CWD context
    # In a distributed setup, this path would need to be accessible by all workers (e.g., shared volume, S3 path)

    # Create initial metadata record
    # The job_id for metadata can be the same as celery task_id for direct linkage
    celery_task = process_staged_file_task.delay(staged_file_path, original_filename, current_user.username)

    initial_metadata = IngestionJobMetadata(
        job_id=celery_task.id, # Using Celery task ID as the job_id for metadata
        celery_task_id=celery_task.id,
        original_filename=original_filename,
        upload_timestamp=datetime.utcnow(),
        user_id=current_user.username,
        source_type="file_upload",
        processing_status="staged_and_queued",
        staged_file_path=staged_file_path,
        pipeline_stages_executed_log=[{"stage": "upload_complete_queued", "status": "success", "timestamp": datetime.utcnow().isoformat()}]
    )
    metadata_catalogue_db.append(initial_metadata)

    return FileUploadResponse(
        job_id=celery_task.id,
        filename=original_filename,
        message="File successfully uploaded and queued for processing.",
        staged_filepath=staged_file_path
    )

# --- Metadata Catalogue API Endpoints ---
@app.post("/metadata/ingestion_jobs", response_model=IngestionJobMetadata, status_code=status.HTTP_201_CREATED, summary="Create Ingestion Job Metadata", tags=["Metadata"])
async def create_ingestion_job_metadata(
    metadata: IngestionJobMetadata,
    current_user: User = Depends(get_current_user) # Assuming admin/system role might do this
):
    """
    Manually creates an ingestion job metadata record.
    Primarily for testing or if pipeline workers need to register jobs directly via API.
    The main flow is that `/upload/data` creates the initial metadata record.
    """
    # Ensure job_id is unique if provided, or rely on default factory if not.
    if any(job.job_id == metadata.job_id for job in metadata_catalogue_db):
        raise HTTPException(status_code=400, detail=f"Job with ID {metadata.job_id} already exists.")
    metadata_catalogue_db.append(metadata)
    return metadata

@app.get("/metadata/ingestion_jobs/{job_id}", response_model=IngestionJobMetadata, summary="Get Ingestion Job Metadata by ID", tags=["Metadata"])
async def get_ingestion_job_metadata_by_id(job_id: str, current_user: User = Depends(get_current_user)):
    """
    Retrieves a specific ingestion job metadata record by its job_id (which is the Celery task ID).
    """
    job = next((job for job in metadata_catalogue_db if job.job_id == job_id), None)
    if not job:
        raise HTTPException(status_code=404, detail=f"Ingestion job with ID {job_id} not found.")
    return job

@app.get("/metadata/ingestion_jobs", response_model=List[IngestionJobMetadata], summary="List Ingestion Job Metadata", tags=["Metadata"])
async def list_ingestion_job_metadata(
    skip: int = 0,
    limit: int = 10,
    current_user: User = Depends(get_current_user)
):
    """
    Retrieves a list of ingestion job metadata records, with optional pagination.
    """
    return metadata_catalogue_db[skip : skip + limit]

@app.put("/metadata/ingestion_jobs/{job_id}", response_model=IngestionJobMetadata, summary="Update Ingestion Job Metadata", tags=["Metadata"])
async def update_ingestion_job_metadata(
    job_id: str,
    metadata_update: IngestionJobMetadata, # Technically, this should be a PATCH model
    current_user: User = Depends(get_current_user) # Assuming admin/system role
):
    """
    Updates an existing ingestion job metadata record.
    NOTE: This is a full PUT, meaning the entire object is replaced.
          A PATCH method with a model containing only updatable fields would be more appropriate for partial updates.
          For simplicity in this subtask, we use PUT.
    """
    job_index = -1
    for i, job in enumerate(metadata_catalogue_db):
        if job.job_id == job_id:
            job_index = i
            break

    if job_index == -1:
        raise HTTPException(status_code=404, detail=f"Ingestion job with ID {job_id} not found.")

    # Ensure the job_id in the path matches the one in the body, or update it if not present in body
    if metadata_update.job_id != job_id :
         # If job_id in body is different, it might be an error or an attempt to change job_id.
         # For this stub, we'll assume the path job_id is authoritative if body's is different or default.
         # A stricter check might be needed in a real app.
         pass # Keep path job_id

    updated_job = metadata_update.copy(update={'job_id': job_id}) # Ensure path job_id is used
    metadata_catalogue_db[job_index] = updated_job
    return updated_job


# --- SPINDEX Index Management API Endpoints ---
@app.post("/indexes", response_model=IndexMetadata, status_code=status.HTTP_202_ACCEPTED, summary="Create New Spatial Index", tags=["Indexes"])
async def create_new_index(
    request: IndexCreationRequest,
    current_user: User = Depends(get_current_user) # Analyst or Admin role
):
    """
    Initiates the creation of a new spatial index from a successfully processed data file.
    - `promoted_data_path`: Path to the data file in the 'processed_area' that has passed AEGIS validation.
    - `index_name`: User-defined name for the index.
    - `index_type`: Type of spatial index (e.g., "R-Tree", "Quadtree").
    - `index_parameters`: Optional dictionary of parameters for the chosen index type.
    """
    # Basic permission check (example, assuming 'create_index' permission)
    if not user_has_permission(current_user, "create_index"): # This is a stub, always true
        raise HTTPException(status_code=status.HTTP_403_FORBIDDEN, detail="User does not have permission to create indexes.")

    # TODO: Validate promoted_data_path - e.g., check if it exists and is in an expected location.
    # For now, we assume it's valid if provided.
    if not os.path.exists(request.promoted_data_path): # Simple check
        raise HTTPException(status_code=status.HTTP_400_BAD_REQUEST, detail=f"Promoted data path does not exist: {request.promoted_data_path}")

    new_index_id = str(uuid.uuid4())

    index_meta = IndexMetadata(
        index_id=new_index_id,
        index_name=request.index_name,
        index_type=request.index_type,
        promoted_data_path=request.promoted_data_path,
        status="creating", # Initial status
        version=1, # Initial version
        created_at=datetime.utcnow(),
        # celery_task_id will be set by the task itself, or can be set here if preferred.
    )

    # Dispatch Celery task for actual index creation by SPINDEX core (simulated)
    celery_task = create_spindex_task.delay(
        index_id=new_index_id,
        promoted_data_path=request.promoted_data_path,
        index_name=request.index_name,
        index_type=request.index_type,
        index_parameters=request.index_parameters
    )

    index_meta.celery_task_id = celery_task.id # Store Celery task ID for tracking
    index_metadata_db.append(index_meta)

    return index_meta

@app.get("/indexes", response_model=List[IndexMetadata], summary="List All Spatial Indexes", tags=["Indexes"])
async def list_all_indexes(
    skip: int = 0,
    limit: int = 10,
    current_user: User = Depends(get_current_user) # Any authenticated user can list
):
    """
    Retrieves a list of all spatial index metadata records, with optional pagination.
    """
    return index_metadata_db[skip : skip + limit]

@app.get("/indexes/{index_id}", response_model=IndexMetadata, summary="Get Spatial Index Metadata by ID", tags=["Indexes"])
async def get_index_metadata_by_id(index_id: str, current_user: User = Depends(get_current_user)):
    """
    Retrieves metadata for a specific spatial index by its ID.
    """
    index_meta = next((idx for idx in index_metadata_db if idx.index_id == index_id), None)
    if not index_meta:
        raise HTTPException(status_code=status.HTTP_404_NOT_FOUND, detail=f"Index with ID {index_id} not found.")
    return index_meta

@app.delete("/indexes/{index_id}", status_code=status.HTTP_200_OK, summary="Delete Spatial Index", tags=["Indexes"])
async def delete_spatial_index(index_id: str, current_user: User = Depends(get_current_user)): # Admin role typically
    """
    Deletes a spatial index (metadata and conceptual deletion of index data).
    Actual data files (promoted data, index files on disk) are not deleted by this stub.
    """
    if not user_has_permission(current_user, "delete_index"): # Stub, always true
         raise HTTPException(status_code=status.HTTP_403_FORBIDDEN, detail="User does not have permission to delete indexes.")

    global index_metadata_db
    original_len = len(index_metadata_db)
    index_metadata_db = [idx for idx in index_metadata_db if idx.index_id != index_id]

    if len(index_metadata_db) == original_len:
        raise HTTPException(status_code=status.HTTP_404_NOT_FOUND, detail=f"Index with ID {index_id} not found for deletion.")

    # TODO: In a real system, trigger cleanup of actual index files from SPINDEX core
    # and potentially the underlying processed data if no other index versions depend on it.

    return {"message": f"Index with ID {index_id} deleted successfully (metadata only)."}


# --- Query API Endpoints ---
@app.post("/query/execute", response_model=QueryResponse, summary="Execute Spatial Query", tags=["Query"])
async def execute_query(
    query_request: QueryRequest,
    current_user: User = Depends(get_current_user) # Requires 'query_data' permission
):
    """
    Executes a spatial query against a specified index.
    (Currently returns dummy data)
    """
    if not user_has_permission(current_user, "query_data"): # Stub, always true
        raise HTTPException(status_code=status.HTTP_403_FORBIDDEN, detail="User does not have permission to execute queries.")

    # 1. Validate index_id
    target_index = next((idx for idx in index_metadata_db if idx.index_id == query_request.index_id), None)
    if not target_index:
        raise HTTPException(status_code=status.HTTP_404_NOT_FOUND, detail=f"Index with ID {query_request.index_id} not found.")
    if target_index.status != "ready":
        raise HTTPException(status_code=status.HTTP_400_BAD_REQUEST, detail=f"Index {query_request.index_id} is not in 'ready' state (current: {target_index.status}).")

    # 2. Log query to history
    history_item = QueryHistoryItem(
        user_id=current_user.username, # Or a more persistent user ID
        request_data=query_request
    )
    query_history_db.insert(0, history_item) # Insert at beginning for recent first

    # 3. Simulate query execution with SPINDEX Core
    # This would involve passing query_request.parameters and target_index.promoted_data_path
    # to the SPINDEX core engine.
    print(f"Simulating query on index {target_index.index_name} (ID: {target_index.index_id}) with params: {query_request.parameters}")
    start_time = time.time()
    time.sleep(0.1) # Simulate query time
    end_time = time.time()
    query_time_ms = int((end_time - start_time) * 1000)

    # For now, return dummy results based on query_type
    dummy_results = []
    if query_request.query_type == "bbox":
        dummy_results = [{'type': 'Feature', 'geometry': {'type': 'Polygon', 'coordinates': [[[0,0], [1,0], [1,1], [0,1], [0,0]]]}, 'properties': {'id': 1, 'name': 'Dummy BBox Result'}}]
    elif query_request.query_type == "point_radius" or query_request.query_type == "knn": # Assuming point based query
         dummy_results = [{'type': 'Feature', 'geometry': {'type': 'Point', 'coordinates': query_request.parameters.get("point", [0,0])}, 'properties': {'id': 2, 'name': 'Dummy Point Result'}}]

    return QueryResponse(
        results=dummy_results,
        count=len(dummy_results),
        query_time_ms=query_time_ms,
        message="Query executed successfully (simulated)."
    )

@app.get("/query/history", response_model=List[QueryHistoryItem], summary="Get Query History", tags=["Query"])
async def get_query_history(
    skip: int = 0,
    limit: int = 20, # Show more history by default
    current_user: User = Depends(get_current_user)
):
    """
    Retrieves the current user's query history (or all history for admin - not implemented yet).
    """
    # Simple user-based filtering for query history (can be expanded)
    # user_history = [item for item in query_history_db if item.user_id == current_user.username]
    # For now, returning global history for simplicity of testing across users
    user_history = query_history_db
    return user_history[skip : skip + limit]
