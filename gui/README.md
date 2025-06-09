# SPINDEX GUI Frontend

This directory contains the frontend files for the SPINDEX Graphical User Interface (GUI).

## Files

*   **`index.html`**: The main HTML file that defines the structure of the GUI. It includes the basic layout (header, sidebar, main content area, footer) and placeholders for different views that are dynamically shown/hidden by JavaScript.
*   **`style.css`**: The CSS file containing styles for the layout, components, and overall look and feel of the GUI.
*   **`script.js`**: The JavaScript file responsible for client-side interactivity. This includes:
    *   Handling navigation between different views.
    *   Managing UI interactions (e.g., button clicks, form submissions).
    *   Fetching data from and sending data to the backend API.
    *   Dynamically updating the content of the views.
*   **`GUI_MOCKUPS.md`**: A markdown document containing textual mockups and descriptions of the different views and components within the GUI. This serves as a design reference.

## Purpose

The GUI aims to provide a user-friendly interface for interacting with the SPINDEX system, allowing users to:
*   Manage spatial indexes.
*   Perform spatial queries.
*   Visualize data and query results.
*   Upload and monitor the ingestion of new data via the AEGIS-ECHO pipeline.
*   Create spatial indexes from data successfully processed by the AEGIS-ECHO pipeline using a guided wizard.
*   Configure system settings (role-dependent).

These files work together to create an interactive web-based experience for SPINDEX users.
The GUI communicates with the backend API (located in the `/backend` directory) to perform operations and retrieve data.

## Key GUI Components / Views

While `GUI_MOCKUPS.md` provides detailed textual mockups of all views, key components include:

*   **Dashboard:** Overview and quick access.
*   **Index Management View:** Allows users to view a table of existing spatial indexes (fetched from `/indexes`), see their status, creation date, type, etc. Users can view detailed metadata for each index (from `/indexes/{index_id}`) and delete indexes (via `DELETE /indexes/{index_id}`). This view also contains the entry point (button) to launch the "Enhanced Index Creation Wizard".
*   **Query Interface View:** Provides a form for users to:
    *   Select a 'ready' spatial index (populated from `GET /indexes`).
    *   Choose a query type (e.g., Bounding Box, Point with Radius).
    *   Dynamically input query parameters based on the selected type.
    *   Execute queries via `POST /query/execute`.
    *   View dummy query results (count, time, and JSON features).
    *   View a history of their queries (populated from `GET /query/history`), with a basic option to re-populate the form from a history item.
*   **Data Visualization:** View data on maps.
*   **Ingestion Dashboard:** Upload data for processing by AEGIS-ECHO (via `/upload/data`), monitor ingestion job statuses (via `/metadata/ingestion_jobs/{job_id}`), and manage quarantined items.
*   **Enhanced Index Creation Wizard:** A multi-step modal dialog for creating new spatial indexes from data previously processed and validated by the AEGIS-ECHO pipeline.
    *   **Step 1 (Select Processed Data):** User selects from a list of successfully processed datasets (fetched from `/metadata/ingestion_jobs?processing_status=validation_successful_ready_for_promotion`).
    *   **Step 2 (Validation Review & Configuration):** Displays validation summaries from the AEGIS pipeline (from the selected job's metadata) and allows configuration of attributes for indexing.
    *   **Step 3 (Index Parameters):** User specifies the index name, type (R-Tree, Quadtree, etc.), and other index-specific parameters.
    *   **Step 4 (Review & Create):** User reviews all selections and submits the request to `POST /indexes` to initiate asynchronous index creation via a Celery task on the backend.
    *   Detailed textual mockups for this wizard are available in `GUI_MOCKUPS.md`.
*   **Settings:** User and system configuration.

### General UI Enhancements

Across various views, the GUI incorporates several enhancements for better user experience:

*   **Loading Indicators:**
    *   **Spinners:** Used during short-duration data fetching operations (e.g., when fetching details for a specific index or query result).
    *   **Skeleton Placeholders:** Used for table views or lists when initially loading data (e.g., Ingestion Jobs table, Index Management table, Query History list) to provide a visual cue of the expected content structure.
*   **Help Tooltips:** Small `(?)` icons are placed next to certain form fields or table headers (e.g., Index Type, Status, Query Type) providing contextual help or explanations when hovered over or clicked. These are implemented using the `title` attribute on `<span>` elements with a specific CSS class.
*   **Progressive Disclosure:** In the Index Management view, index names in the table are clickable. Clicking an index name reveals (or hides if already visible) a dedicated area below the table (`#index-details-area`) displaying the full metadata for that index. This keeps the main table clean while providing easy access to detailed information.
