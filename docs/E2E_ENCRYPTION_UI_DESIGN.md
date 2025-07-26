# E2E Encryption UI Design

## 1. Overview

This document outlines the design for the End-to-End (E2E) Encryption UI. The E2E Encryption UI will allow administrators to manage encryption keys and to re-key data.

## 2. Location

The E2E Encryption UI will be located in the "System" tab of the "Settings" view. This tab will be visible only to users with the "admin" role.

## 3. Encryption Keys

The UI will display a list of encryption keys in a table. The table will have the following columns:

*   **Name**: The name of the key.
*   **Description**: A description of the key.
*   **Creation Date**: The date the key was created.
*   **Active**: A checkmark indicating whether the key is active.
*   **Actions**: A set of buttons to activate, delete, or re-key the data with the key.

The UI will also provide a form to create a new encryption key. The form will have the following fields:

*   **Name**: The name of the key.
*   **Description**: A description of the key.

## 4. Key Deletion

The UI will allow the user to delete an encryption key. A key cannot be deleted if it is active.

## 5. Key Activation

The UI will allow the user to activate an encryption key. Only one key can be active at a time. The active key will be used to encrypt all new data.

## 6. Data Rekeying

The UI will provide a button to re-key all existing data with the active key. This will be a long-running operation that will be performed in the background. The UI will display the progress of the re-keying operation.
