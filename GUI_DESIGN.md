# SPINDEX GUI Design Document

## 1. Introduction

The SPINDEX GUI provides a user-friendly interface for interacting with the SPINDEX (SPatial INDex EXplorer) system. Its primary purpose is to allow users to easily create, manage, query, and visualize spatial indexes.

The target users for this GUI include:

*   **GIS Analysts:** Professionals who work with geospatial data and need to perform spatial queries and analyses efficiently.
*   **Database Administrators:** Individuals responsible for managing spatial databases and optimizing query performance.
*   **Software Developers:** Programmers building applications that leverage spatial indexing for location-based services or data retrieval.
*   **Researchers:** Academics and scientists working with large-scale spatial datasets.

The GUI aims to simplify complex spatial indexing operations, making them accessible to users with varying levels of technical expertise.

## 2. High-Level Architecture

The SPINDEX GUI will be a **web-based application**. This choice offers cross-platform compatibility, ease of deployment, and accessibility from any device with a web browser.

### 2.1 Asynchronous Architecture

To handle potentially long-running operations such as index creation, complex spatial queries, or large data visualizations without blocking the user interface, SPINDEX will employ an asynchronous architecture. This will be facilitated by a task queue/message broker (e.g., RabbitMQ, Redis, or Apache Kafka).

**Key aspects:**

*   **Task Queue/Message Broker:**
    *   **Purpose:** Decouples the frontend and immediate backend API responses from time-consuming background tasks.
    *   **Mechanism:** When a user initiates a long-running job (e.g., "Create New Index"), the backend API will submit a task to the message broker and immediately return a task ID or acknowledgment to the UI.
*   **Worker Processes:**
    *   Separate worker processes will listen to the task queue, pick up tasks, and execute them (e.g., interact with the SPINDEX Core Engine).
*   **Status Tracking & Notifications:**
    *   The UI will periodically poll the backend for task status using the task ID, or the backend will push status updates to the UI (e.g., via WebSockets).
    *   Users will be able to see the progress of their tasks (e.g., "Pending," "In Progress," "Completed," "Failed").
*   **Result Retrieval:**
    *   Once a task is completed, results (if any) can be stored temporarily or linked for retrieval via the backend API.
*   **Benefits:**
    *   **Improved UI Responsiveness:** The GUI remains responsive even when intensive operations are underway.
    *   **Scalability:** Worker processes can be scaled independently to handle varying loads.
    *   **Resilience:** If a worker fails, tasks can often be requeued and retried (depending on task idempotency).

The architecture will consist of the following main components:

*   **Frontend (Client-Side):**
    *   Built using a modern JavaScript framework (e.g., React, Vue, or Angular).
    *   Responsible for rendering the user interface, handling user interactions, and communicating with the backend.
    *   Will include components for displaying maps, forms, tables, and visualizations.
*   **Backend (Server-Side):**
    *   Developed using a robust backend language (e.g., Python with Flask/Django, Node.js with Express).
    *   Provides a RESTful API for the frontend to interact with.
    *   Handles business logic, data validation, and communication with the SPINDEX core engine and the underlying database.
*   **SPINDEX Core Engine:**
    *   The existing SPINDEX library/executable responsible for creating, managing, and querying spatial indexes. The backend will act as a wrapper or interface to this engine.
*   **Database:**
    *   Stores metadata about the spatial indexes, user configurations, task statuses, and potentially cached query results. This could be a relational database (e.g., PostgreSQL with PostGIS) or a NoSQL database depending on the specific needs of SPINDEX.
*   **Task Queue/Message Broker:** (e.g., RabbitMQ, Redis)
    *   Manages asynchronous tasks as described in section 2.1.

### 2.2 Interaction Flow (incorporating Asynchronous Operations)

1.  The user interacts with the frontend through their web browser.
2.  Frontend sends requests to the backend API.
    *   For **short operations** (e.g., fetching index list): The backend processes the request synchronously and returns a direct response.
    *   For **long-running operations** (e.g., creating a large index):
        a.  Backend API validates the request and submits a task to the Task Queue/Message Broker, returning a task ID and an immediate acknowledgment (e.g., "Index creation started").
        b.  A separate Worker Process picks up the task from the queue.
        c.  The Worker Process interacts with the SPINDEX Core Engine and the Database as needed.
        d.  The Worker Process updates the task status in the Database.
        e.  The UI can poll a dedicated API endpoint for status updates using the task ID, or receive updates via WebSockets.
3.  For synchronous operations, the backend sends a response (e.g., success message, query results, error) back to the frontend.
4.  For asynchronous operations, once the task is complete, the frontend is notified, and results can be fetched or displayed.
5.  Frontend updates the UI to reflect the response or task status.

## 3. Features

The following SPINDEX features will be accessible through the GUI:

### 3.1. Index Management

The GUI will provide functionalities to manage the lifecycle of spatial indexes. Index metadata and creation processes are managed via backend API endpoints (e.g., `/indexes`).

*   **Create New Index (Post-AEGIS Pipeline):**
    *   **GUI Presentation:** The primary method for creating indexes will be through an "Enhanced Index Creation Wizard" (detailed in `gui/GUI_MOCKUPS.md`, section 7). This wizard guides the user in creating an index from data that has been successfully processed and validated by the AEGIS-ECHO pipeline.
    *   **Backend Interaction:**
        *   The wizard collects information such as the path to the promoted (processed) data file (`promoted_data_path`), a user-defined `index_name`, the desired `index_type` (e.g., "R-Tree"), and any specific `index_parameters`.
        *   This information is submitted as an `IndexCreationRequest` to the `POST /indexes` backend endpoint.
        *   The backend generates a unique `index_id`, creates an initial `IndexMetadata` record with `status='creating'` and `version=1`, stores this metadata (currently in-memory `index_metadata_db`), and dispatches an asynchronous Celery task (`create_spindex_task`) to handle the actual index construction by the SPINDEX core (simulated).
        *   The API returns the newly created `IndexMetadata` object, including the `celery_task_id` for monitoring.
    *   **User Actions:**
        *   Select a processed dataset (output from AEGIS pipeline, status `validation_successful_ready_for_promotion`).
        *   Review AEGIS validation summaries (format, CRS, geometric validation).
        *   Configure index parameters (name, type, specific settings).
        *   Initiate index creation.
        *   Monitor creation status (e.g., in the Index Management view or via job status notifications). The `IndexMetadata.status` will reflect this.
*   **View Existing Indexes:**
    *   **GUI Presentation:** A sortable and filterable table in the "Index Management" view.
    *   **Backend Interaction:** Fetches a list of `IndexMetadata` objects from the `GET /indexes` endpoint.
    *   **User Actions:**
        *   View key details from `IndexMetadata`: `index_id`, `index_name`, `index_type`, `promoted_data_path` (source data), `status` (e.g., 'creating', 'ready', 'failed'), `version`, `created_at`.
        *   Select an index to view more details or perform actions.
*   **Delete Index:**
    *   **GUI Presentation:** A button or context menu option associated with each index in the list.
    *   **Backend Interaction:** Sends a request to `DELETE /indexes/{index_id}`.
    *   **User Actions:** Confirm deletion of the selected index. The backend removes the `IndexMetadata` record. (Note: Actual deletion of index files by SPINDEX core is a deeper backend process not fully stubbed).
*   **Index Details/Statistics:**
    *   **GUI Presentation:** A dedicated view or panel showing detailed information about a selected index.
    *   **Backend Interaction:** Fetches the specific `IndexMetadata` object from `GET /indexes/{index_id}`.
    *   **User Actions:**
        *   View all fields from the `IndexMetadata` record, including `index_parameters` used, `version`, `status`, timestamps, and path to source data.
        *   (Future) View statistics like item count, bounding box, tree depth (these would be added to `IndexMetadata` after actual index creation by SPINDEX core).
        *   (Future) Visualize the index structure.
        *   Access different versions of the index (see section 3.1.1).

### 3.1.1. Index Versioning & Integrity Verification

To ensure reproducibility, enable rollback capabilities, and provide audit trails, SPINDEX will support index versioning and integrity verification, managed through the `IndexMetadata` stored by the backend.

*   **Mechanism:**
    *   **Versioning:**
        *   Each `IndexMetadata` record has an `index_id` (persistent across versions of the same conceptual index - this might need refinement, e.g., a separate `conceptual_index_id` and `version_id` or treat each `index_id` as unique per version) and a `version` number.
        *   Initial index creation via `POST /indexes` sets `version = 1`.
        *   Future operations like re-indexing the same dataset with different parameters, or indexing an updated version of the source data, would conceptually result in a new `IndexMetadata` record with an incremented `version` number, potentially sharing a common logical name or a foreign key to a parent index concept. (The exact mechanism for managing conceptual groups of indexes vs. versions is a deeper design point; current backend stubs create unique `index_id`s for each entry).
        *   The `promoted_data_path` field in `IndexMetadata` links a specific index version to the exact processed data file it was built from.
    *   **Cryptographic Hash (Conceptual):**
        *   While not yet implemented in the backend Pydantic models or Celery tasks, the design intends for each version of an index (or its source `promoted_data_path`) to have an associated cryptographic hash (e.g., SHA-256).
        *   This hash would be computed during or after the AEGIS pipeline (for the `promoted_data_path`) and/or after index creation by the SPINDEX core.
        *   This hash would be stored as part of the `IndexMetadata` (e.g., `data_hash_processed`, `index_structure_hash`).
    *   **Storage:** Index version information (version number, link to data, creation/update timestamps, status) is stored within the `IndexMetadata` model in the `index_metadata_db` (in-memory list for now).
*   **GUI Presentation:**
    *   The "Index Details" view could list different available versions if multiple `IndexMetadata` entries share a conceptual link (e.g., same `index_name` but different `version` numbers or `created_at` times).
    *   (Future) Display the cryptographic hash for data integrity verification.
*   **User Actions:**
    *   **View Versions:** Users could compare different versions of an index (based on name and version number in the `IndexMetadata`).
    *   **Rollback (Future Enhancement):** This would involve selecting a specific, older `IndexMetadata` version and making it the "active" or "default" version for querying, or re-creating an index based on its parameters and `promoted_data_path`.
    *   **Audit:** Hashes (when implemented) can be used to verify data and index integrity.
*   **Benefits:**
    *   **Reproducibility:** Ensures that queries against a specific index version always use the exact same data and structure.
    *   **Rollback Capability:** Allows reverting to a known good state if a new index version is corrupted or produces unexpected results.
    *   **Auditability & Integrity:** Provides a verifiable way to ensure that an index has not been tampered with.

### 3.2. Spatial Querying

*   **Perform Spatial Query:**
    *   **GUI Presentation:** A query interface, potentially integrated with a map view.
    *   **User Actions:**
        *   Select an existing index to query against.
        *   Choose query type (e.g., range query, nearest neighbor query, point query, polygon intersection).
        *   Define query parameters:
            *   For range query: draw a bounding box on a map or input coordinates.
            *   For nearest neighbor: specify a point (click on map or input coordinates) and the number of neighbors (k).
            *   For point query: specify a point.
            *   For polygon intersection: draw a polygon on a map or upload a geometry file (e.g., GeoJSON, WKT).
        *   Execute the query.
*   **View Query Results:**
    *   **GUI Presentation:**
        *   Results displayed in a table.
        *   Queried items highlighted or plotted on a map.
        *   Option to view attributes of the result items.
    *   **User Actions:**
        *   Paginate through results.
        *   Sort and filter results.
        *   Export query results (e.g., CSV, GeoJSON).
        *   Select a result item to view its details or location on the map.

### 3.3. Data Visualization

*   **Visualize Indexed Data:**
    *   **GUI Presentation:** A map view displaying the spatial distribution of data within a selected index.
    *   **User Actions:**
        *   Pan and zoom on the map.
        *   Toggle display of data points, bounding boxes, or heatmaps.
        *   Click on data points to see their attributes.
*   **Visualize Index Structure (Advanced):**
    *   **GUI Presentation:** For tree-based indexes, an option to overlay the tree structure (e.g., R-tree node boundaries) on the map.
    *   **User Actions:** Control the level of detail shown for the index structure.

### 3.4. Configuration & Settings

*   **Manage Data Sources:**
    *   **GUI Presentation:** A section to configure connections to databases or specify paths to data files.
    *   **User Actions:** Add, edit, or remove data source configurations.
*   **User Preferences:**
    *   **GUI Presentation:** A settings panel.
    *   **User Actions:** Customize aspects like default map view, theme (light/dark), notification preferences.

## 4. User Interface Design

### 4.1. Layout

A **multi-pane interface** is proposed:

*   **Left Sidebar (Navigation Pane):**
    *   Contains links to major sections: Dashboard, Index Management, Query Interface, Data Visualization, Settings.
    *   Collapsible to maximize content area.
*   **Main Content Area:**
    *   Displays the active view based on the selection in the navigation pane.
*   **Top Header Bar:**
    *   Application logo/name.
    *   User profile/logout button.
    *   Global search (optional, for searching indexes or documentation).
    *   Notifications icon.
*   **Status Bar (Optional, at the bottom):**
    *   Displays information about ongoing processes or system status.

### 4.2. Views

#### 4.2.1. Dashboard View

*   **Purpose:** Provides an overview of the SPINDEX system and quick access to common actions.
*   **Components:**
    *   Summary statistics (e.g., total number of indexes, recent queries).
    *   Quick links: "Create New Index," "Go to Query Interface."
    *   List of recently accessed or modified indexes.
    *   Potentially a small map showing an overview of all indexed data extents.
*   **User Interactions:** Click on links to navigate, view summaries.

#### 4.2.2. Index Management View

*   **Purpose:** Allows users to create, view, and manage spatial indexes.
*   **Components:**
    *   **Index List Table:**
        *   Columns: Name, Type, Data Source, Creation Date, Size, Status, Actions (View Details, Query, Delete).
        *   Filtering and sorting options.
        *   "Create New Index" button.
    *   **Index Creation Form (Modal or separate sub-view):**
        *   Fields for index name, type, parameters, data source selection, attribute mapping.
        *   Progress indicator during index creation.
    *   **Index Details Panel/View:**
        *   Displayed when an index is selected from the list.
        *   Shows detailed information, statistics, and visualization options for the index structure.
        *   Map display to show index extent and structure.
*   **User Interactions:** Create, delete, sort, filter indexes. View details and statistics. Initiate queries from an index.

#### 4.2.3. Query Interface View

*   **Purpose:** Enables users to perform spatial queries on existing indexes.
*   **Components:**
    *   **Index Selection:** Dropdown or searchable list to choose the index to query.
    *   **Query Type Selection:** Radio buttons or dropdown for selecting query type (Range, KNN, Point, Polygon).
    *   **Parameter Input Area:** Dynamic form fields based on the selected query type.
        *   Interactive map for drawing query geometries (bounding box, polygon) or selecting points.
        *   Input fields for coordinates, k-value, etc.
    *   "Execute Query" button.
    *   **Results Area:**
        *   Tabular display of query results (attributes).
        *   Map display showing query geometry and result features.
        *   Options to export results.
*   **User Interactions:** Select index, choose query type, define parameters (graphically or via input), execute query, view and export results.

#### 4.2.4. Data Visualization View

*   **Purpose:** Provides tools for visualizing indexed datasets and query results on a map.
*   **Components:**
    *   **Index/Data Selection:** Choose which indexed dataset(s) to visualize.
    *   **Map Display:** Main component showing spatial data.
    *   **Layer Control:** Toggle visibility of different layers (e.g., raw data points, index structure, query results, heatmaps).
    *   **Styling Options:** Basic options to change colors, point sizes for visualization.
    *   **Query Tools Integration:** Ability to initiate queries directly from the visualized data.
*   **User Interactions:** Pan, zoom, select layers, interact with map features to get more information.

#### 4.2.5. Settings View

*   **Purpose:** Allows users to configure data sources and user preferences.
*   **Components:**
    *   **Data Source Management Tab:**
        *   List of configured data sources.
        *   Form to add/edit data source connections (database credentials, file paths).
    *   **User Preferences Tab:**
        *   Options for theme, default map settings, notification settings.
*   **User Interactions:** Add, edit, remove data sources. Modify user preferences.

### 4.3. Navigation

*   **Primary Navigation:** Via the left sidebar, allowing users to switch between the main views (Dashboard, Index Management, Query Interface, etc.).
*   **Secondary Navigation:**
    *   Buttons and links within views (e.g., "Create New Index" button, clicking an index name to see details).
    *   Breadcrumbs (optional, for deeply nested views) to show the user's current location in the application hierarchy.
*   **Browser Navigation:** The application should support browser back/forward buttons by properly managing URL routes for different views.

## 5. Technology Stack

*   **Frontend:**
    *   **Framework:** **React** or **Vue.js**.
        *   **Justification:** Both have large communities, rich ecosystems of libraries, and are well-suited for building interactive single-page applications (SPAs). React offers strong component-based architecture, while Vue.js is known for its gentle learning curve and excellent documentation. The choice might depend on team familiarity.
    *   **Mapping Library:** **MapLibre GL JS** (open-source fork of Mapbox GL JS) or **OpenLayers**.
        *   **Justification:** Powerful, high-performance libraries for rendering interactive maps. MapLibre GL JS offers excellent vector tile support and GPU-accelerated rendering. OpenLayers is very flexible and supports a wide range of OGC standards.
    *   **State Management:** **Redux** (for React) or **Vuex** (for Vue.js).
        *   **Justification:** Essential for managing complex application state, especially with multiple interacting components.
    *   **UI Component Library:** **Material-UI** (for React), **Vuetify** (for Vue.js), or **Bootstrap**.
        *   **Justification:** Provides pre-built, customizable UI components to speed up development and ensure a consistent look and feel.
    *   **Build Tool:** **Webpack** or **Vite**.
        *   **Justification:** Modern build tools for bundling JavaScript, CSS, and other assets. Vite offers a significantly faster development experience.
*   **Backend:**
    *   **Language/Framework:** **Python** with **Flask** or **FastAPI**.
        *   **Justification:** Python is widely used in data science and GIS, with many relevant libraries. Flask is lightweight and flexible, suitable for building REST APIs. FastAPI is modern, high-performance, and includes automatic data validation and API documentation (Swagger UI).
    *   **Alternative Backend:** **Node.js** with **Express.js**.
        *   **Justification:** If the team has strong JavaScript expertise, using Node.js allows for a full-stack JavaScript development experience. Express.js is a mature and popular framework.
*   **Database (for metadata):**
    *   **PostgreSQL with PostGIS extension.**
        *   **Justification:** Robust open-source relational database with excellent spatial capabilities provided by PostGIS. Ideal if SPINDEX itself or its data sources involve complex geometries or require spatial SQL functions.
    *   **SQLite:**
        *   **Justification:** Simpler setup for smaller deployments or if spatial capabilities are primarily handled by the SPINDEX core and not the metadata store.
*   **API Design:**
    *   **RESTful API** with **OpenAPI (Swagger)** specification.
        *   **Justification:** Standardized approach for web APIs, well-understood, and OpenAPI provides excellent documentation and client generation tools. All API endpoints must enforce security measures as outlined in Section 7.

## 6. Future Enhancements

*   **Real-time Collaboration:** Allow multiple users to view or work on the same indexes/queries simultaneously (would require WebSockets).
*   **Batch Processing:** Interface for running batch operations, such as creating multiple indexes or running a series of queries.
*   **Integration with External Services:** Connect to cloud storage providers (S3, Azure Blob) for data sources, or integrate with other GIS platforms.
*   **Enhanced Visualization Options:** More sophisticated charting, 3D visualization, temporal data animation.
*   **Plugin Architecture:** Allow developers to extend GUI functionality with custom plugins.
*   **Automated Reporting:** Generate reports on index usage, query performance, etc.
*   **Mobile Responsiveness:** Ensure the GUI is usable on tablets and potentially mobile phones for viewing data or simple queries.
*   **Natural Language Querying:** Allow users to type queries in plain English (e.g., "show all parks near downtown").
*   **Machine Learning Integration:** Integrate ML models for predictive spatial analysis or anomaly detection based on indexed data.
*   **Advanced Authentication (ZKP):** Explore Zero-Knowledge Proofs for enhanced privacy-preserving authentication, building upon the flexible authentication model.
*   **Accessibility (a11y) Improvements:** Ensure the GUI is compliant with WCAG standards for users with disabilities.

## 7. Security and Access Control

This section details the foundational security framework for the SPINDEX GUI and backend systems.

### 7.1. Role-Based Access Control (RBAC)

SPINDEX will implement RBAC to ensure that users only have access to functionalities and data appropriate for their roles.

*   **Defined Roles:**
    *   **Admin:**
        *   **Permissions:** Full control over the system.
            *   User management (create, edit, delete users, assign roles).
            *   System configuration (modify global settings, manage data sources).
            *   Full index management (create, view, edit, delete all indexes).
            *   View system-wide logs and audit trails.
            *   Can perform all actions available to Analyst and Viewer roles.
    *   **Analyst:**
        *   **Permissions:** Focused on data interaction and index management for their own or shared resources.
            *   Index management: Create new indexes, view/edit/delete indexes they own or have explicit permission for.
            *   Perform spatial queries on accessible indexes.
            *   Visualize data from accessible indexes.
            *   Manage their own data sources (if applicable).
            *   Cannot manage users (except perhaps their own profile) or system-wide configurations.
    *   **Viewer:**
        *   **Permissions:** Read-only access to specific indexes or data visualizations.
            *   View details of indexes they have been granted access to.
            *   Perform spatial queries on indexes explicitly shared with them (results might be limited).
            *   View shared data visualizations.
            *   Cannot create, edit, or delete indexes.
            *   Cannot change system settings or manage data sources.

*   **Implementation Notes:**
    *   Permissions will be granular and associated with specific API endpoints and operations.
    *   User roles will be assigned during user creation or by an Admin.
    *   The backend will enforce these permissions on every API request.

### 7.2. End-to-End Encryption

*   **Data-in-Transit:**
    *   **Protocol:** TLS 1.3 (Transport Layer Security) will be mandatory for all API communications between the client (browser) and the backend server, as well as between backend services if applicable (e.g., backend to database, backend to SPINDEX Core Engine if over a network).
    *   **Certificate Management:** A robust process for managing TLS certificates (e.g., using Let's Encrypt or an internal CA) will be implemented.
*   **Data-at-Rest:**
    *   **Encryption Standard:** Sensitive metadata stored in the SPINDEX database will be encrypted using AES-256-GCM (Galois/Counter Mode) or an equivalent strong symmetric encryption algorithm.
    *   **Scope of Encryption:** This applies to:
        *   Credentials for configured data sources.
        *   User Personally Identifiable Information (PII), if any is stored (e.g., email addresses if used for notifications, user profile details).
        *   Sensitive configuration parameters.
    *   **Key Management:** A secure key management system (e.g., HashiCorp Vault, AWS KMS, or OS-level key protection) will be used to manage encryption keys. Keys will not be stored alongside the encrypted data in plain text.

### 7.3. Authentication Model

SPINDEX will implement a strong, modern authentication model to verify the identity of users and clients.

*   **Baseline Model: DPoP/Sender-Constrained JWTs over mTLS**
    *   **mTLS (Mutual TLS):** Client and server will authenticate each other using TLS certificates. This provides a secure channel and initial client authentication. The client certificate's public key can be used as part of the DPoP proof.
    *   **JWTs (JSON Web Tokens):** After successful initial authentication (e.g., username/password login, or mTLS-based device authentication), the authentication server will issue JWTs.
    *   **DPoP (Demonstration of Proof-of-Possession):** To prevent token theft and replay attacks, DPoP will be used.
        *   The client creates a DPoP proof by signing parts of the HTTP request (e.g., HTTP method, URL, and a unique nonce) along with a hash of the public key associated with the JWT (`cnf` claim) using a private key it possesses.
        *   This DPoP proof is sent as an HTTP header.
        *   The server verifies the DPoP proof, ensuring the JWT is presented by the legitimate owner of the associated private key.
    *   **Sender-Constrained:** The JWT is bound to the client's public key (via the `cnf` claim containing a hash of the public key), ensuring only the client possessing the corresponding private key can use the token with DPoP.
*   **Token Issuance:** An identity provider (IdP), which could be built-in or an external OAuth2/OIDC server, will be responsible for issuing JWTs after primary user authentication (e.g., credentials, SSO).
*   **Flexibility for Future Enhancements:**
    *   The authentication architecture should be modular to allow for future integration of more advanced or alternative authentication mechanisms, such as:
        *   **Zero-Knowledge Proofs (ZKP):** For scenarios requiring enhanced privacy-preserving authentication where the user proves their identity without revealing sensitive information.

This baseline model provides a strong defense against common web vulnerabilities, including token interception and replay.
