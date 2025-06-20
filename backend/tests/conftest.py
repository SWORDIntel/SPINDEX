import pytest
from fastapi.testclient import TestClient
from sqlalchemy import create_engine
from sqlalchemy.orm import sessionmaker, Session as SQLAlchemySession # Renamed to avoid conflict
from typing import Generator

from backend.main import app # The FastAPI app instance
from backend.database import Base, get_db # Base for metadata, get_db for overriding
from backend import models # To ensure all models are imported for Base.metadata

# --- Test Database Setup (In-memory SQLite) ---
SQLALCHEMY_DATABASE_URL = "sqlite:///:memory:" # In-memory SQLite database for tests
engine = create_engine(
    SQLALCHEMY_DATABASE_URL, connect_args={"check_same_thread": False} # check_same_thread needed for SQLite
)
TestingSessionLocal = sessionmaker(autocommit=False, autoflush=False, bind=engine)

# --- Fixture to Create Database Tables ---
@pytest.fixture(scope="session", autouse=True)
def create_tables_session_scoped():
    # Create all tables in the in-memory SQLite database once per test session
    # This ensures that all tables defined in models imported via `backend.models` are created.
    try:
        Base.metadata.create_all(bind=engine)
        yield
        # Teardown: Base.metadata.drop_all(bind=engine) # Optional: drop tables after session
    except Exception as e:
        print(f"Error during initial table creation: {e}")
        # Depending on test philosophy, you might want to raise this or handle it.
        # For now, let errors in table creation be visible.
        raise

# --- Fixture to Override get_db Dependency ---
# This fixture will be used by tests that need a database session.
# It ensures that each test gets a fresh, isolated database session.
@pytest.fixture(scope="function")
def db_session_override() -> Generator[SQLAlchemySession, None, None]:
    connection = engine.connect()
    transaction = connection.begin()
    db = TestingSessionLocal(bind=connection)

    yield db

    # Teardown for the session: rollback any changes to keep tests isolated
    db.close()
    transaction.rollback() # Ensure test isolation
    connection.close()

# --- Fixture to provide a TestClient ---
# This uses the db_session_override to ensure the app uses the test DB
@pytest.fixture(scope="function")
def client(db_session_override: SQLAlchemySession) -> Generator[TestClient, None, None]:
    # Override the get_db dependency for the FastAPI app
    def override_get_db() -> Generator[SQLAlchemySession, None, None]:
        yield db_session_override

    app.dependency_overrides[get_db] = override_get_db

    with TestClient(app) as test_client:
        yield test_client

    # Clean up the override after the test to prevent interference
    del app.dependency_overrides[get_db]

# --- (Optional) Fixture for mock authentication ---
# This can be expanded later if needed for specific tests.
# For now, endpoints might be tested with a known test token if auth is simple,
# or get_current_user might be overridden similarly if complex.
# Example:
# from backend.main import get_current_user, User, Role
# @pytest.fixture
# def mock_admin_user() -> User:
#     return User(username="testadmin", roles=[Role(name="Admin", permissions=["full_access"])])

# @pytest.fixture
# def client_as_admin_user(client: TestClient, mock_admin_user: User):
#     def override_get_current_user_admin():
#         return mock_admin_user
#     app.dependency_overrides[get_current_user] = override_get_current_user_admin
#     yield client # The same client, but now get_current_user is mocked for it
#     del app.dependency_overrides[get_current_user] # Clean up
