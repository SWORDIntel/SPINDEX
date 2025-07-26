# Index Versioning UI Design

## 1. Overview

This document outlines the design for the Index Versioning UI. The Index Versioning UI will allow users to view the version history of an index, compare different versions, and roll back to a previous version.

## 2. Location

The index versioning display will be located in the "Index Management" view. When the user clicks on the "Details" button for an index, a modal will open with the index details. This modal will also contain a "Version History" tab.

## 3. Version History

The "Version History" tab will display a table of all the versions of the index. The table will have the following columns:

*   **Version**: The version number.
*   **Creation Date**: The date the version was created.
*   **Data Source**: The path to the data that was used to create the version.
*   **Hash**: The cryptographic hash of the index structure.
*   **Actions**: A set of buttons to perform actions on the version.

The table will look like this:

| Version | Creation Date | Data Source | Hash | Actions |
|---|---|---|---|---|
| 2 | 2025-07-26 | /path/to/data2.geojson | 0xabc123 | [Details] [Compare] [Rollback] |
| 1 | 2025-07-25 | /path/to/data1.geojson | 0xdef456 | [Details] [Compare] |

## 4. Actions

The following actions will be available for each version:

*   **Details**: This button will open a new modal with the details of the version, including the parameters that were used to create it.
*   **Compare**: This button will allow the user to select another version to compare with. The comparison will be displayed in a new modal.
*   **Rollback**: This button will allow the user to roll back to the selected version. This will create a new version of the index with the same data and parameters as the selected version.
