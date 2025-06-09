# SPINDEX Backend

This directory contains the Python FastAPI backend application for SPINDEX. It includes features for asynchronous task processing using Celery and Redis, a file upload endpoint for data ingestion with GDAL-based processing and Shapely-based geometric validation, and an in-memory metadata catalogue for tracking ingestion jobs.

## Prerequisites

*   Docker (for containerized deployment)
*   Python 3.11+ (for local development)
*   `pip` (Python package installer)
*   Redis server (for local development, if not using the Docker container that includes it)
*   **GDAL (Geospatial Data Abstraction Library):** System libraries and Python bindings.
*   **GEOS:** System library required by Shapely (often installed alongside GDAL or via `libgeos-dev`).
*   **Requests & Geopy:** Python libraries for making HTTP requests and geocoding (e.g., `pip install requests geopy`).
*   `curl` (for testing file uploads, or a similar API client)

## Application Structure

*   `main.py`: The main FastAPI application file. Contains API endpoints, Pydantic models (including `IngestionJobMetadata`), Celery app configuration, task definitions (including GDAL & Shapely processing), and the in-memory metadata catalogue.
*   `requirements.txt`: A list of Python dependencies, including `GDAL` and `shapely`.
*   `Dockerfile`: Instructions to build a Docker image, including Python dependencies, Redis server, GDAL, and GEOS system libraries.
*   `entrypoint.sh`: Script used by Docker to start Redis, Uvicorn, and the Celery worker.
*   `staging_area/`: Directory for temporary storage of raw uploaded files.
*   `processed_area/`: Directory where successfully transformed (e.g., to GeoJSON) files are stored by the Celery task.

## Development

### Local Setup (using a Virtual Environment)

1.  **Navigate to the `backend` directory.**
2.  **Install Redis** (if not already set up).
3.  **Install GDAL and GEOS System Libraries:**
    *   **Linux (Debian/Ubuntu):**
        ```bash
        sudo apt update
        sudo apt install libgdal-dev gdal-bin libgeos-dev
        ```
    *   **macOS (using Homebrew):**
        ```bash
        brew install gdal geos
        ```
    *   **Windows:** Consider Conda, WSL, or pre-compiled wheels for GDAL and Shapely (which bundles GEOS).
    *   Verify GDAL with `gdalinfo --version` and GEOS (indirectly via Python after installing Shapely).
4.  **Create and activate a Python virtual environment.**
5.  **Install Python dependencies:** `pip install -r requirements.txt`
    *(This will install `GDAL` and `shapely` Python packages, which link against the system libraries.)*
6.  **Run the FastAPI application locally:** `uvicorn main:app --reload --host 0.0.0.0 --port 8000`
7.  **Run the Celery worker locally:** In a new terminal (with venv activated):
    ```bash
    celery -A main.celery_app worker -l info -P solo
    ```
    *   Worker logs will show task execution, including GDAL & Shapely processing messages.

### Docker Deployment

The Docker container includes Redis, GDAL & GEOS system libraries, Python dependencies, FastAPI (Uvicorn), and a Celery worker.

1.  **Navigate to the `backend` directory.**
2.  **Build the Docker image:** `docker build -t spindex-backend-final:latest .`
3.  **Run the Docker container:** `docker run -d -p 8000:8000 --name spindex-backend-final-container spindex-backend-final:latest`
4.  **Accessing the application:** API at `http://localhost:8000`, Docs at `http://localhost:8000/docs`.
5.  **Viewing logs:** `docker logs spindex-backend-final-container`.

## API Endpoints

Refer to the OpenAPI documentation at `/docs` for full details.

*   `/health`, `/users/me`, `/secure-data`: General and example secure endpoints.

### Data Upload Endpoint:

*   **POST `/upload/data`**:
    *   Accepts a geospatial file, stages it, and queues `process_staged_file_task` via Celery. Creates an initial metadata record.
    *   **Request:** `curl -X POST "http://localhost:8000/upload/data" -H "Authorization: Bearer faketoken_admin" -F "file=@/path/to/your/geospatial_file.geojson"`
    *   **Response (202 Accepted):** Includes `job_id`, `filename`, `message`, `staged_filepath`.
    *   The `process_staged_file_task` Celery task now performs:
        1.  **CRS Transformation:** Converts the input file to GeoJSON in EPSG:4326, saving it to `processed_area/`. Updates metadata with format, CRS info.
        2.  **Geometric Validation:** Reads the GeoJSON produced by GDAL, uses `shapely` to check `is_valid` for each geometry. Counts valid/invalid geometries and logs this to metadata.
        3.  **Geocoding (Conditional):**
            *   If a feature in the GeoJSON has an 'address' property and its geometry is missing or invalid, it attempts to geocode the address using `geopy.geocoders.Nominatim`.
            *   **Important:** A `time.sleep(1)` is implemented between each Nominatim request to comply with their public usage policy (max 1 request/second). The `user_agent` is set to `spindex_pipeline_processor_v1_{uuid}`.
            *   **Nominatim Usage Policy:** Nominatim is a free service provided by OpenStreetMap. It is **crucial** to respect their usage policy, which includes providing a valid User-Agent and not sending too many requests too quickly. For production or high-volume use, consider setting up a local Nominatim instance or using a commercial geocoding service.
            *   If geocoding is successful, the feature's geometry is updated to a Point.
            *   The (potentially geocoded) features are saved to a new GeoJSON file (e.g., `*_geocoded.geojson`) in `processed_area/`.
            *   Relevant geocoding statistics (attempted, successful, failed) are logged in the `IngestionJobMetadata`.
        4.  **Complex Geometry Handling (Filtering):**
            *   Operates on the (potentially geocoded) GeoJSON file.
            *   Determines a `target_geometry_type` by inspecting the first few valid geometries in the dataset (e.g., "Point", "Polygon").
            *   Filters the dataset to include only features matching this `target_geometry_type`.
            *   Logs the original and filtered feature counts, and the chosen geometry type.
            *   Saves the filtered features to a new file (e.g., `*_geomfiltered.geojson`) in `processed_area/`. The `location_of_promoted_data` is updated to this new path.
            *   *(This is a simplification; more advanced strategies like user-defined type or splitting mixed types are future considerations.)*
        5.  **Unstructured Text Parsing (NLP/NER) for Location Extraction (Design Phase):**
            *   *(This module is a design concept and not yet implemented in the Celery task.)*
            *   **Purpose:** To identify and extract location names (e.g., GPE, LOC entities) from textual content that might be part of the uploaded data (e.g., a text field within a GeoJSON's properties, or from an uploaded plain text file).
            *   **Conceptual Process:** This stage would use NLP libraries (e.g., spaCy with pre-trained NER models) to parse text. Extracted location strings would then be passed to the Geocoding stage (Stage 3) to derive spatial Point geometries. This could result in new features or enrichment of existing features.
            *   This stage highlights a planned capability for handling data where geospatial information is not explicit but embedded in text.
        6.  **Content-Level Security Scanning (Placeholder):** Comments in code indicate where these checks would go. Currently assumes success.
        7.  **Schema & Value-Domain Validation (Placeholder):** Comments in code for future checks. Currently assumes success.
        8.  **Promotion/Quarantine Logic:**
            *   Based on the outcomes of all preceding steps (GDAL processing, geometric validation, geocoding success (if applicable), geometry filtering success), the status in metadata is set to `validation_successful_ready_for_promotion` or `quarantined_validation_failed`.
            *   The `location_of_promoted_data` in metadata points to the final successfully processed file.

### Task Management Endpoints:

*   **POST `/tasks/run_sample_task`**: Submits a sample Celery task.
*   **GET `/tasks/status/{task_id}`**: Retrieves status and result of any Celery task.
    *   For `process_staged_file_task`, the `result` field contains a summary message. The detailed outcome, including validation results and paths to processed files, should be checked via the Metadata Catalogue endpoints using the same task ID as `job_id`.

### Metadata Catalogue Endpoints:

The Metadata Catalogue is an **in-memory list** (data lost on app restart).

*   **POST `/metadata/ingestion_jobs`**: Manually creates a metadata record.
*   **GET `/metadata/ingestion_jobs/{job_id}`**: Retrieves a specific metadata record. This is the primary way to check detailed processing outcomes for an uploaded file.
*   **GET `/metadata/ingestion_jobs`**: Lists all metadata records.
*   **PUT `/metadata/ingestion_jobs/{job_id}`**: Updates a metadata record.

### `IngestionJobMetadata` Pydantic Model (Key Fields):

This model tracks the ingestion process. Consult `main.py` or `/docs` for the full model.
```python
class IngestionJobMetadata(BaseModel):
    job_id: str # Usually the Celery Task ID from file upload
    original_filename: Optional[str]
    # ... other identification fields ...
    processing_status: str # Critical field, e.g., "staged_and_queued", "processing_in_progress",
                           # "crs_transformed_success", "crs_transformed_failed",
                           # "validation_successful_ready_for_promotion", "quarantined_validation_failed"
    staged_file_path: Optional[str]
    location_of_promoted_data: Optional[str] # Path to file in processed_area/
    location_of_quarantined_data: Optional[str]
    detected_format: Optional[str]
    original_crs: Optional[str]
    target_crs: Optional[str]
    pipeline_stages_executed_log: List[Dict[str, Any]] # Log of steps and their outcomes
    validation_summary: Optional[Dict[str, Any]] # e.g., {"geometric_validation": {"valid_features": N, "invalid_features": M, "passed": True/False}}
    error_log_references: List[str] # Specific error messages or references
    # ... (see main.py or /docs for full model)
```

### Index Management Endpoints:

These endpoints manage the metadata of spatial indexes after data has been processed by the AEGIS-ECHO pipeline and is ready for indexing by SPINDEX. The actual index creation is simulated by a Celery task. Like the Ingestion Metadata, Index Metadata is stored in an **in-memory list** and will be lost on application restart.

*   **POST `/indexes`**:
    *   Initiates the creation of a new spatial index from a successfully processed data file.
    *   Requires authentication (e.g., Analyst or Admin role).
    *   **Request Body:** `IndexCreationRequest` Pydantic model (see below).
    *   **Response (202 Accepted):** `IndexMetadata` object with status "creating" and a `celery_task_id` for the `create_spindex_task`.
    *   The `create_spindex_task` Celery task simulates index creation (sleeps for 15s) and then updates the index status to "ready" in the in-memory list.
*   **GET `/indexes`**:
    *   Retrieves a list of all spatial index metadata records. Supports pagination with `skip` and `limit`.
    *   Requires authentication.
    *   **Response:** List of `IndexMetadata` objects.
*   **GET `/indexes/{index_id}`**:
    *   Retrieves metadata for a specific spatial index by its `index_id`.
    *   Requires authentication.
    *   **Response:** The `IndexMetadata` object or 404 if not found.
*   **DELETE `/indexes/{index_id}`**:
    *   Deletes a spatial index metadata record from the in-memory list.
    *   Requires authentication (e.g., Admin role).
    *   **Response (200 OK):** Success message or 404 if not found.
    *   *(Note: This only deletes the metadata. Actual index files managed by a SPINDEX core would require separate cleanup.)*

### Index Management Pydantic Models:

*   **`IndexCreationRequest`**: Used for the `POST /indexes` endpoint.
    ```python
    class IndexCreationRequest(BaseModel):
        promoted_data_path: str # Path to the AEGIS-processed file
        index_name: str
        index_type: str         # e.g., "R-Tree", "Quadtree"
        index_parameters: Optional[Dict[str, Any]] = None # Specific params for the index type
    ```
*   **`IndexMetadata`**: Represents the stored metadata for a spatial index.
    ```python
    class IndexMetadata(BaseModel):
        index_id: str
        index_name: str
        index_type: str
        promoted_data_path: str
        version: int = 1
        status: str             # e.g., 'creating', 'ready', 'failed'
        created_at: datetime
        updated_at: Optional[datetime] = None
        celery_task_id: Optional[str] = None # Tracks the Celery task for creation
        # ... (other potential fields like description, item_count)
    ```
    (Consult `main.py` or the `/docs` endpoint for the complete model definition.)

### Query Interface Endpoints:

These endpoints allow users to execute spatial queries against 'ready' indexes and view their query history. Query history is stored in an **in-memory list**.

*   **POST `/query/execute`**:
    *   Executes a spatial query against a specified, 'ready' index.
    *   Requires authentication (e.g., 'query_data' permission).
    *   Validates that the target `index_id` exists and its status is 'ready'.
    *   Logs the query request to an in-memory query history list.
    *   **Request Body:** `QueryRequest` Pydantic model (see below).
    *   **Response:** `QueryResponse` Pydantic model (see below), containing dummy results based on `query_type` and simulated query time.
*   **GET `/query/history`**:
    *   Retrieves the list of past queries (from in-memory `query_history_db`).
    *   Requires authentication.
    *   Supports pagination with `skip` and `limit`.
    *   **Response:** List of `QueryHistoryItem` objects.

### Query Interface Pydantic Models:

*   **`QueryRequest`**: Used for the `POST /query/execute` endpoint.
    ```python
    class QueryRequest(BaseModel):
        index_id: str
        query_type: str  # e.g., 'bbox', 'point_radius'
        parameters: Dict[str, Any] # e.g., {"bbox": [minx, miny, maxx, maxy]} or {"point": [x,y], "radius": r}
    ```
*   **`QueryResponse`**: Returned by the `POST /query/execute` endpoint.
    ```python
    class QueryResponse(BaseModel):
        results: List[Dict[str, Any]] # Dummy GeoJSON Feature-like list
        count: int
        query_time_ms: int
        message: Optional[str] = None
    ```
*   **`QueryHistoryItem`**: Represents a logged query in the history.
    ```python
    class QueryHistoryItem(BaseModel):
        query_id: str
        timestamp: datetime
        user_id: str
        request_data: QueryRequest
    ```
    (Consult `main.py` or the `/docs` endpoint for the complete model definition.)

This setup provides a more detailed framework for geospatial data ingestion, including initial validation steps.
In a production system, the in-memory list would be a persistent database, and error handling and module configuration would be more extensive.
