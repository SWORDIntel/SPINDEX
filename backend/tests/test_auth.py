import pytest
from fastapi.testclient import TestClient
import os

from backend.main import app # Assuming app is your FastAPI instance

client = TestClient(app)

# Use an endpoint that requires authentication, e.g., GET /indexes
PROTECTED_ENDPOINT_URL = "/indexes" # Or any other protected endpoint

def test_missing_auth_token(client: TestClient):
    response = client.get(PROTECTED_ENDPOINT_URL) # No "X-Auth-Token" header
    assert response.status_code == 401
    data = response.json()
    assert data["detail"] == "Not authenticated: X-Auth-Token header is missing."

def test_invalid_auth_token(client: TestClient):
    response = client.get(PROTECTED_ENDPOINT_URL, headers={"X-Auth-Token": "invalid-dummy-token"})
    assert response.status_code == 401
    data = response.json()
    assert data["detail"] == "Invalid authentication credentials."

def test_valid_auth_token_viewer(client: TestClient):
    # Assuming TOKEN_VIEWER is set up in your main.py or conftest.py for a valid viewer token
    viewer_token = os.getenv("AEGIS_TOKEN_VIEWER", "secure-dev-token-viewer")
    response = client.get(PROTECTED_ENDPOINT_URL, headers={"X-Auth-Token": viewer_token})
    # This just checks that a valid token for a role passes the auth check.
    # The actual response (e.g., 200 OK or 403 if viewer can't access /indexes) depends on endpoint permissions.
    # For /indexes, a viewer should get a 200.
    assert response.status_code == 200
    # If testing permissions, you'd assert based on the role's allowed actions.
    # For example, if viewers can't POST to /indexes:
    # viewer_response_post = client.post("/indexes", json={}, headers={"X-Auth-Token": viewer_token})
    # assert viewer_response_post.status_code == 403
    # This specific test is more about the auth dependency itself.
