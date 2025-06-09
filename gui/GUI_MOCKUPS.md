# SPINDEX GUI Mockups

This document provides textual mockups for the key views of the SPINDEX GUI. These mockups are based on the `GUI_DESIGN.md` document and describe the layout and components of each view.

## Common Elements

All views operate within the main application shell:

*   **Header:** Displays "SPINDEX" title on the left, User information and Logout button on the right.
*   **Sidebar (Left Pane):** Contains navigation links. Can be collapsed.
    *   Button: `☰` (Toggle Sidebar)
    *   Nav Links: Dashboard, Index Management, Query Interface, Data Visualization, Settings.
*   **Main Content Area (Right Pane):** Renders the selected view.
*   **Footer/Status Bar:** Displays copyright information and a status message (e.g., "Status: Ready").

---

## 1. Dashboard View

*   **Purpose:** Provides an overview of the SPINDEX system and quick access to common actions.
*   **Layout:** Single pane within the Main Content Area.

```
+--------------------------------------------------------------------------+
| Header: [SPINDEX]                                [User | Logout]        |
+--------------------------------------------------------------------------+
| Sidebar:                                                                 |
|   [☰ Collapse]                                                           |
|   - Dashboard (Active)                                                   |
|   - Index Management                                                     |
|   - Query Interface                                                      |
|   - Data Visualization                                                   |
|   - Settings                                                             |
|--------------------------------------------------------------------------|
| Main Content Area: Dashboard                                             |
|                                                                          |
|   ## Dashboard                                                          |
|                                                                          |
|   +---------------------------------+ +---------------------------------+  |
|   | Quick Stats                     | | Recent Activity                 |  |
|   |---------------------------------| |---------------------------------|  |
|   | - Total Indexes: 5              | | - Index 'poi_data' queried    |  |
|   | - Active Data Sources: 2        | | - Index 'city_boundaries' created |
|   | - Recent Queries (24h): 12      | | - ...                           |  |
|   +---------------------------------+ +---------------------------------+  |
|                                                                          |
|   ### Quick Actions                                                      |
|   [Button: Create New Index] [Button: Go to Query Interface]             |
|                                                                          |
|   ### Recently Accessed Indexes                                          |
|   - [Link: `idx_points_of_interest`] (Type: R-Tree, Items: 150K)         |
|   - [Link: `idx_admin_boundaries`] (Type: Quadtree, Items: 10K)          |
|   - ...                                                                  |
|                                                                          |
|   (Optional: Small map overview showing general data extents)            |
|                                                                          |
+--------------------------------------------------------------------------+
| Footer: [© 2023 SPINDEX Project]                  [Status: Ready]        |
+--------------------------------------------------------------------------+
```

**Components:**

*   **Summary Statistics Section:**
    *   Displays key metrics like "Total Indexes," "Active Data Sources," "Recent Queries."
*   **Recent Activity Feed:**
    *   Lists recent actions performed in the system.
*   **Quick Actions Buttons:**
    *   `[Button: Create New Index]`
    *   `[Button: Go to Query Interface]`
*   **Recently Accessed Indexes List:**
    *   Links to detail views of recently used indexes, with brief info.

---

## 2. Index Management View

*   **Purpose:** Allows users to create new spatial indexes (primarily from AEGIS-processed data), view existing indexes, and manage them.
*   **Layout:**
    *   A button to launch the "Enhanced Index Creation Wizard".
    *   A table (`#indexes-table`) listing existing spatial indexes.
    *   An area (`#index-details-area`) to display details of a selected index.

```
+--------------------------------------------------------------------------+
| Header: [SPINDEX]                                [User | Logout]        |
+--------------------------------------------------------------------------+
| Sidebar:                                                                 |
|   [☰ Collapse]                                                           |
|   - Dashboard                                                            |
|   - Index Management (Active)                                            |
|   ...                                                                    |
|--------------------------------------------------------------------------|
| Main Content Area: Index Management                                      |
|                                                                          |
|   ## Index Management                                                    |
|                                                                          |
|   [Button: Create New Index from Processed Data] (id='show-create-index-wizard-button') |
|                                                                          |
|   ### Available Indexes                                                  |
|   <!-- Add Filter/Search options here in future -->                      |
|   <table id="indexes-table">                                             |
|     <thead>                                                              |
|       <tr>                                                               |
|         <th>ID</th><th>Name</th><th>Type</th><th>Status</th><th>Version</th> |
|         <th>Created At</th><th>Actions</th>                               |
|       </tr>                                                              |
|     </thead>                                                             |
|     <tbody>                                                              |
|       <!-- Example Row (populated by JS from GET /indexes) -->          |
|       <tr>                                                               |
|         <td>idx_abc123</td><td>My First RTree</td><td>R-Tree</td><td>Ready</td> |
|         <td>1</td><td>2023-10-29 10:00</td>                              |
|         <td>[Details] [Query] [Delete]</td>                              |
|       </tr>                                                              |
|       <tr>                                                               |
|         <td>idx_def456</td><td>City Boundaries QTree</td><td>Quadtree</td><td>Creating</td>|
|         <td>1</td><td>2023-10-29 10:05</td>                              |
|         <td>[Details] [Delete]</td>                                      |
|       </tr>                                                              |
|     </tbody>                                                             |
|   </table>                                                               |
|                                                                          |
|   <div id="index-details-area" style="margin-top:20px; padding:15px; border:1px solid #eee; border-radius:5px; display:none;">                    |
|     <h4>Index Details (for Index ID: <span id="details-index-id"></span>)</h4>|
|     <pre id="index-details-content"></pre>                               |
|   </div>                                                                 |
|                                                                          |
+--------------------------------------------------------------------------+
| Footer: [© 2023 SPINDEX Project]                  [Status: Ready]        |
+--------------------------------------------------------------------------+
```

**Components:**

*   **"Create New Index from Processed Data" Button (`#show-create-index-wizard-button`):**
    *   Launches the "Enhanced Index Creation Wizard" modal (see Section 7).
*   **Indexes Table (`#indexes-table`):**
    *   Displays a list of all spatial indexes with their metadata.
    *   Populated by JavaScript making a `GET` request to the `/indexes` backend endpoint.
    *   Columns: Index ID, Name, Type, Status (e.g., 'creating', 'ready', 'failed'), Version, Created At, Actions.
    *   **Actions Column:**
        *   `[Details]` button: Calls `viewIndexDetails(indexId)` to fetch and display full metadata for the index in the `#index-details-area`.
        *   `[Query]` button (Future): Would navigate to the Query Interface, pre-selecting this index.
        *   `[Delete]` button: Calls `deleteIndex(indexId, indexName)` to request deletion from the backend via `DELETE /indexes/{index_id}`.
*   **Index Details Area (`#index-details-area`):**
    *   Initially hidden. Becomes visible when a "Details" button is clicked.
    *   Displays the full JSON (or formatted details) of an `IndexMetadata` object fetched from `/indexes/{index_id}`.
*   **(Wizard is a separate modal, detailed in Section 7)**

---

## 3. Query Interface View

*   **Purpose:** Allows users to select a 'ready' spatial index, choose a query type, input parameters, execute queries against the backend (`POST /query/execute`), and view results. It also displays a history of past queries.
*   **Layout:** A main form for query setup, an area for results, and a list for query history.
*   **Loading Indicators:**
    *   Index selection dropdown shows "Loading..." or a spinner icon while fetching indexes.
    *   Query results area shows "Executing query..." or a spinner icon while waiting for results.
    *   Query history list shows "Loading history..." or skeleton line items while fetching.

```
+--------------------------------------------------------------------------+
| Header: [SPINDEX]                      [Logged in as: testuser (analyst) | Logout] |
+--------------------------------------------------------------------------+
| Sidebar:                                                                 |
|   [☰ Collapse]                                                           |
|   - Dashboard                                                            |
|   - Index Management                                                     |
|   - Query Interface (Active)                                             |
|   ...                                                                    |
|--------------------------------------------------------------------------|
| Main Content Area: Query Interface                                       |
|                                                                          |
|   ## Query Interface                                                     |
|                                                                          |
|   <div class="query-form-container">                                     |
|     <label for="query-index-select">Select Index:</label>                  |
|     <select id="query-index-select">                                     |
|       <option value="">-- Select an Index --</option>                    |
|       <option value="idx_abc123">My First RTree (ID: idx_abc123)</option> |
|       <!-- Options populated by JS from GET /indexes?status=ready -->    |
|       <!-- Shows spinner/loading text during fetch -->                   |
|     </select>                                                            |
|                                                                          |
|     <label for="query-type-select">Query Type:<span class='help-tooltip' title='Select the type of spatial query to perform.'>?</span></label> |
|     <select id="query-type-select">                                      |
|       <option value="bbox">Bounding Box</option>                         |
|       <option value="point_radius">Point with Radius</option>             |
|       <!-- Other query types like KNN can be added -->                   |
|     </select>                                                            |
|                                                                          |
|     <div id="query-params-inputs">                                       |
|       <!-- Inputs for BBox (Default) -->                                 |
|       <label>Min X:</label> <input type="number" id="query-bbox-minx" />   |
|       <label>Min Y:</label> <input type="number" id="query-bbox-miny" />   |
|       <label>Max X:</label> <input type="number" id="query-bbox-maxx" />   |
|       <label>Max Y:</label> <input type="number" id="query-bbox-maxy" />   |
|     </div>                                                               |
|                                                                          |
|     <button id="execute-query-button">Execute Query</button>             |
|   </div>                                                                 |
|                                                                          |
|   <div class="query-results-container">                                  |
|     <h3>Query Results</h3>                                               |
|     <div id="query-results-summary">                                     |
|       <!-- e.g., "Found 5 results in 25ms." -->                         |
|     </div>                                                               |
|     <pre id="query-results-json">                                        |
|       <!-- JSON results will be displayed here -->                        |
|       <!-- Or "Executing query..." / Spinner -->                          |
|     </pre>                                                               |
|   </div>                                                                 |
|                                                                          |
|   <div class="query-history-container">                                  |
|     <h3>Query History</h3>                                               |
|     <ul id="query-history-list">                                         |
|       <!-- e.g., <li>2023-10-30: bbox on idx_abc123 - Params: {...}</li> --> |
|       <!-- Or "Loading history..." / Skeleton lines -->                  |
|     </ul>                                                                |
|   </div>                                                                 |
|                                                                          |
+--------------------------------------------------------------------------+
| Footer: [© 2023 SPINDEX Project]                  [Status: Ready]        |
+--------------------------------------------------------------------------+
```

**Components & Interactions:**

*   **Select Index (`#query-index-select`):**
    *   Dropdown populated by JavaScript (`populateQueryIndexSelect`) calling `GET /indexes` and filtering for indexes with `status: 'ready'`. Shows a loading message/spinner during fetch.
*   **Query Type (`#query-type-select`):**
    *   Dropdown to select query type (e.g., "Bounding Box", "Point with Radius"). Includes a help tooltip.
    *   Changing this dynamically updates the `#query-params-inputs` div via `updateQueryParamsUI` JavaScript function.
*   **Query Parameter Inputs (`#query-params-inputs`):**
    *   Dynamically generated input fields based on selected query type.
    *   Example for "Bounding Box": Min X, Min Y, Max X, Max Y.
    *   Example for "Point with Radius": Point X, Point Y, Radius.
*   **Execute Query Button (`#execute-query-button`):**
    *   Triggers `executeQuery()` JavaScript function.
    *   Collects `index_id`, `query_type`, and `parameters` from the form.
    *   POSTs to `/query/execute` backend endpoint.
    *   Displays a loading state/spinner in the results area (`#query-results-json`) during execution.
    *   On response, displays results count and time in `#query-results-summary` and the JSON results in `#query-results-json`.
    *   Refreshes the query history by calling `loadQueryHistory()`.
*   **Query Results Summary (`#query-results-summary`):** Displays count of results and query execution time.
*   **Query Results JSON (`#query-results-json`):** Displays the raw JSON (dummy) results from the backend. Shows a spinner during fetch.
*   **Query History List (`#query-history-list`):**
    *   Populated by `loadQueryHistory()` calling `GET /query/history`. Shows loading state/skeleton lines during fetch.
    *   Lists past queries with timestamp, type, index ID, and parameters.
    *   List items are clickable to (partially) re-populate the query form.

---

## 4. Data Visualization View

*   **Purpose:** Provides tools for visualizing indexed datasets and query results.
*   **Layout:** Primarily a large map display with layer controls.

```
+--------------------------------------------------------------------------+
| Header: [SPINDEX]                                [User | Logout]        |
+--------------------------------------------------------------------------+
| Sidebar:                                                                 |
|   [☰ Collapse]                                                           |
|   - Dashboard                                                            |
|   - Index Management                                                     |
|   - Query Interface                                                      |
|   - Data Visualization (Active)                                          |
|   - Settings                                                             |
|--------------------------------------------------------------------------|
| Main Content Area: Data Visualization                                    |
|                                                                          |
|   ## Data Visualization                                                  |
|                                                                          |
|   +--------------------------+-------------------------------------------+
|   | Layer & Style Controls   | Map Display                               |
|   |--------------------------|-------------------------------------------|
|   | Select Index to View:    | +---------------------------------------+ |
|   | [Dropdown: idx_A, idx_B] | |                                       | |
|   |                          | |                                       | |
|   | --- Layers ---           | |                                       | |
|   | [Checkbox] Data Points   | |           MAP AREA                    | |
|   | [Checkbox] Index Structure| |                                       | |
|   |    (e.g., R-Tree BBoxes) | |                                       | |
|   | [Checkbox] Heatmap       | |                                       | |
|   |                          | |                                       | |
|   | --- Styling ---          | +---------------------------------------+ |
|   | Point Color: [Color Picker]|                                         |
|   | Point Size: [Slider]     |                                         |
|   | BBox Opacity: [Slider]   |                                         |
|   |                          |                                         |
|   | [Button: Apply Styles]   |                                         |
|   | [Button: Reset View]     |                                         |
|   +--------------------------+-------------------------------------------+
|                                                                          |
+--------------------------------------------------------------------------+
| Footer: [© 2023 SPINDEX Project]                  [Status: Visualizing idx_A] |
+--------------------------------------------------------------------------+
```

**Components:**

*   **Layer & Style Controls Pane (Left or Collapsible Panel):**
    *   **Index Selection Dropdown:** Choose which indexed dataset to visualize.
    *   **Layer Toggles:** Checkboxes to show/hide layers like:
        *   Raw data points
        *   Index structure (e.g., R-tree bounding boxes)
        *   Heatmap
    *   **Styling Options:** Controls for point color, size, opacity, etc.
    *   `[Button: Apply Styles]`
*   **Map Display (Main Area):**
    *   Large interactive map showing the selected data and layers.
    *   Pan and zoom controls.
    *   Popups on click to show feature attributes.

---

## 5. Settings View

*   **Purpose:** Allows users to configure data sources and user preferences.
*   **Layout:** Tabbed interface for different settings categories.

```
+--------------------------------------------------------------------------+
| Header: [SPINDEX]                                [User | Logout]        |
+--------------------------------------------------------------------------+
| Sidebar:                                                                 |
|   [☰ Collapse]                                                           |
|   - Dashboard                                                            |
|   - Index Management                                                     |
|   - Query Interface                                                      |
|   - Data Visualization                                                   |
|   - Settings (Active)                                                    |
|--------------------------------------------------------------------------|
| Main Content Area: Settings                                              |
|                                                                          |
|   ## Settings                                                            |
|                                                                          |
|   [Tab: Data Sources (Active)] [Tab: User Preferences] [Tab: System]     |
|   <hr>                                                                     |
|                                                                          |
|   --- Data Sources Tab Content ---                                       |
|   ### Manage Data Sources                                                |
|   [Button: Add New Data Source]                                          |
|                                                                          |
|   +--------------------------------------------------------------------+
|   | Name      | Type     | Connection Details        | Actions          |
|   |-----------|----------|---------------------------|------------------|
|   | Main DB   | PostgreSQL| host=localhost, db=spindex| [Edit] [Delete]  |
|   | Legacy GIS| Oracle   | user@service_name         | [Edit] [Delete]  |
|   | ...       | ...      | ...                       | ...              |
|   +--------------------------------------------------------------------+
|                                                                          |
|   --- Modal: Add/Edit Data Source (Appears on Add/Edit click) ---        |
|   |   Name: [Input: My DB Connection]                                    |
|   |   Type: [Dropdown: PostgreSQL, Oracle, MySQL, File System]         |
|   |   Host: [Input: db.example.com]                                      |
|   |   Port: [Input: 5432]                                                |
|   |   Database: [Input: gis_data]                                        |
|   |   Username: [Input: user]                                            |
|   |   Password: [Password Input]                                         |
|   |   [Button: Test Connection]                                          |
|   |   [Button: Save] [Button: Cancel]                                    |
|   -------------------------------------------------------------------    |
|                                                                          |
|   --- User Preferences Tab Content (Example) ---                         |
|   ### User Preferences                                                   |
|   Theme: [Radio: Light] [Radio: Dark]                                    |
|   Default Map Extent: [Input: xmin,ymin,xmax,ymax] [Button: Set from Current Map] |
|   Notifications: [Checkbox: Enable email notifications for long tasks]   |
|   [Button: Save Preferences]                                             |
|                                                                          |
|   --- System Tab Content (Example - Admin Only) ---                      |
|   ### System Configuration                                               |
|   Log Level: [Dropdown: INFO, DEBUG, WARN, ERROR]                        |
|   Max Concurrent Queries: [Input: 10]                                    |
|   [Button: Save System Config]                                           |
|                                                                          |
+--------------------------------------------------------------------------+
| Footer: [© 2023 SPINDEX Project]                  [Status: Ready]        |
+--------------------------------------------------------------------------+
```

**Components:**

*   **Tabs:** "Data Sources," "User Preferences," "System" (potentially admin-only).
*   **Data Sources Tab:**
    *   `[Button: Add New Data Source]`
    *   Table listing existing data source connections (Name, Type, Details, Actions: Edit, Delete).
    *   Modal for adding/editing data source details (type, connection string, credentials, test connection button).
*   **User Preferences Tab:**
    *   Options for theme (Light/Dark).
    *   Default map settings.
    *   Notification preferences.
    *   `[Button: Save Preferences]`
*   **System Tab (Admin):**
    *   Configuration for system-wide settings like log levels, resource limits.
    *   `[Button: Save System Config]`

---

## 6. Ingestion Dashboard View

*   **Purpose:** Allows users to upload new data files for processing, view the status of their ingestion jobs, and manage quarantined jobs.
*   **Layout:** Three main sections within the Main Content Area: 1. Upload New Data, 2. Ingestion Jobs, 3. Quarantined Jobs & Details.

```
+--------------------------------------------------------------------------+
| Header: [SPINDEX]                      [Logged in as: testuser (analyst) | Logout] |
+--------------------------------------------------------------------------+
| Sidebar:                                                                 |
|   [☰ Collapse]                                                           |
|   - Dashboard                                                            |
|   - Index Management                                                     |
|   - Query Interface                                                      |
|   - Data Visualization                                                   |
|   - Ingestion (Active)                                                   |
|   - Settings                                                             |
|--------------------------------------------------------------------------|
| Main Content Area: Ingestion Dashboard                                   |
|                                                                          |
|   ## Ingestion Dashboard                                                 |
|                                                                          |
|   +--------------------------------------------------------------------+
|   | Upload New Data                                                    |
|   |--------------------------------------------------------------------|
|   | [Choose File Button: No file chosen]  [Button: Upload File]        |
|   | Upload Status: [Text: e.g., "File uploaded. Job ID: xyz123"]        |
|   +--------------------------------------------------------------------+
|                                                                          |
|   +--------------------------------------------------------------------+
|   | Ingestion Jobs                                                     |
|   |--------------------------------------------------------------------|
|   | Job ID   | Filename      | Status                  | Uploaded At        | Actions |
|   |----------|---------------|-------------------------|--------------------|---------|
|   | xyz123   | mydata.geojson| Queued for Processing   | 2023-10-27 10:30 AM| [Details]|
|   | ...      | ...           | ...                     | ...                | ...     |
|   +--------------------------------------------------------------------+
|                                                                          |
|   +--------------------------------------------------------------------+
|   | Quarantined Jobs                                                   |
|   |--------------------------------------------------------------------|
|   | Job ID   | Original Filename | Quarantined Date | Error Summary      | Actions |
|   |----------|-------------------|------------------|--------------------|---------|
|   | job_q1   | error_file.shp  | 2023-10-25       | Invalid geometry   | [Details] [Retry] [Discard] |
|   | job_q2   | corrupt.zip     | 2023-10-26       | Security scan fail | [Details] [Retry] [Discard] |
|   +--------------------------------------------------------------------+
|                                                                          |
|   +--------------------------------------------------------------------+
|   | Quarantine Job Details (visible when "Details" is clicked)         |
|   |--------------------------------------------------------------------|
|   | #### Quarantine Details for Job: [job_q1]                          |
|   | <pre>                                                              |
|   | {                                                                  |
|   |   "job_id": "job_q1",                                              |
|   |   "original_filename": "error_file.shp",                           |
|   |   "processing_status": "quarantined_validation_failed",            |
|   |   "error_log_references": ["validation_failure: Geometric ..."],   |
|   |   ... (full metadata record) ...                                   |
|   | }                                                                  |
|   | </pre>                                                             |
|   +--------------------------------------------------------------------+
|                                                                          |
+--------------------------------------------------------------------------+
| Footer: [© 2023 SPINDEX Project]                  [Status: Ready]        |
+--------------------------------------------------------------------------+
```

**Components:**

*   **Upload New Data Section:**
    *   **File Input:** `<input type='file' id='file-input'>`.
    *   **Upload Button:** `<button id='upload-button'>Upload File</button>`.
    *   **Upload Status Display:** `<div id='upload-status'></div>`.
*   **Ingestion Jobs Section:**
    *   **Jobs Table:** `<table id='ingestion-jobs-table'>`.
        *   Columns: Job ID, Filename, Status, Uploaded At, Actions (`[View Details]`).
*   **Quarantined Jobs Section:**
    *   **Quarantine Table:** `<table id='quarantine-jobs-table'>`.
        *   Columns: Job ID, Original Filename, Quarantined Date, Error Summary, Actions (`[Details]`, `[Retry]`, `[Discard]`).
    *   **Quarantine Job Details Area:** `<div id='quarantine-job-details'>`.
        *   Initially hidden. Shows up when a "Details" button in the quarantine table is clicked.
        *   Contains a heading `<h4>Quarantine Details</h4>` and a `<pre id='quarantine-details-content'></pre>` tag to display formatted JSON (or other text) of the job's metadata/error logs.

**Interactions:**

*   **Upload New Data & Ingestion Jobs Table:** (As previously described)
    *   File selection and upload via `/upload/data`.
    *   Successful uploads add a row to the Ingestion Jobs table.
    *   "View Details" button in Ingestion Jobs table fetches and displays job status/metadata (e.g., in an alert or modal - details not fully specified for this button in current implementation).
*   **Quarantined Jobs Table:**
    *   Populated on page load with dummy data (for now) or could be fetched from a backend endpoint in the future (e.g., `/metadata/ingestion_jobs?status=quarantined`).
    *   **Clicking "Details" (`viewQuarantineDetails(jobId)`):**
        *   Fetches the full metadata for the specified `jobId` from `/metadata/ingestion_jobs/{jobId}`.
        *   Displays the fetched JSON data (pretty-printed) in the `#quarantine-details-content` preformatted text area.
        *   Makes the `#quarantine-job-details` div visible.
    *   **Clicking "Retry" (`retryQuarantineJob(jobId)`):**
        *   Currently shows an alert: `Retry job {jobId} - not implemented yet.`.
        *   Future: Would call a backend endpoint to re-queue the task. The UI would then ideally update (e.g., remove from quarantine or show a "retrying" status).
    *   **Clicking "Discard" (`discardQuarantineJob(jobId)`):**
        *   Shows a confirmation dialog.
        *   If confirmed, currently shows an alert: `Discard job {jobId} - not implemented yet.`.
        *   Future: Would call a backend endpoint to mark the job as discarded and potentially clean up associated files. The UI would remove the row from the table. For the current stub, it removes the row from the UI.

---

## 7. Enhanced Index Creation Wizard (Post-AEGIS Pipeline)

*   **Purpose:** To guide users through the process of creating a new spatial index from data that has been successfully processed and validated by the AEGIS-ECHO Data Ingestion Pipeline. This wizard leverages the metadata and analysis results from the pipeline to provide a more informed and streamlined index creation experience.
*   **Initiation:**
    *   From the "Ingestion Dashboard": A user might select a job with status `validation_successful_ready_for_promotion` from the "Ingestion Jobs" table and click an action like "Create Index with this Data" (this button would be added to the Ingestion Jobs table rows in a future iteration).
    *   From "Index Management" view: Clicking the "Create New Index from Processed Data" button (`#show-create-index-wizard-button`).
*   **Layout:** A multi-step modal dialog (`#create-index-wizard-modal`) overlaying the current view. Navigation buttons (`[Next Step]`, `[Previous Step]`, `[Cancel Wizard]`, `[Create Index]`) guide the user.

### Step 1: Select Processed Data (`#wizard-step-1`)

*   **Context:** If the wizard is launched from the general "Create New Index..." button, this step is crucial. If launched from a specific "promoted" job in the Ingestion Dashboard, this step could be pre-filled or even skipped, directly taking the user to Step 2 with the data context already loaded.
*   **UI Elements:**
    *   **Title:** "Step 1: Select Processed Data"
    *   **Processed Datasets Selector (`#processed-data-select`):**
        *   A dropdown/select list.
        *   Populated by JavaScript fetching data from `/metadata/ingestion_jobs?processing_status=validation_successful_ready_for_promotion` (backend endpoint). Each option displays: `Original Filename (Job ID: {job.job_id}, Promoted: {job.promotion_timestamp})`. The `value` of each option would be the `job.job_id`.
    *   **Selected Dataset Info (`#selected-data-info`):**
        *   Read-only area updated when a dataset is chosen. Displays: "Selected File:", "Job ID:", "Processed Path:" (from the selected option's dataset, using `job.location_of_promoted_data`).
    *   **Navigation:** `[Button: Next: Review Validation]` (onclick `wizardNavigate(2)`), `[Button: Cancel Wizard]` (onclick `closeCreateIndexWizard()`).

```
+------------------------------------------------------------------------------+
| Create New Index - Step 1: Select Processed Data                             |
+------------------------------------------------------------------------------+
|   <label for="processed-data-select">Available Processed Datasets:</label>    |
|   <select id="processed-data-select">                                        |
|     <option value="">-- Select a dataset --</option>                        |
|     <option value="job_p123_id" data-fulljob='{...metadata...}'>file1.shp (Job: job_p123_id)</option> |
|   </select>                                                                  |
|   <div id="selected-data-info">                                              |
|     <p><strong>Selected File:</strong> <span id="wizard-selected-filename">-</span></p>         |
|     <p><strong>Job ID:</strong> <span id="wizard-selected-jobid">-</span></p>                     |
|     <p><strong>Processed Path:</strong> <span id="wizard-selected-filepath">-</span></p>       |
|   </div>                                                                     |
|                                         [Next: Review Validation >] [Cancel] |
+------------------------------------------------------------------------------+
```

### Step 2: Validation Review & Configuration (`#wizard-step-2`)

*   **Purpose:** Display AEGIS pipeline validation results for the selected dataset (`IngestionJobMetadata`) and allow basic configuration.
*   **UI Elements:**
    *   **Title:** "Step 2: Validation Review & Configuration"
    *   **Dataset Identifier:** (Read-only) `Selected Processed File: <span id="wizard-dataset-path-step2"></span>`
    *   **AEGIS Pipeline Analysis Summary (`#data-validation-summary`):**
        *   Populated from the `IngestionJobMetadata` of the selected dataset (retrieved in Step 1).
        *   Displays: Original Format, Standardized Output, Original CRS, Standardized CRS, Geometric Validation summary (from `validation_summary.geometric_validation`), Schema Validation (placeholder), Security Scan (placeholder).
    *   **Attribute Selection (`#wizard-attributes-select`):**
        *   Multi-select list box for attributes (placeholder for now, as actual attributes are not yet fetched/known by frontend from the GeoJSON).
    *   **Acknowledgement Checkbox (`#wizard-ack-validation`):**
        *   `[ ] I have reviewed the validation summary and confirm data suitability.` (Enables "Next" button).
    *   **Navigation:** `[< Previous: Select Data]`, `[Button: Next: Index Parameters]` (id=`wizard-step2-next`, initially disabled), `[Cancel Wizard]`.

```
+------------------------------------------------------------------------------+
| Create New Index - Step 2: Validation Review & Config                        |
+------------------------------------------------------------------------------+
|   **Dataset:** <span id="wizard-dataset-path-step2"></span>                  |
|   **AEGIS Pipeline Validation Summary:**                                     |
|   - Original Format: <span id="wizard-val-orig-format"></span> -> Standardized: GeoJSON |
|   - Original CRS: <span id="wizard-val-orig-crs"></span> -> Target: <span id="wizard-val-target-crs"></span> |
|   - Geometric Validation: <span id="wizard-val-geom"></span>                   |
|   Attributes to Index: <select id="wizard-attributes-select" multiple>...</select> |
|   [X] <label for="wizard-ack-validation">I acknowledge validation.</label>    |
|                                   [< Prev] [Next (disabled) >] [Cancel]      |
+------------------------------------------------------------------------------+
```

### Step 3: Index Parameters (`#wizard-step-3`)

*   **Purpose:** Define SPINDEX-specific parameters for the new index.
*   **UI Elements:**
    *   **Title:** "Step 3: Index Parameters"
    *   **Index Name Input (`#index-name-input`):** Text input.
    *   **Index Description Input (`#index-description-input`):** Textarea.
    *   **Index Type Selector (`#index-type-select`):** Dropdown (R-Tree, Quadtree, etc.).
    *   **Dynamic Parameters Area (`#index-specific-params`):** For type-specific params (e.g., Max Entries for R-Tree via `#rtree-max-entries`).
    *   **Data Source:** (Read-only, shows path of selected processed file).
    *   **Navigation:** `[< Previous: Validation]`, `[Next: Review & Create >]`, `[Cancel Wizard]`.

```
+------------------------------------------------------------------------------+
| Create New Index - Step 3: Index Parameters                                  |
+------------------------------------------------------------------------------+
|   <label>Index Name:</label> <input id="index-name-input" type="text" />     |
|   <label>Description:</label> <textarea id="index-description-input"></textarea>|
|   <label>Index Type:</label> <select id="index-type-select"><option>R-Tree</option>...</select> |
|   <div id="index-specific-params">Max Entries (RTree): <input id="rtree-max-entries" value="16"/></div> |
|   Data Source: /processed_area/job_p123_processed.geojson (Read-only)        |
|                                   [< Prev] [Next: Review >] [Cancel]         |
+------------------------------------------------------------------------------+
```

### Step 4: Review & Create (`#wizard-step-4`)

*   **Purpose:** Final review before submitting the index creation request to `POST /indexes`.
*   **UI Elements:**
    *   **Title:** "Step 4: Review & Create"
    *   **Summary Display (`#wizard-summary`):** Read-only text summarizing: Processed Data Path, Index Name, Type, Parameters, Selected Attributes.
    *   **Navigation:** `[< Previous: Parameters]`, `[Button: Create Index]` (id=`create-index-button`, onclick `submitCreateIndex()`), `[Cancel Wizard]`.

```
+------------------------------------------------------------------------------+
| Create New Index - Step 4: Review & Create                                   |
+------------------------------------------------------------------------------+
|   **Review Your Selections:**                                                |
|   - Processed Data: <span id="summary-data-path"></span>                     |
|   - Index Name: <span id="summary-index-name"></span>                         |
|   - Index Type: <span id="summary-index-type"></span>                         |
|   - Parameters: <span id="summary-index-params"></span>                       |
|   - Attributes: <span id="summary-index-attributes"></span>                   |
|                                                                              |
|                                   [< Prev] [Create Index] [Cancel]           |
+------------------------------------------------------------------------------+
```

**Interactions:**
*   Wizard navigation is handled by `wizardNavigate(stepNum)`.
*   Step 1: `populateProcessedDataSelect` fetches from `/metadata/ingestion_jobs`. Selection updates info display and stores chosen `IngestionJobMetadata`.
*   Step 2: `displayValidationSummaryForWizard` uses stored metadata. "Next" enabled by `#wizard-ack-validation`.
*   Step 3: User inputs index details.
*   Step 4: `populateWizardSummary` shows all choices. "Create Index" calls `submitCreateIndex`, which POSTs an `IndexCreationRequest` to `/indexes`. Success closes wizard & refreshes index table.

---
```
