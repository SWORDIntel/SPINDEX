# SPINDEX Backend API Guide

## 1. Introduction

The SPINDEX Backend API provides RESTful endpoints for interacting with the SPINDEX system. This includes managing data ingestion jobs, handling index metadata, storing and retrieving query history, and performing searches.

This guide provides an overview of key API functionalities, authentication, error handling, and how to use the auto-generated OpenAPI documentation for detailed endpoint specifications.

## 2. Base URL and OpenAPI Documentation

*   **Base URL:** The API is typically served at the root of the backend application (e.g., `http://localhost:8000/`).
*   **Interactive OpenAPI (Swagger UI):** Available at `/docs` (e.g., `http://localhost:8000/docs`).
*   **Alternative OpenAPI (ReDoc):** Available at `/redoc` (e.g., `http://localhost:8000/redoc`).

It is highly recommended to use the interactive OpenAPI documentation for the most up-to-date details on request/response schemas, parameters, and specific endpoint paths.

## 3. Authentication

The API uses a token-based authentication mechanism. Clients must provide a valid token in the `X-Auth-Token` HTTP header for all protected endpoints.

*   **Header:** `X-Auth-Token: <your_api_token>`
*   **Token Acquisition (Development):** For local development, default tokens are configured and can be overridden by environment variables:
    *   `AEGIS_TOKEN_ADMIN`
    *   `AEGIS_TOKEN_ANALYST`
    *   `AEGIS_TOKEN_VIEWER`
    (Refer to `backend/main.py` for default values if environment variables are not set).
*   **Roles and Permissions:** Different tokens correspond to different user roles (e.g., Admin, Analyst, Viewer), which have varying permissions for API operations. Unauthorized access attempts will result in an HTTP `401 Unauthorized` or `403 Forbidden` error.

## 4. Standard Error Handling

The API uses standardized JSON responses for errors:

*   **Client Errors (4xx):**
    *   `401 Unauthorized`: Missing or invalid `X-Auth-Token`.
        ```json
        { "detail": "Not authenticated: X-Auth-Token header is missing." }
        ```
        ```json
        { "detail": "Invalid authentication credentials." }
        ```
    *   `403 Forbidden`: Authenticated user lacks permission for the operation.
        ```json
        { "detail": "User does not have permission to upload data." }
        ```
    *   `404 Not Found`: Requested resource does not exist.
        ```json
        {
          "detail": "IndexMetadata with ID 'some-id' not found.",
          "error_code": "RESOURCE_NOT_FOUND"
        }
        ```
    *   `409 Conflict`: Request conflicts with the current state of the resource (e.g., creating a duplicate unique item).
        ```json
        {
          "detail": "IndexMetadata with index_name 'existing_index' already exists.",
          "error_code": "DUPLICATE_RESOURCE"
        }
        ```
    *   `422 Unprocessable Entity`: Request payload validation failed (e.g., missing required fields, incorrect data types). The response body will contain detailed information about validation errors.
        ```json
        {
          "detail": [
            {
              "loc": ["body", "index_type"],
              "msg": "field required",
              "type": "value_error.missing"
            }
          ]
        }
        ```
*   **Server Errors (5xx):**
    *   `500 Internal Server Error` (Specific, e.g., Database Error):
        ```json
        {
          "detail": "A database error occurred.",
          "error_code": "DATABASE_ERROR"
        }
        ```
    *   `500 Internal Server Error` (Generic/Unexpected): Includes an `error_id` for support and troubleshooting.
        ```json
        {
          "detail": "An unexpected internal server error occurred. Please contact support.",
          "error_id": "generated-uuid-for-this-error"
        }
        ```
    *   `503 Service Unavailable`: A backend dependency (like Elasticsearch) is unavailable.
        ```json
        { "detail": "Search service (Elasticsearch backend) is currently unavailable." }
        ```

## 5. Key API Endpoint Groups

The following sections highlight major groups of API endpoints. For complete details, always refer to the OpenAPI documentation (`/docs`).

### 5.1. AEGIS Data Ingestion Pipeline

*   **Endpoint:** `POST /aegis/upload`
*   **Purpose:** Upload data files for processing through the AEGIS pipeline.
*   **Authentication:** Required. User must have `upload_data` permission.
*   **Request:** `multipart/form-data` with a `file` part.
*   **Response (Success - 200 OK):**
    ```json
    {
      "job_id": "string (uuid)",
      "filename": "string",
      "message": "File uploaded and validated successfully. Ingestion job created.",
      "staged_filepath": "string (path on server)"
    }
    ```
*   **Key Validations:** File size, MIME type.
*   **Action:** Stages the file, creates an `IngestionJobDB` record, and publishes a message to NATS (via JobOutbox) to trigger the Celery worker for pipeline processing.

### 5.2. Index Metadata Management

*   **Base Path:** `/indexes`
*   **Purpose:** Manage metadata about search indexes created from promoted data.
*   **Authentication:** Required for all operations. Specific permissions might apply (e.g., admin for delete).

*   **`POST /indexes`**: Create new index metadata.
    *   **Request Body Example (`IndexMetadata` Pydantic model, some fields are auto-generated):**
        ```json
        {
          "index_name": "my_first_index",
          "index_type": "elasticsearch",
          "promoted_data_path": "/path/to/trusted_data/dataset.geojson"
        }
        ```
    *   **Response (Success - 201 Created):** The created `IndexMetadata` object.

*   **`GET /indexes`**: List all index metadata records (supports pagination with `skip` and `limit` query parameters).
    *   **Response (Success - 200 OK):** List of `IndexMetadata` objects.

*   **`GET /indexes/{index_id}`**: Retrieve a specific index metadata record by its ID.
    *   **Response (Success - 200 OK):** Single `IndexMetadata` object.
    *   **Error (404):** If `index_id` not found.

*   **`PUT /indexes/{index_id}`**: Update an existing index metadata record.
    *   **Request Body Example (partial update):**
        ```json
        {
          "status": "active",
          "index_name": "my_renamed_index"
        }
        ```
    *   **Response (Success - 200 OK):** The updated `IndexMetadata` object.
    *   **Errors (404, 409):** If `index_id` not found, or if update causes a conflict (e.g., duplicate name).

*   **`DELETE /indexes/{index_id}`**: Delete an index metadata record.
    *   **Response (Success - 204 No Content).**
    *   **Error (404):** If `index_id` not found.

### 5.3. Query History Management

*   **Base Path:** `/users/me/query_history`
*   **Purpose:** Log and retrieve search queries made by the authenticated user.
*   **Authentication:** Required for all operations. Endpoints are user-specific.

*   **`POST /users/me/query_history`**: Log a query.
    *   **Request Body (`QueryRequest` Pydantic model):**
        ```json
        {
          "index_id": "some_index_uuid_1",
          "query_type": "keyword_search",
          "parameters": {"keyword": "test search", "fields": ["content", "title"]}
        }
        ```
    *   **Response (Success - 201 Created):** The created `QueryHistoryItem` object.

*   **`GET /users/me/query_history`**: Retrieve query history for the authenticated user (supports pagination with `skip` and `limit`).
    *   **Response (Success - 200 OK):** List of `QueryHistoryItem` objects, newest first.

### 5.4. Search

*   **Endpoint:** `POST /search_with_avx`
*   **Purpose:** Perform search queries, optionally enhanced with AVX-512 post-filtering.
*   **Authentication:** Required.
*   **Request Body (`AvxSearchQueryRequest` Pydantic model):**
    ```json
    {
      "index_name": "spindex_default_index",
      "query_text": "main elasticsearch query",
      "avx_phrases": "phrase one for AVX\nphrase two for AVX", // Newline separated
      "use_avx": true,
      "avx_case_sensitive": false,
      "avx_find_all_occurrences": true,
      "avx_max_hits_per_doc": 0
    }
    ```
*   **Response (Success - 200 OK - `AvxSearchQueryResponse` Pydantic model):**
    ```json
    {
      "es_query_time_ms": 50,
      "avx_processing_time_ms": 20,
      "total_results_before_avx": 15,
      "total_results_after_avx": 3,
      "results": [
        {
          "document_id": "doc1",
          "document_content_preview": "Snippet...",
          "score": 0.95,
          "avx_match_found": true,
          "avx_match_details": { /* ... details from AVX plugin ... */ }
        }
        // ... other results
      ],
      "message": "Search complete."
    }
    ```
*   **Error (503):** If Elasticsearch is unavailable.
*   **Error (500):** For other search-related errors (ES query issues, C component failures).

## 6. Further Details

For precise request/response schemas, parameter options, and all available endpoints, please consult the interactive OpenAPI documentation available at the `/docs` endpoint of the running backend application.
```
