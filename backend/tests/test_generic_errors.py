import pytest
from fastapi.testclient import TestClient
import uuid # To check for UUID like structure for error_id

from backend.main import app

client = TestClient(app)

def is_valid_uuid(uuid_to_test, version=4):
    """ Check if uuid_to_test is a valid UUID. """
    try:
        uuid_obj = uuid.UUID(uuid_to_test, version=version)
    except ValueError:
        return False
    return str(uuid_obj) == uuid_to_test

def test_generic_unhandled_exception_handler(client: TestClient):
    # This endpoint is designed to raise a ZeroDivisionError
    response = client.get("/test_generic_error")

    assert response.status_code == 500
    data = response.json()
    assert data["detail"] == "An unexpected internal server error occurred. Please contact support."
    assert "error_id" in data
    assert is_valid_uuid(data["error_id"]) # Check if it's a valid UUID string
