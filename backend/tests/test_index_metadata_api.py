import pytest
from fastapi.testclient import TestClient
from sqlalchemy.orm import Session as SQLAlchemySession # For type hinting if needed for db fixture
from typing import List, Dict, Any

from backend.main import app # For direct Pydantic model import if needed, though client uses app
from backend.models import IndexMetadataDB # For direct DB checks
# Pydantic model for IndexMetadata is in main.py, will be used for response validation
from backend.main import IndexMetadata as IndexMetadataPydantic
from backend.main import TOKEN_ADMIN # Using admin token for these tests (environment default or set)

# Helper to get auth headers
def admin_auth_headers() -> Dict[str, str]:
    return {"X-Auth-Token": TOKEN_ADMIN}

# Test data
sample_index_payload_1 = {
    "index_name": "test_index_1",
    "index_type": "elasticsearch",
    "promoted_data_path": "/path/to/promoted_data_1.geojson",
    # version, status, created_at, index_id are often auto-generated or defaulted
}

sample_index_payload_2 = {
    "index_name": "test_index_2",
    "index_type": "qdrant",
    "promoted_data_path": "/path/to/promoted_data_2.gpkg",
    "status": "active" # Example of overriding default
}

def test_create_index_metadata_success(client: TestClient, db_session_override: SQLAlchemySession):
    response = client.post("/indexes", json=sample_index_payload_1, headers=admin_auth_headers())
    assert response.status_code == 201
    data = response.json()
    assert data["index_name"] == sample_index_payload_1["index_name"]
    assert data["index_type"] == sample_index_payload_1["index_type"]
    assert data["promoted_data_path"] == sample_index_payload_1["promoted_data_path"]
    assert "index_id" in data
    assert data["status"] == "creating" # Default status
    assert data["version"] == 1 # Default version

    # Verify in DB
    db_item = db_session_override.query(IndexMetadataDB).filter(IndexMetadataDB.index_id == data["index_id"]).first()
    assert db_item is not None
    assert db_item.index_name == sample_index_payload_1["index_name"]

def test_create_index_metadata_missing_fields(client: TestClient):
    invalid_payload = {"index_name": "incomplete_index"} # Missing index_type, promoted_data_path
    response = client.post("/indexes", json=invalid_payload, headers=admin_auth_headers())
    assert response.status_code == 422 # Unprocessable Entity for Pydantic validation error

def test_get_all_index_metadata_empty(client: TestClient):
    response = client.get("/indexes", headers=admin_auth_headers())
    assert response.status_code == 200
    assert response.json() == []

def test_get_all_index_metadata_with_items(client: TestClient, db_session_override: SQLAlchemySession):
    # Create some items first
    response1 = client.post("/indexes", json=sample_index_payload_1, headers=admin_auth_headers())
    assert response1.status_code == 201
    item1_id = response1.json()["index_id"]

    response2 = client.post("/indexes", json=sample_index_payload_2, headers=admin_auth_headers())
    assert response2.status_code == 201
    item2_id = response2.json()["index_id"]

    response_get = client.get("/indexes", headers=admin_auth_headers())
    assert response_get.status_code == 200
    data = response_get.json()
    assert len(data) == 2

    ids_in_response = {item["index_id"] for item in data}
    assert item1_id in ids_in_response
    assert item2_id in ids_in_response

def test_get_index_metadata_by_id_success(client: TestClient):
    # Create an item
    response_post = client.post("/indexes", json=sample_index_payload_1, headers=admin_auth_headers())
    assert response_post.status_code == 201
    created_item = response_post.json()
    item_id = created_item["index_id"]

    # Get it by ID
    response_get = client.get(f"/indexes/{item_id}", headers=admin_auth_headers())
    assert response_get.status_code == 200
    retrieved_item = response_get.json()
    assert retrieved_item["index_id"] == item_id
    assert retrieved_item["index_name"] == sample_index_payload_1["index_name"]

def test_get_index_metadata_by_id_not_found(client: TestClient):
    non_existent_id = "non_existent_uuid_123"
    response = client.get(f"/indexes/{non_existent_id}", headers=admin_auth_headers())
    assert response.status_code == 404
    data = response.json()
    assert data["detail"] == f"IndexMetadata with ID '{non_existent_id}' not found."
    assert data["error_code"] == "RESOURCE_NOT_FOUND"

def test_create_index_metadata_duplicate_name(client: TestClient, db_session_override: SQLAlchemySession):
    # Create the first item successfully
    client.post("/indexes", json=sample_index_payload_1, headers=admin_auth_headers())

    # Attempt to create another with the same name
    duplicate_payload = {
        "index_name": sample_index_payload_1["index_name"], # Same name
        "index_type": "elasticsearch", # Can be same or different type
        "promoted_data_path": "/path/to/other_promoted_data.geojson" # Different data path
    }
    response = client.post("/indexes", json=duplicate_payload, headers=admin_auth_headers())
    assert response.status_code == 409 # Conflict
    data = response.json()
    assert data["detail"] == f"IndexMetadata with index_name '{sample_index_payload_1['index_name']}' already exists."
    assert data["error_code"] == "DUPLICATE_RESOURCE"


def test_update_index_metadata_success(client: TestClient):
    # Create an item
    response_post = client.post("/indexes", json=sample_index_payload_1, headers=admin_auth_headers())
    assert response_post.status_code == 201
    item_id = response_post.json()["index_id"]

    update_payload = {"status": "active", "index_name": "updated_index_name"}
    response_put = client.put(f"/indexes/{item_id}", json=update_payload, headers=admin_auth_headers())
    assert response_put.status_code == 200
    updated_item = response_put.json()
    assert updated_item["index_id"] == item_id
    assert updated_item["status"] == "active"
    assert updated_item["index_name"] == "updated_index_name"
    # Check that original fields not in payload are preserved
    assert updated_item["index_type"] == sample_index_payload_1["index_type"]

def test_update_index_metadata_not_found(client: TestClient):
    non_existent_id = "non_existent_uuid_for_update"
    update_payload = {"status": "active"}
    response = client.put(f"/indexes/{non_existent_id}", json=update_payload, headers=admin_auth_headers())
    assert response.status_code == 404
    data = response.json()
    assert data["detail"] == f"IndexMetadata with ID '{non_existent_id}' not found." # Or specific "not found for update"
    assert data["error_code"] == "RESOURCE_NOT_FOUND"


def test_delete_index_metadata_success(client: TestClient, db_session_override: SQLAlchemySession):
    # Create an item
    response_post = client.post("/indexes", json=sample_index_payload_1, headers=admin_auth_headers())
    assert response_post.status_code == 201
    item_id = response_post.json()["index_id"]

    # Delete it
    response_delete = client.delete(f"/indexes/{item_id}", headers=admin_auth_headers())
    assert response_delete.status_code == 204 # No content

    # Verify it's gone from DB
    db_item = db_session_override.query(IndexMetadataDB).filter(IndexMetadataDB.index_id == item_id).first()
    assert db_item is None

    # Try to get it again (should be 404)
    response_get = client.get(f"/indexes/{item_id}", headers=admin_auth_headers())
    assert response_get.status_code == 404
    data = response_get.json()
    assert data["detail"] == f"IndexMetadata with ID '{item_id}' not found."
    assert data["error_code"] == "RESOURCE_NOT_FOUND"


def test_delete_index_metadata_not_found(client: TestClient):
    non_existent_id = "non_existent_uuid_for_delete"
    response = client.delete(f"/indexes/{non_existent_id}", headers=admin_auth_headers())
    assert response.status_code == 404
    data = response.json()
    assert data["detail"] == f"IndexMetadata with ID '{non_existent_id}' not found."
    assert data["error_code"] == "RESOURCE_NOT_FOUND"

# TODO: Add tests for pagination in GET /indexes if implemented in endpoint
# TODO: Add tests for other auth scenarios (e.g., no token, invalid token, insufficient permissions if RBAC is granular)
