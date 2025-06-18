# AEGIS Message Queue (MQ) Pipeline Architecture

## 1. Overview

*   **Purpose:** To describe the redesigned AEGIS data ingestion pipeline utilizing a message queue (NATS JetStream) and PostgreSQL for enhanced decoupling, resilience, and scalability.
*   **Scope:** Covers job creation, message queuing, worker processing, and metadata management.
*   **Key Technologies:** NATS JetStream, PostgreSQL.

## 2. Core Components

*   **Secure Upload Endpoint:** The primary entry point for incoming data files. Responsible for initial validation and handoff to the staging area.
*   **Staging Area:** A temporary storage location (e.g., a dedicated file system path) where uploaded files are stored before being processed by the pipeline.
*   **NATS JetStream:** The core message queuing system.
    *   **Stream:** `AEGIS_JOB_STREAM`. This stream will capture subjects like `aegis.jobs.new`.
    *   **Configuration:**
        *   Storage: File-based persistence for durability.
        *   Replication: R=3 (3 replicas) for high availability.
        *   Retention Policy: `WorkQueue` policy for `AEGIS_JOB_STREAM` to ensure messages are kept until acknowledged by a worker, even if they are redelivered multiple times.
        *   Duplicate Message Detection: Configured with a suitable time window (e.g., 2 minutes) to prevent processing duplicate messages if producers publish them.
*   **AEGIS Job Producers:** Any component or service that originates a new data processing job. Initially, this is the Secure Upload Endpoint. These producers interact with the PostgreSQL database to create job records and (indirectly via the Transactional Outbox pattern) publish messages to NATS.
*   **AEGIS Pipeline Workers:** A pool of identical consumer applications that subscribe to the `AEGIS_JOB_STREAM`. These workers fetch jobs from NATS, execute the AEGIS processing modules, and update the job status in the PostgreSQL database.
*   **PostgreSQL Database (`IngestionJobDB`):** The central metadata store for all ingestion jobs, their status, and related events. It acts as the source of truth for the pipeline.
*   **AI-Powered Threat Detection Engine:** A specialized service that consumes data streams from NATS to analyze content for potential security threats using machine learning models and threat intelligence. It reports findings and can trigger alerts.
*   **Data Promotion Module:** A component responsible for moving validated and processed data to a designated trusted storage location and updating its metadata status.

## 3. PostgreSQL Schema (`IngestionJobDB`)

### `jobs` Table
Stores the primary record for each ingestion job.

*   `job_id`: UUID (Primary Key) - Universally unique identifier for the job.
*   `original_filename`: TEXT - The original name of the file uploaded by the user.
*   `staged_file_path`: TEXT - The path to the file in the staging area.
*   `job_status`: VARCHAR(50) - Current high-level status of the job. Examples:
    *   `PENDING_PUBLISH`: Job created in DB, awaiting message publication.
    *   `PUBLISHED_TO_MQ`: Message successfully published to NATS.
    *   `PROCESSING`: A worker has picked up the job.
    *   `COMPLETED`: All processing modules completed successfully.
    *   `VALIDATION_FAILED`: Failed business rule validation (e.g., invalid GeoJSON structure).
    *   `MODULE_FAILED`: A specific processing module failed.
    *   `QUARANTINED`: Job failed processing and has been moved to a quarantine location for manual review.
    *   `MANUALLY_RESOLVED`: A quarantined job that has been fixed and potentially reprocessed.
*   `overall_processing_status_message`: TEXT - A human-readable summary of the job's status or error.
*   `output_geopackage_path`: TEXT - Path to the final GeoPackage output.
*   `intermediate_geojson_path`: TEXT - Path to the processed GeoJSON before conversion to GeoPackage.
*   `summary_geojson_path`: TEXT - Path to a summary or simplified GeoJSON output.
*   `received_at`: TIMESTAMPTZ - When the original file was received by the upload endpoint.
*   `processing_started_at`: TIMESTAMPTZ (Nullable) - When a worker first started processing this job.
*   `processing_completed_at`: TIMESTAMPTZ (Nullable) - When the job reached a final state (COMPLETED, FAILED, QUARANTINED).
*   `quarantined_at`: TIMESTAMPTZ (Nullable) - When the job was moved to quarantine.
*   `quarantine_reason`: TEXT (Nullable) - Reason why the job was quarantined.
*   `quarantined_data_location`: TEXT (Nullable) - Path to where the quarantined data is stored.
*   `created_at`: TIMESTAMPTZ DEFAULT NOW() - Timestamp of when the job record was created.
*   `updated_at`: TIMESTAMPTZ DEFAULT NOW() - Timestamp of the last update to the job record.

*Key Indexes:* `job_id` (Primary Key), `job_status`, `received_at`.

### `job_outbox` Table (Transactional Outbox)
Ensures atomic job creation in the DB and message publishing to NATS.

*   `event_id`: BIGSERIAL (Primary Key) - Unique identifier for the outbox event.
*   `job_id`: UUID (FK to `jobs.job_id` ON DELETE CASCADE) - The job this event pertains to.
*   `nats_subject`: VARCHAR(255) - The NATS subject to publish to (e.g., `aegis.jobs.new`).
*   `nats_message_payload`: JSONB - The actual message payload to be sent to NATS.
*   `nats_message_id`: VARCHAR(128) - A unique ID for the NATS message (for idempotency at NATS level).
*   `published_status`: VARCHAR(20) DEFAULT 'UNPUBLISHED' - Status of the message (`UNPUBLISHED`, `PUBLISHED`, `FAILED_TO_PUBLISH`).
*   `created_at`: TIMESTAMPTZ DEFAULT NOW() - When the outbox event was created.
*   `published_at`: TIMESTAMPTZ (Nullable) - When the message was successfully published to NATS.

*Key Indexes:* `published_status`, `created_at` (for efficient polling by the message relay).

### `job_module_status` Table
Tracks the status of individual processing modules for each job.

*   `module_status_id`: BIGSERIAL (Primary Key) - Unique identifier for this module status entry.
*   `job_id`: UUID (FK to `jobs.job_id` ON DELETE CASCADE) - The job this module status pertains to.
*   `module_name`: VARCHAR(100) - Name of the processing module (e.g., `NLP_NER_Parser`, `Geometric_Validator`).
*   `module_status`: VARCHAR(50) - Status of this module for the job (`PENDING`, `RUNNING`, `COMPLETED_SUCCESS`, `COMPLETED_WARN`, `FAILED`).
*   `status_message`: TEXT (Nullable) - Any specific message from the module (e.g., error details, warnings).
*   `started_at`: TIMESTAMPTZ (Nullable) - When this module started processing for the job.
*   `completed_at`: TIMESTAMPTZ (Nullable) - When this module completed processing for the job.
*   `input_data_path`: TEXT (Nullable) - Path to the input data for this module (if applicable).
*   `output_data_path`: TEXT (Nullable) - Path to the output data from this module (if applicable).

*Key Indexes:* (`job_id`, `module_name`) (Unique constraint recommended), `job_id`.

## 4. Workflow

*(Placeholder for a high-level diagram illustrating the flow: Upload -> Staging -> DB (Jobs, Outbox) -> Message Relay -> NATS -> Worker / AI Threat Detection Engine -> Validation -> Data Promotion Module -> Trusted Storage & DB Updates / SOC or Alerting System)*

**A. Job Creation (Producer - e.g., Secure Upload Endpoint):**

1.  **File Reception:** An external client uploads a data file. The Secure Upload Endpoint receives it.
2.  **Staging:** The file is saved to a designated Staging Area on the file system. A unique path is generated.
3.  **Database Transaction (PostgreSQL):**
    *   Start a new database transaction.
    *   INSERT a new record into the `jobs` table.
        *   `job_id`: Generate UUID.
        *   `original_filename`: From upload.
        *   `staged_file_path`: Path from step A.2.
        *   `job_status`: Set to `PENDING_PUBLISH`.
        *   `received_at`: Current timestamp.
    *   INSERT a corresponding event into the `job_outbox` table.
        *   `job_id`: Same as above.
        *   `nats_subject`: e.g., `aegis.jobs.new.<filetype>` or `aegis.jobs.new`.
        *   `nats_message_payload`: JSON containing `job_id`, `staged_file_path`, and other relevant initial metadata.
        *   `nats_message_id`: Generate a unique ID (e.g., UUID) for NATS message deduplication.
        *   `published_status`: Set to `UNPUBLISHED`.
    *   Commit the database transaction. If any part fails, the transaction is rolled back, ensuring no orphaned job records or outbox events.

**B. Message Publishing (Dedicated Message Relay Service/Process):**

1.  **Polling:** The Message Relay periodically polls the `job_outbox` table for records where `published_status = 'UNPUBLISHED'`, ordered by `created_at`.
2.  **Publish to NATS:** For each unpublished event:
    *   Attempt to publish the `nats_message_payload` to the specified `nats_subject` on the `AEGIS_JOB_STREAM` in NATS JetStream. Include the `Nats-Msg-Id: <nats_message_id>` header for deduplication.
3.  **Handle NATS Acknowledgement:**
    *   **On successful publish (NATS ACK):**
        *   Update the corresponding `job_outbox` record: set `published_status = 'PUBLISHED'` and `published_at = NOW()`.
        *   Optionally, update the `jobs` table: set `job_status = 'PUBLISHED_TO_MQ'`. This can be done in the same transaction as the outbox update or as a separate step.
    *   **On NATS publish failure:**
        *   Implement a retry mechanism within the Message Relay for transient NATS errors.
        *   If failures persist, mark `job_outbox.published_status = 'FAILED_TO_PUBLISH'` and log the error for investigation. Do not delete the outbox message.

**C. Job Processing (AEGIS Pipeline Worker - NATS Pull Consumer):**

1.  **Fetch Message:** A worker instance requests a message from the `AEGIS_JOB_STREAM` (e.g., using a pull subscription on `aegis.jobs.new`). NATS ensures only one worker in the consumer group receives a particular message at a time.
2.  **Idempotency Check (Database):**
    *   Extract `job_id` from the NATS message.
    *   Query the `jobs` table for this `job_id`.
    *   If the `job_status` is already `PROCESSING`, `COMPLETED`, `VALIDATION_FAILED`, `MODULE_FAILED`, or `QUARANTINED`, it might be a redelivered message for a job already handled or in progress by another worker. The worker should carefully decide whether to ACK the message immediately or log a potential issue. (NATS duplicate detection should minimize this, but DB check is an extra safeguard).
3.  **Update Job Status (DB):**
    *   Set `jobs.job_status = 'PROCESSING'` and `jobs.processing_started_at = NOW()`.

#### Stage: Content-Level Security Scanning

Before extensive processing, each job's staged file undergoes a content-level security scan. This module is implemented in `backend/security_scanner.py`.

**Key Checks Performed:**

*   **File Type Validation (Magic Numbers):** Verifies file signatures (e.g., for ZIP, XML) against expectations from filename/MIME type.
*   **Zip Bomb Detection:** If the input is a ZIP archive, its metadata is analyzed for common zip bomb characteristics:
    *   Total uncompressed size of all members.
    *   Total number of files within the archive.
    *   Compression ratio of individual files.
    *   Overall archive compression ratio.
    These checks use configurable thresholds (sourced from environment variables like `SCANNER_MAX_TOTAL_UNCOMPRESSED_SIZE`, etc.) to identify potential threats.
*   **XXE (XML External Entity) Prevention:** For files identified as XML-based (e.g., KML, GML), an `lxml` parser configured with `resolve_entities=False` and `no_network=True` is used. This ensures that any external entities are not resolved, preventing XXE vulnerabilities during this parsing step.

**Outcomes:**

*   **Threat Detected:** If a significant threat (like a potential zip bomb) is detected, or a critical parsing issue occurs that suggests a security risk, the job's status is set to `FAILED_SECURITY_SCAN`. Details are logged in the `validation_summary` (and potentially `job_module_status`), and pipeline processing for the job is halted.
*   **Scanner Error:** If the scanner itself encounters an internal processing error, the job's status is set to `ERROR_SECURITY_SCAN`, and processing is halted.
*   **Success:** If no threats are detected and the scans complete successfully, the pipeline proceeds to subsequent stages. A report from the scan is added to the `validation_summary`.

This stage is integrated into `pipeline_logic.execute_aegis_pipeline` and runs on the initially staged file. After this stage, the worker proceeds to other defined modules.

4.  **Execute Further Pipeline Modules:** Sequentially (or in parallel if designed for it) execute the configured AEGIS processing modules (e.g., Format Detection, CRS Transformation, NLP, GeoJSON processing, validation).
    *   For each module:
        *   Update `job_module_status`: `module_name`, `module_status = 'RUNNING'`, `started_at = NOW()`.
        *   Execute the module.
        *   On completion: Update `job_module_status`: `module_status` (e.g., `COMPLETED_SUCCESS`, `FAILED`), `completed_at = NOW()`, `status_message`, `output_data_path`.
    *   **If a module fails:**
        *   Update `jobs.job_status = 'MODULE_FAILED'` (or `VALIDATION_FAILED` if it's a validation module).
        *   Set `jobs.overall_processing_status_message` with error details.
        *   Optionally, initiate a quarantine procedure (see Error Handling).
        *   Acknowledge the message to NATS (ACK) to prevent redelivery, as this is a terminal state for this processing attempt.
        *   Stop processing further modules for this job.
5.  **Final Job Status (DB):**
    *   If all modules complete successfully: Set `jobs.job_status = 'COMPLETED'`, `jobs.processing_completed_at = NOW()`.
    *   If validation issues occurred and were handled as a final state: `jobs.job_status = 'VALIDATION_FAILED'`.
6.  **Acknowledge Message to NATS:**
    *   If processing was successful or reached a definitive terminal state (e.g., module failure handled, quarantined): Send an ACK to NATS JetStream to confirm the message has been fully processed and should not be redelivered.
    *   If a transient error occurred where a retry might succeed (e.g., temporary network issue accessing an external resource): Send a NAK (Negative Acknowledgement) to NATS, or simply don't ACK and let the message visibility timeout expire, allowing NATS to redeliver it to another worker (or the same one after a delay).

**D. AI-Powered Threat Detection Processing:**

1.  **Message Consumption:** The AI-Powered Threat Detection Engine subscribes to relevant subjects on the `AEGIS_JOB_STREAM` (or a dedicated stream if appropriate). This could be all processed data messages or specific types of content identified for advanced threat analysis.
2.  **Data Analysis:**
    *   Retrieves the data associated with the job message (e.g., from the staged path or an intermediate processed output path).
    *   Applies its suite of AI/ML models, heuristics, and threat intelligence feeds to analyze the data content for sophisticated threats (e.g., malware, advanced persistent threat indicators, complex obfuscated scripts).
    *   Data is handled securely, potentially in an isolated environment depending on the nature of the analysis and the sensitivity of the models.
3.  **Reporting and Alerting:**
    *   **Database Update:** May update the `jobs` table with a threat score or specific flags. It can also log its activity and findings in the `job_module_status` table (e.g., `module_name = 'AI_Threat_Detector'`, `status = 'COMPLETED_ANALYSIS_CLEAN'` or `COMPLETED_ANALYSIS_THREAT_DETECTED'`, `status_message = 'Threat signature XYZ found'`).
    *   **NATS Alert Publication:** If a credible threat is detected, it publishes a structured alert message to a dedicated NATS subject (e.g., `aegis.alerts.threat.detected`). This message would contain `job_id`, threat type, severity, confidence score, and evidence.
    *   **External Alerting (Conceptual):** The NATS alert messages can then be consumed by a separate component responsible for forwarding alerts to a Security Operations Center (SOC), an Incident Response System, or designated security personnel. This maintains decoupling.
4.  **Message Acknowledgement:** ACKs the NATS message once its analysis and reporting/alerting actions are complete for that job. If it encounters a recoverable error, it may NAK the message for retry.

**Security Considerations for AI Threat Detection Engine:**
*   **Model Security:** ML models and threat intelligence data must be protected from unauthorized access or tampering.
*   **Data Isolation:** Depending on the analysis, consider processing data in sandboxed or isolated environments to prevent any potential "escape" of malicious content if it were to be inadvertently activated.
*   **Secure Communication:** All communication (NATS, database, external alert systems) must be over encrypted channels.
*   **Principle of Least Privilege:** The engine should have only the necessary permissions to access data and publish alerts.

**E. Data Promotion Module (AEGIS-013):**

This module is triggered after data has successfully passed all preceding processing and validation stages, including security scans and any domain-specific validations.

1.  **Responsibilities:**
    *   Securely move or copy validated data from its final intermediate processing location (e.g., a path in `intermediate_geojson_path` or `output_geopackage_path` from the `jobs` table) to a designated Trusted Storage area.
    *   Update the job's metadata in the `IngestionJobDB` to reflect its promotion. This includes setting a specific status (e.g., `PROMOTED_TO_TRUSTED`) and recording the path to the data in the Trusted Storage.
    *   Ensure atomicity or idempotency of the promotion process to prevent data loss or inconsistent states.

2.  **Inputs:**
    *   `job_id`: The UUID of the job being processed.
    *   `source_data_path`: The file path to the validated data that needs to be promoted (e.g., `jobs.output_geopackage_path`).
    *   `source_data_type`: Type of data being promoted (e.g., GeoPackage, GeoJSON).
    *   Associated metadata that might influence storage decisions (e.g., data classification, original filename).

3.  **Processing:**
    *   **Retrieve Job Metadata:** Fetch the current job record from the `jobs` table using `job_id`. Verify its status is appropriate for promotion (e.g., `COMPLETED` or a specific pre-promotion validation status).
    *   **Determine Trusted Storage Path:** Based on configuration and potentially data type or classification, determine the target directory and filename in the Trusted Storage.
    *   **Secure Transfer:**
        *   Copy or move the file from `source_data_path` to the target path in Trusted Storage. Use mechanisms that ensure data integrity during transfer (e.g., checksum verification after copy).
        *   The choice between copy and move depends on whether the intermediate data should be retained for a period or cleaned up immediately.
    *   **Database Update:**
        *   Start a database transaction.
        *   Update the `jobs` table for the given `job_id`:
            *   Set `job_status` to `PROMOTED` (or a similar status like `ARCHIVED_TRUSTED`).
            *   Store the new path in Trusted Storage in a dedicated field (e.g., `trusted_storage_path`).
            *   Set `promotion_timestamp` to `NOW()`.
        *   Log the promotion action in `job_module_status` (e.g., `module_name = 'DataPromotion'`, `status = 'COMPLETED_SUCCESS'`, `output_data_path = <new_trusted_path>`).
        *   Commit the database transaction.
    *   **Cleanup (Optional):** If the data was copied and the original is no longer needed, schedule or perform cleanup of the file at `source_data_path`.

4.  **Outputs:**
    *   `trusted_data_path`: The new file path of the data in the Trusted Storage.
    *   Updated `job_id` metadata in the `IngestionJobDB` reflecting the promoted status and location.

5.  **Trusted Storage:**
    *   This is a highly secure and reliable storage location designated for long-term retention of validated and processed data.
    *   Examples:
        *   A dedicated, access-controlled file system mount (e.g., NFS, SAN).
        *   A specific S3 bucket with strict IAM policies, versioning, and lifecycle management.
        *   A document store or database optimized for large object storage if data is transformed into a non-file format.
    *   Characteristics: Regular backups, access logging, integrity checks, and potentially immutability features.

6.  **Error Handling:**
    *   **Storage Full/Permissions Issue:** If the transfer to Trusted Storage fails due to space, permissions, or connectivity:
        *   Log the error in detail in `job_module_status` and `jobs.overall_processing_status_message`.
        *   Do NOT update the job status to `PROMOTED`. The job remains in its pre-promotion state.
        *   Implement a retry mechanism with backoff for transient issues.
        *   If retries are exhausted, the job status could be set to `PROMOTION_FAILED`.
        *   Generate an alert for manual intervention. The data remains in its last valid intermediate location.
    *   **Database Update Failure:** If the data transfer succeeds but the database update fails:
        *   This indicates an inconsistent state. The system should attempt to roll back the file move if possible, or at least flag the job for immediate manual reconciliation.
        *   Log the critical error. Alerts are crucial.
    *   **Data Integrity Mismatch:** If checksums don't match after transfer:
        *   Delete the corrupted file from Trusted Storage.
        *   Log the error and retry the transfer. If it fails repeatedly, treat as a storage or network issue.

7.  **Security Considerations:**
    *   **Credentials Management:** Securely manage credentials (e.g., API keys for S3, service account tokens for file servers) used by the Data Promotion Module. Use environment variables, secrets management systems, or IAM roles.
    *   **Path Sanitization:** Ensure that any paths used (source, destination) are canonicalized and validated to prevent path traversal or other injection attacks, especially if parts of paths are derived from metadata.
    *   **Data Integrity:** Use checksums (e.g., SHA256) to verify data integrity before and after transfer to Trusted Storage. Store these checksums in the metadata if possible.
    *   **Access Control:** The Trusted Storage must have strict access controls. The Data Promotion Module should operate with the minimum necessary permissions.
    *   **Auditability:** All promotion actions, successes, and failures must be logged for audit purposes.

## 5. Error Handling, Retries, and Dead Letter Queues (DLQ)

*   **Worker Errors:**
    *   **Transient Errors:** If a worker encounters a temporary error (e.g., network glitch, temporary unavailability of a downstream service), it can NAK the message (if supported by the NATS client library for explicit redelivery scheduling) or simply fail to ACK it before its visibility timeout. NATS JetStream will then redeliver the message after a configured delay.
    *   **Permanent Errors:** If a worker encounters an error that is unlikely to be resolved by a retry (e.g., malformed data, critical module failure), it should:
        1.  Update the job's status in the PostgreSQL `jobs` table to a failed state (e.g., `MODULE_FAILED`, `VALIDATION_FAILED`, `QUARANTINED`).
        2.  Store relevant error details and context in `jobs.overall_processing_status_message` and/or `job_module_status.status_message`.
        3.  ACK the message to NATS to prevent further redelivery attempts.
*   **NATS Consumer Configuration:** Consumers on `AEGIS_JOB_STREAM` should be configured with a maximum redelivery attempt count. If a message is redelivered more than this number of times, it indicates a persistent issue.
*   **Dead Letter Queue (DLQ) Strategy:**
    *   NATS JetStream itself can be configured with a DLQ where messages that exceed max delivery attempts are sent. However, for the AEGIS pipeline, the **PostgreSQL database acts as the primary "source of truth" for jobs requiring investigation.**
    *   When a job consistently fails (exhausts retries or encounters a permanent error):
        *   The worker updates its status in PostgreSQL to `QUARANTINED` or a specific `FAILED` state.
        *   The `jobs` table can then be queried by administrators or an automated system to identify jobs that require manual intervention. Fields like `quarantined_at`, `quarantine_reason`, and `quarantined_data_location` will store information for this process.
    *   This approach centralizes error management in the database, which already stores all job metadata.

## 6. Scalability & Resilience

*   **AEGIS Pipeline Workers:** Designed as stateless (or state managed via DB) services. They can be horizontally scaled out by running multiple instances. Each instance acts as a competing consumer on the NATS `AEGIS_JOB_STREAM`.
*   **NATS JetStream:**
    *   Can be deployed as a cluster (e.g., 3 or 5 nodes).
    *   The `AEGIS_JOB_STREAM` is configured with R=3 (replication factor of 3), meaning each message is replicated to 3 NATS servers, providing data safety and high availability. If a NATS node fails, the stream remains operational.
*   **PostgreSQL Database:**
    *   Can be configured for High Availability (HA) using streaming replication, failover mechanisms, and connection pooling to handle load from producers and workers.
*   **Message Relay:** Can also be scaled if it becomes a bottleneck, though typically it's a lightweight component.

## 7. MQ Choice Rationale: NATS JetStream

NATS JetStream was selected for the AEGIS pipeline due to several key advantages:

*   **Robust Persistence:** JetStream provides durable, log-based persistence for messages, ensuring data is not lost even if consumers or NATS servers restart.
*   **Work Queue Semantics:** `WorkQueue` retention policy on streams is ideal for distributing jobs among a pool of workers, ensuring each job is processed by one worker.
*   **Exactly-Once Semantics (within NATS, aids At-Least-Once for workers):** JetStream offers features like message deduplication (via `Nats-Msg-Id`) and acknowledgements, which are crucial building blocks for achieving at-least-once or effectively-once processing in the workers when combined with idempotent database operations.
*   **Operational Simplicity:** Compared to systems like Apache Kafka (which often requires Zookeeper for cluster management), NATS is generally considered easier to deploy, manage, and scale, especially for smaller to medium-sized deployments.
*   **Performance:** NATS is known for its high performance and low latency.
*   **Client Libraries:** Good availability of client libraries in various programming languages.
*   **Pull Consumers:** JetStream supports pull consumers, which allow workers to request messages at their own pace, offering better flow control and backpressure management compared to push-based consumers in some scenarios.
*   **Built-in Clustering and HA:** JetStream's clustering and replication (R>1) capabilities provide inherent resilience.
