import pytest
from fastapi.testclient import TestClient
from sqlalchemy.orm import Session as SQLAlchemySession # For type hinting if needed
from typing import List, Dict, Any

from backend.models import QueryHistoryDB # For direct DB checks
# Pydantic models are in main.py
from backend.main import QueryHistoryItem as QueryHistoryItemPydantic
from backend.main import QueryRequest as QueryRequestPydantic
from backend.main import SECURE_PLACEHOLDER_TOKEN_ADMIN, SECURE_PLACEHOLDER_TOKEN_ANALYST # Using admin and another user for basic isolation check

# Helper to get auth headers
def user_auth_headers(token: str) -> Dict[str, str]:
    return {"X-Auth-Token": token}

# Sample QueryRequest data
sample_query_request_1: Dict[str, Any] = {
    "index_id": "some_index_uuid_1",
    "query_type": "match_all",
    "parameters": {}
}

sample_query_request_2: Dict[str, Any] = {
    "index_id": "some_index_uuid_2",
    "query_type": "keyword_search",
    "parameters": {"keyword": "test search", "fields": ["content", "title"]}
}

def test_create_query_history_item_success(client: TestClient, db_session_override: SQLAlchemySession):
    response = client.post(
        "/users/me/query_history",
        json=sample_query_request_1,
        headers=user_auth_headers(SECURE_PLACEHOLDER_TOKEN_ADMIN)
    )
    assert response.status_code == 201
    data = response.json()

    # Validate response structure against QueryHistoryItemPydantic
    assert "query_id" in data
    assert data["user_id"] == "admin" # Based on SECURE_PLACEHOLDER_TOKEN_ADMIN mapping
    assert data["request_data"]["index_id"] == sample_query_request_1["index_id"]
    assert data["request_data"]["query_type"] == sample_query_request_1["query_type"]
    assert "timestamp" in data

    # Verify in DB
    db_item = db_session_override.query(QueryHistoryDB).filter(QueryHistoryDB.query_id == data["query_id"]).first()
    assert db_item is not None
    assert db_item.user_id == "admin"
    assert db_item.request_data["index_id"] == sample_query_request_1["index_id"]

def test_create_query_history_item_invalid_payload(client: TestClient):
    invalid_query_request = {"index_id": "missing_type"} # query_type is required by QueryRequest Pydantic model
    response = client.post(
        "/users/me/query_history",
        json=invalid_query_request,
        headers=user_auth_headers(SECURE_PLACEHOLDER_TOKEN_ADMIN)
    )
    assert response.status_code == 422 # Unprocessable Entity

def test_get_my_query_history_empty(client: TestClient):
    response = client.get("/users/me/query_history", headers=user_auth_headers(SECURE_PLACEHOLDER_TOKEN_ADMIN))
    assert response.status_code == 200
    assert response.json() == []

def test_get_my_query_history_with_items(client: TestClient, db_session_override: SQLAlchemySession):
    # Create some history items for admin user
    client.post("/users/me/query_history", json=sample_query_request_1, headers=user_auth_headers(SECURE_PLACEHOLDER_TOKEN_ADMIN))
    client.post("/users/me/query_history", json=sample_query_request_2, headers=user_auth_headers(SECURE_PLACEHOLDER_TOKEN_ADMIN))

    response = client.get("/users/me/query_history", headers=user_auth_headers(SECURE_PLACEHOLDER_TOKEN_ADMIN))
    assert response.status_code == 200
    data = response.json()
    assert len(data) == 2
    # Items should be sorted by timestamp desc (newest first)
    assert data[0]["request_data"]["index_id"] == sample_query_request_2["index_id"]
    assert data[1]["request_data"]["index_id"] == sample_query_request_1["index_id"]
    for item in data:
        assert item["user_id"] == "admin"

def test_get_my_query_history_pagination(client: TestClient):
    # Create 3 items for admin
    client.post("/users/me/query_history", json=sample_query_request_1, headers=user_auth_headers(SECURE_PLACEHOLDER_TOKEN_ADMIN))
    client.post("/users/me/query_history", json=sample_query_request_2, headers=user_auth_headers(SECURE_PLACEHOLDER_TOKEN_ADMIN))
    # Create a third one to check ordering and limit
    third_query_request = {"index_id": "some_index_uuid_3", "query_type": "term", "parameters": {"field": "tag", "value": "new"}}
    client.post("/users/me/query_history", json=third_query_request, headers=user_auth_headers(SECURE_PLACEHOLDER_TOKEN_ADMIN))

    # Get 2 items, skipping 0
    response_limit_2 = client.get("/users/me/query_history?limit=2", headers=user_auth_headers(SECURE_PLACEHOLDER_TOKEN_ADMIN))
    assert response_limit_2.status_code == 200
    data_limit_2 = response_limit_2.json()
    assert len(data_limit_2) == 2
    assert data_limit_2[0]["request_data"]["index_id"] == third_query_request["index_id"] # Newest
    assert data_limit_2[1]["request_data"]["index_id"] == sample_query_request_2["index_id"]

    # Get 1 item, skipping 1
    response_skip_1_limit_1 = client.get("/users/me/query_history?skip=1&limit=1", headers=user_auth_headers(SECURE_PLACEHOLDER_TOKEN_ADMIN))
    assert response_skip_1_limit_1.status_code == 200
    data_skip_1_limit_1 = response_skip_1_limit_1.json()
    assert len(data_skip_1_limit_1) == 1
    assert data_skip_1_limit_1[0]["request_data"]["index_id"] == sample_query_request_2["index_id"] # Second newest

def test_get_my_query_history_user_isolation(client: TestClient, db_session_override: SQLAlchemySession):
    # Create item for admin user
    client.post("/users/me/query_history", json=sample_query_request_1, headers=user_auth_headers(SECURE_PLACEHOLDER_TOKEN_ADMIN))

    # Create item for analyst user
    client.post("/users/me/query_history", json=sample_query_request_2, headers=user_auth_headers(SECURE_PLACEHOLDER_TOKEN_ANALYST))

    # Check admin's history
    response_admin = client.get("/users/me/query_history", headers=user_auth_headers(SECURE_PLACEHOLDER_TOKEN_ADMIN))
    assert response_admin.status_code == 200
    data_admin = response_admin.json()
    assert len(data_admin) == 1
    assert data_admin[0]["user_id"] == "admin"
    assert data_admin[0]["request_data"]["index_id"] == sample_query_request_1["index_id"]

    # Check analyst's history
    response_analyst = client.get("/users/me/query_history", headers=user_auth_headers(SECURE_PLACEHOLDER_TOKEN_ANALYST))
    assert response_analyst.status_code == 200
    data_analyst = response_analyst.json()
    assert len(data_analyst) == 1
    assert data_analyst[0]["user_id"] == "analyst"
    assert data_analyst[0]["request_data"]["index_id"] == sample_query_request_2["index_id"]

def test_query_history_unauthenticated(client: TestClient):
    response_post = client.post("/users/me/query_history", json=sample_query_request_1) # No auth header
    assert response_post.status_code == 401 # Unauthorized

    response_get = client.get("/users/me/query_history") # No auth header
    assert response_get.status_code == 401 # Unauthorized
