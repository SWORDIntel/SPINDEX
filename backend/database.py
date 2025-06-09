from sqlalchemy import create_engine
from sqlalchemy.ext.declarative import declarative_base
from sqlalchemy.orm import sessionmaker
import os

# Define the SQLite database URL.
# SQLALCHEMY_DATABASE_URL = "sqlite:///./spindex_aegis.db"
# For Docker, it's better to use a path that's clearly inside the app directory if using relative paths
# Or use an environment variable for production flexibility.
DATABASE_FILE_PATH = os.path.join(os.path.dirname(os.path.abspath(__file__)), "spindex_aegis.db")
SQLALCHEMY_DATABASE_URL = f"sqlite:///{DATABASE_FILE_PATH}"


# Create the SQLAlchemy engine.
# `connect_args` is needed only for SQLite to disable `check_same_thread`
# because FastAPI, by default, can interact with the database in multiple threads for a single request (e.g. in dependencies).
engine = create_engine(
    SQLALCHEMY_DATABASE_URL, connect_args={"check_same_thread": False}
)

# Create a SessionLocal class. Each instance of SessionLocal will be a database session.
# The class itself is not a database session yet.
SessionLocal = sessionmaker(autocommit=False, autoflush=False, bind=engine)

# Create a Base class. Our ORM models will inherit from this class.
Base = declarative_base()

# Dependency to get a DB session for API endpoints
def get_db():
    db = SessionLocal()
    try:
        yield db
    finally:
        db.close()

# Function to create all tables (useful for initial setup without Alembic, or for tests)
# However, with Alembic, `alembic upgrade head` is the preferred way.
def create_db_and_tables():
    Base.metadata.create_all(bind=engine)

if __name__ == "__main__":
    # This could be run to initialize the DB schema directly if not using Alembic
    # For example, in a very simple setup or for initial testing before Alembic is configured.
    print(f"Creating database and tables at {SQLALCHEMY_DATABASE_URL} (if they don't exist)...")
    # Base.metadata.create_all(bind=engine) # Typically handled by Alembic
    print("Database and tables setup process finished (handled by Alembic usually).")
    # To actually create, you'd uncomment Base.metadata.create_all(bind=engine)
    # and ensure your models are imported here or Base is populated elsewhere.
    # For this project, we will rely on Alembic.
