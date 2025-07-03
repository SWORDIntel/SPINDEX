import pytest
from fastapi.testclient import TestClient
from unittest import mock
import ctypes
import json
import os

# Assuming your FastAPI app instance is named 'app' in 'backend.main'
# And C structures are also in backend.main for simplicity of import here
from backend.main import app, AvxSearchQueryRequest, AvxSearchQueryResponse, \
                         CUserQuery, CSearchResultItem, CSearchResults, \
                         CSearchOrchestrator # For typing if needed by mocks

client = TestClient(app)

# --- Mock C Structures and Data ---

def create_mock_c_search_result_item(doc_id: str, content: str, score: float, avx_found: bool, avx_details_dict: dict) -> CSearchResultItem:
    item = CSearchResultItem()
    item.document_id = doc_id.encode('utf-8')
    item.document_content_preview = content.encode('utf-8')
    item.score = ctypes.c_double(score)
    item.avx_match_found = ctypes.c_bool(avx_found)
    avx_details_str = json.dumps(avx_details_dict)
    item.avx_match_details = avx_details_str.encode('utf-8')
    return item

def create_mock_c_search_results(items_data: list) -> ctypes.POINTER(CSearchResults):
    if not items_data:
        # Return a pointer to a CSearchResults with count 0
        c_results_obj = CSearchResults()
        c_results_obj.items = None # Or POINTER(CSearchResultItem)() for empty array
        c_results_obj.count = 0
        c_results_obj.capacity = 0
        return ctypes.pointer(c_results_obj)

    num_items = len(items_data)
    # Create an array of CSearchResultItem
    item_array_type = CSearchResultItem * num_items
    c_item_array = item_array_type()

    for i, data in enumerate(items_data):
        c_item_array[i] = create_mock_c_search_result_item(**data)

    c_results_obj = CSearchResults()
    c_results_obj.items = c_item_array
    c_results_obj.count = num_items
    c_results_obj.capacity = num_items

    # To return a POINTER(CSearchResults), we need to allocate memory that persists
    # beyond this function's scope if the caller expects to use it.
    # For mocking, often the mock directly returns the POINTER type.
    # If the mock is spied on, this object needs to be kept alive or handled by the mock.
    # A simple way for a mock to return a pointer is to return ctypes.pointer(c_results_obj)
    # but the actual object c_results_obj must exist when dereferenced.
    # Let's assume the mock setup will handle the lifetime or return a direct POINTER.
    # For now, this function helps construct the object; the mock will return pointer(obj).
    # This might require careful handling of memory if tests get complex.
    # A common mocking pattern is `mock_c_function.return_value = ctypes.pointer(my_constructed_c_results_obj)`
    # where `my_constructed_c_results_obj` is stored at test scope.
    return ctypes.pointer(c_results_obj)


# --- Pytest Fixtures (Optional, can define mocks directly in tests) ---

@pytest.fixture
def mock_es_client(mocker):
    mock_search = mocker.patch("backend.main.es_client.search")
    return mock_search

@pytest.fixture
def mock_spindex_c_lib(mocker):
    # Mock the spindex_c_lib object itself if it's accessed directly
    mock_lib = mocker.patch("backend.main.spindex_c_lib")

    # Setup individual function mocks if needed, e.g., for search_orchestrator_execute_query
    mock_lib.search_orchestrator_execute_query = mock.MagicMock()
    mock_lib.search_results_destroy = mock.MagicMock()
    # Mock other C functions if they are called during the request lifecycle by the endpoint
    mock_lib.config_manager_load = mock.MagicMock(return_value=ctypes.pointer(CSearchOrchestrator())) # Dummy pointer
    mock_lib.load_plugins = mock.MagicMock(return_value=0)
    mock_lib.search_orchestrator_create = mock.MagicMock(return_value=ctypes.pointer(CSearchOrchestrator())) # Dummy pointer

    # Ensure spindex_c_lib is not None for tests that expect it to be loaded
    mocker.patch("backend.main.spindex_c_lib", new=mock_lib)
    return mock_lib

@pytest.fixture
def mock_c_orchestrator(mocker):
    # This fixture can be used if we need to ensure c_search_orchestrator is not None
    mock_obj = ctypes.pointer(CSearchOrchestrator()) # A dummy non-None pointer
    mocker.patch("backend.main.c_search_orchestrator", new=mock_obj)
    return mock_obj

# --- Test Cases ---

def test_search_with_avx_success_and_matches(mock_es_client, mock_spindex_c_lib, mock_c_orchestrator, mocker):
    # 1. Setup: Mock ES response
    mock_es_hits = [
        {"_id": "doc1", "_score": 0.9, "_source": {"text_content": "Elasticsearch hit one with AVX target phrase."}},
        {"_id": "doc2", "_score": 0.8, "_source": {"text_content": "Elasticsearch hit two, no AVX phrase here."}},
    ]
    mock_es_client.return_value = {"hits": {"hits": mock_es_hits}}

    # 1. Setup: Mock C library response (AVX found matches for doc1)
    # Keep this object alive for the duration of the test if pointer() is used
    mock_c_item_data = [
        {"doc_id": "doc1", "content": "Preview for doc1", "score": 0.95, "avx_found": True, "avx_details_dict": {"search_results": [{"phrase": "AVX target phrase", "offsets": [25]}]}},
        # doc2 from C's simulated ES might not be here, or AVX found nothing
    ]
    # Note: The C orchestrator's current simulation means its doc IDs might not directly map 1:1
    # to the Python ES hits unless the C simulation is perfectly aligned or ignored.
    # The Python code currently takes the C results if AVX is used.

    # Construct the CSearchResults object that will be pointed to
    # This object needs to stay in scope when its pointer is used by the mocked C function.
    # One way is to make it part of the test function's scope or a fixture.

    num_c_items = len(mock_c_item_data)
    c_item_array_type = CSearchResultItem * num_c_items
    global_c_item_array = c_item_array_type() # Make it global-like for lifetime or pass around
    for i, data in enumerate(mock_c_item_data):
        global_c_item_array[i] = create_mock_c_search_result_item(**data)

    global_c_results_obj = CSearchResults() # Make it global-like for lifetime
    global_c_results_obj.items = global_c_item_array
    global_c_results_obj.count = num_c_items
    global_c_results_obj.capacity = num_c_items

    mock_spindex_c_lib.search_orchestrator_execute_query.return_value = ctypes.pointer(global_c_results_obj)

    # Patch FEATURE_FLAG_AVX_SEARCH_ENABLED to True for this test
    mocker.patch("backend.main.FEATURE_FLAG_AVX_SEARCH_ENABLED", True)

    # 2. Action: Call the endpoint
    request_payload = {
        "query_text": "search query",
        "avx_phrases": "AVX target phrase",
        "use_avx": True,
        "index_name": "test_index"
    }
    # Assuming a dummy token works with Depends(get_current_user)
    # Replace "dummy_valid_token" with an actual token string that your get_current_user mock or real logic expects
    # For this example, let's assume TOKEN_ADMIN is "secure-dev-token-admin" as per main.py
    admin_token = os.getenv("AEGIS_TOKEN_ADMIN", "secure-dev-token-admin")
    response = client.post("/search_with_avx", json=request_payload, headers={"X-Auth-Token": admin_token})

    # 3. Assertions
    assert response.status_code == 200
    response_data = response.json()

    mock_es_client.assert_called_once()
    # Check args of es_client.search if needed, e.g., index name
    assert mock_es_client.call_args[1]['index'] == "test_index"

    mock_spindex_c_lib.search_orchestrator_execute_query.assert_called_once()
    # Check args of C call (CUserQuery fields)
    c_query_arg = mock_spindex_c_lib.search_orchestrator_execute_query.call_args[0][1].contents # get CUserQuery struct
    assert c_query_arg.main_query_text.decode('utf-8') == "search query"
    assert c_query_arg.avx_phrases.decode('utf-8') == "AVX target phrase"
    # Add checks for new AVX params if CUserQuery was updated for them in main.py
    # assert c_query_arg.avx_case_sensitive == request_payload.get("avx_case_sensitive", False) # Default if not in payload

    mock_spindex_c_lib.search_results_destroy.assert_called_once()

    assert response_data["total_results_after_avx"] == 1
    assert len(response_data["results"]) == 1
    result_item = response_data["results"][0]
    assert result_item["document_id"] == "doc1"
    assert result_item["avx_match_found"] is True
    assert result_item["avx_match_details"] == {"search_results": [{"phrase": "AVX target phrase", "offsets": [25]}]}


def test_search_avx_disabled_via_flag(mock_es_client, mock_spindex_c_lib, mock_c_orchestrator, mocker):
    # 1. Setup: Mock ES response
    mock_es_hits = [
        {"_id": "doc1", "_score": 0.9, "_source": {"text_content": "Content for doc1"}},
        {"_id": "doc2", "_score": 0.8, "_source": {"text_content": "Content for doc2"}},
    ]
    mock_es_client.return_value = {"hits": {"hits": mock_es_hits}}

    # Patch FEATURE_FLAG_AVX_SEARCH_ENABLED to False
    mocker.patch("backend.main.FEATURE_FLAG_AVX_SEARCH_ENABLED", False)

    # 2. Action
    request_payload = {
        "query_text": "another query",
        "avx_phrases": "some phrase",
        "use_avx": True, # User wants AVX, but flag is off
        "index_name": "another_index"
    }
    admin_token = os.getenv("AEGIS_TOKEN_ADMIN", "secure-dev-token-admin")
    response = client.post("/search_with_avx", json=request_payload, headers={"X-Auth-Token": admin_token})

    # 3. Assertions
    assert response.status_code == 200
    response_data = response.json()

    mock_es_client.assert_called_once()
    mock_spindex_c_lib.search_orchestrator_execute_query.assert_not_called() # C lib should not be called

    assert response_data["total_results_after_avx"] == 2 # Should be same as ES results
    assert len(response_data["results"]) == 2
    for item in response_data["results"]:
        assert item["avx_match_found"] is False
        assert "AVX search not performed" in item["avx_match_details"]["info"]

# TODO: Add more test cases:
# - AVX enabled, but no AVX phrases provided
# - AVX enabled, C library not loaded (mock spindex_c_lib to None)
# - AVX enabled, C orchestrator not initialized (mock c_search_orchestrator to None)
# - AVX enabled, C call returns NULL (mock search_orchestrator_execute_query to return None)
# - AVX enabled, C call returns results but no AVX matches found in those results
# - ES client connection error
# - ES client returns no hits

# Note on C object lifetimes for mocks:
# When mocking a C function that returns a pointer to a structure that Python then reads,
# the Python `ctypes.Structure` object that the pointer refers to must remain alive
# while Python code is accessing its fields.
# In these tests, `global_c_results_obj` and `global_c_item_array` are implicitly kept alive
# within the scope of the test function because they are local to it. If they were created
# inside a helper that returned only the pointer, that would be an issue unless the helper
# also returned the actual objects or they were stored in a way that outlives the helper call.
# Using `mocker.patch.object` or complex mock setups might also require careful management
# of what the mocked C function `return_value` points to.

# A global variable or a class member for test setup could also hold these C objects
# to ensure their lifetime if many tests need similar mock C data.
# For example:
# test_data_c_results_obj = None
# test_data_c_item_array = None
# And then in a setup_method or fixture:
#   global test_data_c_results_obj, test_data_c_item_array
#   ... construct them ...
#   mock_spindex_c_lib.search_orchestrator_execute_query.return_value = ctypes.pointer(test_data_c_results_obj)
# This ensures they live for the test session or class duration.
# For simplicity in this example, local scope within the test function for the mock data is used.
# Python's garbage collector should keep them alive as long as `ctypes.pointer()` result is used by the mock
# AND the mock's return_value (the pointer) is held by the mock object which is alive.
# However, explicit management (e.g. test class members) is safer for complex C data.

# The current approach relies on the mock object (`mock_spindex_c_lib.search_orchestrator_execute_query`)
# holding a reference to the `ctypes.pointer(global_c_results_obj)`, and `global_c_results_obj` itself
# being in the scope of the test function. This should be fine for `unittest.mock`.

# Final check for the `create_mock_c_search_results` helper:
# It returns `ctypes.pointer(c_results_obj)`. `c_results_obj` is local to that function.
# This is problematic if the pointer is used after the helper returns.
# The mock setup should be:
#   actual_c_obj_instance = CSearchResults()
#   ... populate actual_c_obj_instance ...
#   mock_c_function.return_value = ctypes.pointer(actual_c_obj_instance)
# where `actual_c_obj_instance` lives in the test's scope.
# The current implementation of `test_search_with_avx_success_and_matches` does this correctly
# by creating `global_c_results_obj` in its own scope.
# The helper function `create_mock_c_search_results` is therefore not directly usable for setting `return_value`
# unless it returns the object too, or the object is stored at a higher scope.
# For now, the direct construction in the test is clearer.
# I will remove the `create_mock_c_search_results` helper as it's misleading in its current form.

# (Removing the helper function for clarity as direct construction in test is better for managing lifetime)

# Consider adding a test for when C library is None
def test_search_avx_c_lib_not_loaded(mock_es_client, mocker):
    # 1. Setup: Mock ES response
    mock_es_hits = [{"_id": "doc1", "_score": 0.9, "_source": {"text_content": "Content for doc1"}}]
    mock_es_client.return_value = {"hits": {"hits": mock_es_hits}}

    # Patch spindex_c_lib to be None
    mocker.patch("backend.main.spindex_c_lib", None)
    # Also ensure c_search_orchestrator is None as it wouldn't be initialized
    mocker.patch("backend.main.c_search_orchestrator", None)
    mocker.patch("backend.main.FEATURE_FLAG_AVX_SEARCH_ENABLED", True)


    # 2. Action
    request_payload = {
        "query_text": "query when c lib is missing",
        "avx_phrases": "avx phrase",
        "use_avx": True
    }
    admin_token = os.getenv("AEGIS_TOKEN_ADMIN", "secure-dev-token-admin")
    response = client.post("/search_with_avx", json=request_payload, headers={"X-Auth-Token": admin_token})

    # 3. Assertions
    assert response.status_code == 200
    response_data = response.json()
    mock_es_client.assert_called_once()
    assert response_data["total_results_after_avx"] == 1
    assert response_data["results"][0]["avx_match_found"] is False
    assert "C library/orchestrator not available" in response_data["results"][0]["avx_match_details"]["info"]

# (End of file)
