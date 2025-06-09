# AEGIS-ECHO Data Ingestion Pipeline Design

## 1. Overview

*   **Purpose:** This document details the architecture and design of the AEGIS-ECHO Data Ingestion Pipeline for SPINDEX. The primary goal of this pipeline is to provide a secure, resilient, and extensible mechanism for ingesting heterogeneous, unstructured, and potentially malicious geospatial data from various sources into the SPINDEX system. It aims to abstract the complexities of data handling, ensuring that only validated and standardized data is made available for indexing.
*   **Scope:** Covers the pipeline from data reception to its promotion for indexing or quarantine.
*   **Guiding Principles:** Security by design, resilience, modularity, extensibility, detailed metadata tracking.

## 2. Pipeline Architecture

This section will describe the overall flow and components.

**(Placeholder for a diagram illustrating the stages and data flow that can be added later)**

### 2.1. Pipeline Stages

Detailed description of each stage:

*   **Stage 1: Secure Upload/Reception Endpoint**
    *   **Responsibilities:** Securely accept incoming data.
    *   **Input:** Raw data (file uploads via HTTP POST, potentially API streams, or connection parameters for database sources in later phases).
    *   **Process:**
        *   Authentication & Authorization: Verify user identity and permissions (linking to RBAC in `GUI_DESIGN.md`).
        *   Initial validation (e.g., file size limits, basic MIME type checks).
        *   Immediate transfer to Staging Area.
    *   **Output:** Data written to Staging Area; metadata entry for the Ingestion Task.
    *   **Technology:** Part of the FastAPI backend.
*   **Stage 2: Staging Area**
    *   **Responsibilities:** Provide temporary, isolated storage for uploaded raw data before processing.
    *   **Characteristics:**
        *   Isolated from core system and other data.
        *   Unique, traceable identifiers for each staged item.
        *   Permissions restricted to pipeline components.
    *   **Technology:** Dedicated directory on the file system with strict permissions, or a dedicated, secured S3 bucket/Azure Blob container.
*   **Stage 3: Ingestion Task Trigger**
    *   **Responsibilities:** Initiate asynchronous processing of staged data.
    *   **Process:**
        *   Upon successful staging of data, a message is published to the Ingestion Task Queue.
        *   Message content: Reference to staged data (e.g., path/URI), original filename, user ID, initial metadata (upload timestamp, source type if known).
    *   **Technology:** Message publishing logic within the FastAPI upload endpoint, targeting RabbitMQ/Redis.
*   **Stage 4: Pipeline Worker Execution**
    *   **Responsibilities:** Orchestrate and execute a sequence of configurable processing and validation modules on the data.
    *   **Process:**
        *   Workers subscribe to the Ingestion Task Queue.
        *   On receiving a task:
            1.  Retrieve data from Staging Area.
            2.  Execute a defined sequence of pluggable modules (see Section 3). Each module processes the data and can pass its output to the next, or modify data in place.
            3.  Collect logs, results, and error information from each module.
            4.  Update the Metadata Catalogue at each significant step.
    *   **Technology:** Celery workers (if using Celery with RabbitMQ/Redis). Each module could be a Python function or class called by the worker.
*   **Stage 5: Data Promotion**
    *   **Responsibilities:** Move validated and processed data to a trusted location for subsequent indexing by SPINDEX.
    *   **Process:**
        *   If all mandatory pipeline modules execute successfully and pass all validation checks (geometric, schema, security):
            1.  The final, processed data (e.g., the GeoJSON file from `PROCESSED_AREA_PATH`) is considered ready for promotion.
            2.  The `IngestionJobMetadata.processing_status` is updated to `validation_successful_ready_for_promotion`.
            3.  The `IngestionJobMetadata.location_of_promoted_data` is updated with the path to the processed file.
            4.  (Optionally, in a full system) A notification or event could be triggered to inform SPINDEX that new data is ready for indexing.
    *   **Initial Implementation Note:** The `process_staged_file_task` in `backend/main.py` now sets the status to `validation_successful_ready_for_promotion` if geometric validation passes and placeholder schema/security checks are considered successful. The path to the GeoJSON file in `processed_area/` is recorded.
*   **Stage 6: Data Quarantine (Dead-Letter Queue - DLQ)**
    *   **Responsibilities:** Isolate data that fails processing or validation, along with detailed error information, for manual review.
    *   **Process:**
        *   If any critical pipeline module fails (e.g., GDAL transformation) or data does not pass validation checks (e.g., geometric validation):
            1.  The `IngestionJobMetadata.processing_status` is updated to `quarantined_validation_failed` (or a more specific failure status like `crs_transformed_failed`).
            2.  A detailed error log, including which module/check failed and why, is stored in `IngestionJobMetadata.error_log_references` or `IngestionJobMetadata.pipeline_stages_executed_log`.
            3.  The path to the problematic data (either the original staged file or the partially processed file like the GeoJSON from `processed_area/`) is recorded in `IngestionJobMetadata.location_of_quarantined_data`.
            4.  (Optionally) Notifications sent to administrators.
    *   **Initial Implementation Note:** The `process_staged_file_task` in `backend/main.py` now sets the status to `quarantined_validation_failed` if geometric validation fails. It also handles `crs_transformed_failed` status. Error details are logged in the metadata.
    *   **Technology:** The DLQ could be a specific routing key in RabbitMQ that routes failed tasks to a dedicated queue, or simply a designated storage location for failed data. The metadata status update is key.
            4.  (Optionally) Notifications sent to administrators.
    *   **Technology:** The DLQ could be a specific routing key in RabbitMQ that routes failed tasks to a dedicated queue, or simply a designated storage location for failed data.

### 2.2. Data Flow Diagram

**(Placeholder for a diagram showing these stages and interactions that can be added later)**

## 3. Core Pluggable Modules (Initial Design Focus)

This section details the design of the initial set of processing/validation modules within the Pipeline Worker Execution stage. Each module should be designed to be independently configurable and ideally pluggable/skippable where appropriate.

*   **3.1. Format Conversion & Abstraction (GDAL/OGR Integration)**
    *   **Purpose:** Convert various input vector formats into a common internal representation and detect the input format.
    *   **Library:** GDAL/OGR.
    *   **Initial Implementation Note:** A basic version of format detection and conversion to GeoJSON is implemented within the `process_staged_file_task` Celery task in `backend/main.py`.
    *   **Process (as implemented in `process_staged_file_task`):**
        1.  The staged input file is opened using `ogr.Open()`.
        2.  The driver name (format) is retrieved using `in_ds.GetDriver().GetName()` and logged in the metadata.
        3.  The task iterates through layers and features.
        4.  The output is written to a new GeoJSON file in the `processed_area/` directory. This acts as a common interchange format for this initial implementation.
    *   **Conceptual Process (Full Module):**
        *   Detect input format using GDAL.
        *   Read vector data (geometries, attributes).
        *   Transform to a well-defined internal SPINDEX format or a standard interchange format like GeoJSON for further processing by subsequent modules.
    *   **Configuration:** List of supported input formats. Output format for inter-module exchange (e.g., GeoJSON).
    *   **Error Handling:** Failure on unsupported format or read errors. Update metadata with error status.
*   **3.2. Coordinate Reference System (CRS) Transformation**
    *   **Purpose:** Ensure all data is in a standard internal CRS before indexing.
    *   **Standard Internal CRS (as implemented):** EPSG:4326 - WGS 84. This is configurable in `backend/main.py` via `TARGET_CRS_EPSG`.
    *   **Initial Implementation Note:** CRS detection and transformation to EPSG:4326 are implemented within the `process_staged_file_task` Celery task.
    *   **Process (as implemented in `process_staged_file_task`):**
        1.  For each layer in the input datasource, the original CRS is obtained using `in_layer.GetSpatialRef()`. This is logged in the metadata.
        2.  A target `osr.SpatialReference` is created based on `TARGET_CRS_EPSG`.
        3.  An `osr.CoordinateTransformation` object is created from the original CRS to the target CRS.
        4.  Each feature's geometry is cloned and transformed using `geom_clone.Transform(transform)`.
        5.  The transformed feature is written to the output GeoJSON file (which will have the target CRS).
    *   **Conceptual Process (Full Module):**
        *   Auto-detect source CRS using GDAL/OGR. If detection fails, allow user to specify or flag as error.
        *   Re-project geometries to the standard internal CRS.
    *   **Error Handling:** Failure if CRS cannot be determined or re-projection fails. Update metadata with error status.
*   **3.3. Geometric Validation**
    *   **Purpose:** Check for and optionally repair invalid geometries.
    *   **Process:**
        *   Use GDAL/OGR's `IsValid()` or, more commonly for post-processing GeoJSON, use libraries like Shapely.
        *   Attempt repair for common issues (e.g., `MakeValid()`).
        *   Log errors for irreparable geometries.
    *   **Configuration:** Strict (reject invalid) vs. Attempt Repair.
    *   **Error Handling:** Depending on configuration, either quarantine data with invalid geometries or promote with warnings. Update metadata with validation summary (e.g., counts of valid/invalid geometries).
    *   **Initial Implementation Note:** The `process_staged_file_task` in `backend/main.py` now implements geometric validation after CRS transformation. It reads the processed GeoJSON file, converts each feature's geometry to a Shapely object, and checks `is_valid()`. Counts of valid and invalid geometries are logged to the `IngestionJobMetadata.validation_summary` and `IngestionJobMetadata.pipeline_stages_executed_log`. If invalid geometries are found, a `geometric_validation_passed` flag is set to false, leading to quarantine. No repair is attempted in this initial version.
*   **3.4. Content-Level Security Scanning**
    *   **Purpose:** Mitigate risks from potentially malicious file structures.
    *   **Process:**
        *   For XML-based formats (KML, GML): Configure XML parser to disable XXE processing and limit entity expansion.
        *   For compressed files (ZIP, etc.): Check for "zip bomb" characteristics (e.g., extreme compression ratios, excessive number of files). Use libraries that offer protection.
    *   **Error Handling:** Quarantine suspicious files.
    *   **Initial Implementation Note:** This is currently a placeholder in `process_staged_file_task` in `backend/main.py`. Comments like `# TODO: Implement pre-transformation XXE scan...` are added. A flag `content_security_passed` is set to `True` by default.
*   **3.5. Schema & Value-Domain Validation**
    *   **Purpose:** Validate data against expected structure and value constraints.
    *   **Process:**
        *   Check for presence of user-defined required columns/attributes.
        *   Validate data types (e.g., numeric columns contain numbers).
        *   Validate coordinate values are within logical domains (lat: -90 to 90, lon: -180 to 180 for EPSG:4326).
        *   (Future) Allow user-defined schemas or validation rules (e.g., JSON Schema for GeoJSON properties).
    *   **Error Handling:** Quarantine data that fails schema validation. Log specific errors.
    *   **Initial Implementation Note:** This is currently a placeholder in `process_staged_file_task` in `backend/main.py`. Comments like `# TODO: Implement user-defined schema checks...` are added. A placeholder for coordinate domain checks is mentioned conceptually. A flag `schema_validation_passed` is set to `True` by default.

*   **3.6 Geocoding Service Integration (Nominatim)**
    *   **Purpose:** To generate point geometries (latitude, longitude) for features that have address information but may lack explicit geometry, or to enrich existing features with standardized coordinates.
    *   **Library:** `geopy` (specifically `Nominatim` geocoder).
    *   **Initial Implementation Note:** This is implemented within the `process_staged_file_task` Celery task in `backend/main.py`, after geometric validation.
    *   **Process (as implemented in `process_staged_file_task`):**
        1.  The task loads the GeoJSON file produced by the previous (e.g., CRS transformation/geometric validation) stage.
        2.  It iterates through each feature in the GeoJSON `FeatureCollection`.
        3.  For each feature, it checks if its `properties` contain a specific key (defaulting to `'address'`).
        4.  It also checks if the feature's `geometry` is `None` or represents an invalid/placeholder geometry (e.g., point at 0,0 if that's a known placeholder, or re-uses `shapely.is_valid`). This ensures geocoding is only attempted when needed.
        5.  If an address string is found and geometry needs to be generated/updated:
            *   A `Nominatim` geolocator is initialized with a unique `user_agent` (e.g., `spindex_pipeline_processor_v1_{uuid}`).
            *   The address is passed to `geolocator.geocode()`. A `timeout` is specified.
            *   Crucially, `time.sleep(1)` is called after each geocoding request to comply with Nominatim's Usage Policy (max 1 request per second).
            *   If a location is found, the feature's `geometry` is updated to a GeoJSON Point with the returned longitude and latitude. Success and coordinates are logged.
            *   If not found, or if a geocoding error/timeout occurs, this is logged, and the feature's geometry remains unchanged or is nulled.
        6.  After processing all features, if any geocoding was successfully applied, the modified GeoJSON `FeatureCollection` is saved to a new file in the `PROCESSED_AREA_PATH` (e.g., `_geocoded.geojson` suffix). The `IngestionJobMetadata.location_of_promoted_data` is updated to this new file path.
        7.  Statistics (attempted, successful, failed geocodes) are added to `IngestionJobMetadata.validation_summary` and a log entry is added to `pipeline_stages_executed_log`.
    *   **Configuration:**
        *   Address field name in feature properties (e.g., 'address', 'full_address').
        *   User agent string for the geocoding service.
        *   Timeout for geocoding requests.
    *   **Error Handling:** Handles `GeocoderTimedOut`, `GeocoderUnavailable`, `GeocoderServiceError` from `geopy`. Logs errors and continues processing other features. The overall success of the geocoding step can be determined by the statistics.
    *   **Production Alternatives:** For production environments or high-volume geocoding, using a bulk geocoding service, a self-hosted Nominatim instance, or a commercial geocoding API (like Google Geocoding API, Here, etc.) would be necessary to avoid hitting Nominatim's public service limits and for better performance and reliability.

*   **3.7 Complex Geometry Handling (Initial: Filtering)**
    *   **Purpose:** To handle datasets that may contain a mix of geometry types (e.g., points, lines, and polygons in a single file) or to ensure only a specific geometry type is processed further.
    *   **Initial Implementation Note:** This is implemented as a filtering step within the `process_staged_file_task` Celery task in `backend/main.py`, operating on the GeoJSON file produced by the (optional) geocoding stage, or the geometric validation stage if geocoding was skipped/failed.
    *   **Process (as implemented - "Filter" strategy):**
        1.  The task loads the input GeoJSON file (which is the `current_processed_file` from the previous stage).
        2.  **Determine Target Geometry Type:** It iterates through the first few features (e.g., up to 10) to find the first valid geometry type encountered (e.g., "Point", "LineString", "Polygon"). This determined type is logged as the `target_geometry_type`.
            *   A comment `# TODO: Allow desired_geometry_type as a parameter to the task for user control.` is included, indicating future enhancement for user-specified target types.
            *   If no valid geometry type is found in the sample, the current implementation passes all features through (effectively skipping filtering for this specific criterion).
        3.  **Filter Features:** A new list of features (`filtered_features`) is created. The task iterates through all features in the input GeoJSON. If a feature's `geometry.type` matches the determined `target_geometry_type`, it's added to `filtered_features`.
        4.  The number of original features, the chosen `target_geometry_type`, and the number of `filtered_features` are logged.
        5.  **Save Filtered Output:** If features were actually filtered out (i.e., `filtered_feature_count < original_feature_count`) or if a previous stage like geocoding created a new file (signaled by `geocoding_applied` flag), a new GeoJSON FeatureCollection containing only the `filtered_features` is created and saved to a new file in `PROCESSED_AREA_PATH` (e.g., `[job_id]_geomfiltered.geojson`). The `current_processed_file` path is updated to this new file.
        6.  The `IngestionJobMetadata` is updated:
            *   `pipeline_stages_executed_log` includes an entry for this geometry handling/filtering stage, noting the strategy, counts, and chosen type.
            *   `processing_status` is updated (e.g., to `geometry_filtering_completed` or a similar status indicating completion of this part of the pipeline).
            *   `location_of_promoted_data` is updated to the path of this new filtered file if it was created.
    *   **Future Enhancements / Alternative Strategies:**
        *   **Split by Type:** Instead of filtering, split a mixed-type GeoJSON into multiple files, one for each geometry type (e.g., `data_points.geojson`, `data_lines.geojson`). Each could then become a separate "promoted" dataset or be handled differently.
        *   **Reject on Mixed Types:** If strict homogeneity is required, the pipeline could be configured to reject (quarantine) any dataset that contains mixed geometry types after initial processing.
        *   **User Configuration:** Allow users to specify the desired geometry type(s) or the handling strategy (filter, split, reject) as part of the ingestion job submission or pipeline configuration.

*   **3.8 Unstructured Text Parsing (NLP/NER) for Location Extraction (Design)**
    *   **Purpose:** To identify and extract location names (Geopolitical Entities - GPE, other Locations - LOC) from unstructured or semi-structured text data. These extracted names can then be geocoded to create spatial features (points) or to enrich existing features. This module aims to unlock spatial information embedded in textual descriptions, documents, or text fields within tabular data.
    *   **Current Status:** This module is currently in the **design phase** and is not yet implemented in the `process_staged_file_task`. The following describes its conceptual workflow and considerations.
    *   **Proposed Libraries & Models:**
        *   **spaCy (Primary Candidate):**
            *   **Pros:** Offers excellent pre-trained models for various languages (e.g., `en_core_web_sm`, `en_core_web_lg` for English), which include NER components. It's known for its ease of use, good performance, and efficiency. It can be extended with custom training if needed.
            *   **Cons:** Pre-trained models might not always capture highly specific or obscure location names without fine-tuning. Model size can be a consideration for larger models.
        *   **NLTK (Natural Language Toolkit):**
            *   **Pros:** Highly flexible, provides many building blocks for NLP. Can use various taggers and chunkers for NER, including the Maxent NE Chunker which can be trained.
            *   **Cons:** Generally requires more setup and NLTK data downloads (corpora, models). Performance might be slower than spaCy for out-of-the-box NER.
        *   **Hugging Face Transformers:**
            *   **Pros:** Access to a vast range of state-of-the-art pre-trained NER models (BERT, RoBERTa, etc.). Can achieve very high accuracy, especially with models fine-tuned on specific NER datasets.
            *   **Cons:** Can be more resource-intensive (memory, compute) and have larger model download sizes. Integration might be slightly more complex than spaCy for basic NER.
        *   **Considerations:** The choice would depend on the desired balance of accuracy, performance, ease of deployment, and available computational resources. For an initial implementation, **spaCy** with its pre-trained English NER models (e.g., `en_core_web_lg` for better accuracy) is a strong candidate due to its balance of features and ease of use.
    *   **Workflow Design:**
        1.  **Input Identification:**
            *   The pipeline would need a mechanism to determine if a file or a specific field within a file requires NLP processing. This could be based on:
                *   Original file MIME type (e.g., `text/plain`, `application/pdf`, `application/msword`). For non-plain text files, a preliminary text extraction step (e.g., using `pypdf2` for PDFs, `python-docx` for DOCX) would be required before NLP.
                *   User-specified parameters during data upload (e.g., "Process column 'description' for locations").
                *   Configuration within a defined pipeline profile for a specific data source.
        2.  **Text Preprocessing (Optional):** Basic text cleaning (e.g., removing excessive whitespace, HTML tags if applicable) might be performed.
        3.  **NER Processing:**
            *   The selected NLP library (e.g., spaCy) would process the input text.
            *   The NER component would identify entities tagged as GPE (Geopolitical Entity), LOC (Location), and potentially others like FAC (Facility) if relevant.
        4.  **Location String Extraction & Aggregation:**
            *   Extracted location names (strings) are collected.
            *   A list of unique location strings is compiled to avoid redundant geocoding attempts.
        5.  **Integration with Geocoding Module:**
            *   The list of unique, extracted location strings is passed to the existing Geocoding module (currently Section 3.6, using Nominatim).
            *   Each location string is treated as an "address" to be geocoded.
        6.  **Output & Data Association:**
            *   **For document-based input (e.g., .txt, .pdf):** For each successfully geocoded location string, a new Point feature could be created in a GeoJSON FeatureCollection. Properties of these features could include the original location string, the source document ID, and perhaps a snippet of context text.
            *   **For tabular input (e.g., text field in a CSV or GeoJSON property):**
                *   Option 1: Enrich the existing feature/row by adding a new geometry column if one doesn't exist or if the original was missing/invalid.
                *   Option 2: Create new Point features for each geocoded location, ensuring a link back to the original feature/row ID (e.g., as a property). This is more suitable if one text field yields multiple locations.
            *   The final output containing these new or enriched spatial features would be saved to a new file in the `PROCESSED_AREA_PATH` (e.g., `[job_id]_nlp_geocoded.geojson`). The `current_processed_file` path in the pipeline would be updated.
    *   **Configuration Options:**
        *   `source_text_field_name`: If processing tabular data, the name of the field containing text to parse.
        *   `nlp_language_model`: Specification of the language model to use (e.g., `en_core_web_lg` for spaCy).
        *   `target_entity_types`: List of NER entity types to target (e.g., `["GPE", "LOC", "FAC"]`).
        *   `ner_confidence_threshold` (if supported by the model/library): Minimum confidence for an entity to be considered.
        *   `geocoding_fallback`: What to do if an extracted location cannot be geocoded (e.g., discard, keep as attribute only).
    *   **Metadata Logging:**
        *   `nlp_model_used`: Name and version of the NLP model.
        *   `text_sources_processed_count`: Number of documents or text fields processed.
        *   `extracted_location_entities_count`: Total number of targeted location entities found by NER.
        *   `unique_locations_for_geocoding_count`: Number of unique strings sent to the geocoder.
        *   Geocoding statistics for these entities (attempted, successful, failed – potentially integrated with the main geocoding stats or as a sub-section).
        *   Path to the output file with new/enriched features.

## 4. Metadata Cataloguing

*   **Purpose:** Track the lineage and processing history of all ingested data.
*   **Information to Capture (for each ingestion job/dataset):**
    *   `job_id` (unique identifier for the ingestion task)
    *   `original_filename`
    *   `upload_timestamp`
    *   `user_id` (who uploaded)
    *   `source_type` (e.g., file_upload, db_pull)
    *   `detected_format`
    *   `original_crs`
    *   `target_crs` (system standard)
    *   `processing_status` (e.g., staged, processing, promoted, quarantined)
    *   `promotion_timestamp` (if applicable)
    *   `quarantine_timestamp` (if applicable)
    *   `location_of_promoted_data` (if applicable)
    *   `location_of_quarantined_data` (if applicable)
    *   `pipeline_stages_executed` (list of modules run)
    *   `validation_results` (summary of validation outcomes, e.g., number of valid/invalid geometries)
    *   `error_log_references` (link to detailed error logs)
    *   `data_hash_original` (optional, hash of raw staged file)
    *   `data_hash_processed` (optional, hash of promoted data)
*   **Storage:** Dedicated tables in the SPINDEX metadata database.
*   **Access:** API endpoints for querying this catalogue.

## 5. Error Handling & Reporting

*   **Module-Level Logging:** Each pipeline module must generate detailed logs (INFO, WARNING, ERROR).
*   **Centralized Logging:** Aggregate logs from pipeline workers (e.g., using standard logging libraries configured to output to a central store or stdout for container log collection).
*   **Quarantine Mechanism:** As described in Stage 6, failures lead to quarantine. The error logs and reasons for failure must be clearly associated with the quarantined data.
*   **User Notification:** The Ingestion Dashboard in the GUI will be the primary interface for users to see the status and errors of their ingestion jobs.

## 6. Configuration

*   **Pipeline Definitions:** A mechanism (e.g., JSON/YAML configuration files, or database entries) will define available pipelines and the sequence of modules for each. This allows for different processing flows for different types of data sources.
*   **Module Configuration:** Each module may have its own specific configuration parameters (e.g., repair strategy for geometric validation, API keys for geocoding service).

## 7. Future Considerations / Extensibility

*   Adding new processing/validation modules.
*   User-defined pipeline configurations via the GUI.
*   Support for more complex data sources (API streams, database connectors).
*   Integration with external data quality tools.
```
