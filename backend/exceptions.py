# backend/exceptions.py

class SpindexBaseException(Exception):
    """Base exception for SPINDEX application."""
    status_code: int = 500
    detail: str = "An unexpected internal error occurred."
    error_code: Optional[str] = None # Optional internal error code

    def __init__(self, detail: Optional[str] = None, status_code: Optional[int] = None, error_code: Optional[str] = None):
        if detail is not None:
            self.detail = detail
        if status_code is not None:
            self.status_code = status_code
        if error_code is not None:
            self.error_code = error_code
        super().__init__(self.detail)


class ResourceNotFoundException(SpindexBaseException):
    """Raised when a resource is not found in the database."""
    def __init__(self, resource_name: str, resource_id: Any):
        super().__init__(
            detail=f"{resource_name} with ID '{resource_id}' not found.",
            status_code=404,
            error_code="RESOURCE_NOT_FOUND"
        )

class DatabaseErrorException(SpindexBaseException):
    """Raised for general database operation errors after logging the original exception."""
    def __init__(self, detail: str = "A database error occurred.", original_exception: Optional[Exception] = None):
        # In a real app, you'd log original_exception here or in the handler
        super().__init__(
            detail=detail,
            status_code=500,
            error_code="DATABASE_ERROR"
        )

class InvalidOperationException(SpindexBaseException):
    """Raised when an operation is invalid given the current state of the resource."""
    def __init__(self, detail: str):
        super().__init__(
            detail=detail,
            status_code=400, # Bad Request, or 409 Conflict depending on context
            error_code="INVALID_OPERATION"
        )

class DuplicateResourceException(SpindexBaseException):
    """Raised when attempting to create a resource that already exists (e.g. unique constraint)."""
    def __init__(self, resource_name: str, conflicting_field: str, conflicting_value: Any):
        super().__init__(
            detail=f"{resource_name} with {conflicting_field} '{conflicting_value}' already exists.",
            status_code=409, # Conflict
            error_code="DUPLICATE_RESOURCE"
        )

class ElasticsearchConnectionException(SpindexBaseException):
    """Raised when there's an issue connecting to Elasticsearch."""
    def __init__(self, detail: str = "Could not connect to Elasticsearch."):
        super().__init__(
            detail=detail,
            status_code=503, # Service Unavailable
            error_code="ELASTICSEARCH_CONNECTION_ERROR"
        )

class ElasticsearchQueryException(SpindexBaseException):
    """Raised for errors during Elasticsearch query execution."""
    def __init__(self, detail: str = "An error occurred while querying Elasticsearch."):
        super().__init__(
            detail=detail,
            status_code=500, # Or more specific if possible
            error_code="ELASTICSEARCH_QUERY_ERROR"
        )

class CComponentException(SpindexBaseException):
    """Raised for errors related to the Spindex C components."""
    def __init__(self, detail: str = "An error occurred within the Spindex C components."):
        super().__init__(
            detail=detail,
            status_code=500,
            error_code="C_COMPONENT_ERROR"
        )

# Add more specific exceptions as needed
