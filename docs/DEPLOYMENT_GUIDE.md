# SPINDEX Setup and Deployment Guide

## 1. Introduction

This guide provides instructions for setting up a development environment for the SPINDEX project, building its components, and deploying the application stack using Docker and Docker Compose.

SPINDEX consists of a C-based worker/library for core processing and plugins, and a Python-based backend API (FastAPI). This guide covers building the C components and running the combined stack in a containerized environment.

## 2. Development Environment Setup

### 2.1. Prerequisites

Ensure you have the following software installed on your development machine:

*   **Git:** For cloning the repository.
*   **Python:** Version 3.8+ (for backend and Conan). Pip should be included.
*   **Conan Package Manager:** Version 2.x (e.g., `pip install "conan>=2.0,<2.2"`). Used for C/C++ dependency management.
    *   Initialize a default Conan profile if you haven't already: `conan profile detect --force`.
    *   **Important for OpenVINO:** If you intend to build with OpenVINO support, you may need to add Intel's Conan remote:
        ```sh
        conan remote add intel https://intel.github.io/openvino-conan/index.json
        ```
*   **CMake:** Version 3.15 or higher.
*   **A C Compiler:** GCC or Clang that supports C11. (Installed via `build-essential` on Debian/Ubuntu).
*   **Docker:** Latest stable version.
*   **Docker Compose:** Latest stable version (usually included with Docker Desktop or installed as a plugin).

### 2.2. Cloning the Repository

```sh
git clone <repository_url> # Replace <repository_url> with the actual URL
cd spindex-project-root # Navigate to the root directory of the project
```

### 2.3. Building C Components (Locally - Optional, also done in Docker)

The C components (core application `spindex_app` and plugins) are built using CMake and Conan. Refer to `SPINDEX/README_BUILD.md` for detailed local build instructions. A summary:

1.  **Navigate to the C worker directory:**
    ```sh
    cd SPINDEX
    ```
2.  **Install Conan dependencies:**
    ```sh
    # Ensure your conan profile matches your local dev environment
    conan install . --output-folder=build_cmake --build=missing -s:h build_type=Debug
    ```
3.  **Configure with CMake:**
    ```sh
    cmake -S . -B build_cmake -DCMAKE_TOOLCHAIN_FILE=build_cmake/generators/conan_toolchain.cmake -DCMAKE_BUILD_TYPE=Debug
    ```
4.  **Compile:**
    ```sh
    cmake --build build_cmake -j $(nproc)
    ```
    Compiled artifacts will be in `SPINDEX/build_cmake/bin` and `SPINDEX/build_cmake/lib/spindex/plugins`.

### 2.4. Python Backend Setup (Locally - Optional, also done in Docker)

The Python backend (FastAPI) is typically located in the `SPINDEX/backend/` directory.

1.  **Navigate to the backend directory:**
    ```sh
    cd SPINDEX/backend
    ```
2.  **Create and activate a Python virtual environment:**
    ```sh
    python -m venv .venv
    source .venv/bin/activate # On Linux/macOS
    # .venv\Scripts\activate # On Windows
    ```
3.  **Install Python dependencies:**
    ```sh
    pip install -r requirements.txt
    ```
4.  **Set up database (Alembic migrations):**
    Ensure PostgreSQL is running (either locally or via Docker from the `docker-compose.yml` described later).
    Update `sqlalchemy.url` in `alembic.ini` if your DB connection differs from the default.
    ```sh
    alembic upgrade head
    ```
5.  **Running the backend locally:**
    ```sh
    # Ensure necessary environment variables are set (DATABASE_URL, NATS_URL, ELASTICSEARCH_HOST, etc.)
    # These often point to services from docker-compose.yml if running hybrid.
    # Example:
    # export DATABASE_URL="postgresql://spindex_user:spindex_password@localhost:5432/spindex_db"
    # export ELASTICSEARCH_HOST="http://localhost:9200"
    # export NATS_URL="nats://localhost:4222"
    # export SPINDEX_C_LIB_PATH="../build_cmake/lib/libspindex_core_cffi.so" # Path to C library if used via CFFI
    # export SPINDEX_PLUGIN_DIR="../build_cmake/lib/spindex/plugins/"
    # export SPINDEX_CONFIG_FILE="../test_spindex_es.conf"

    uvicorn main:app --reload --host 0.0.0.0 --port 8000
    ```

## 3. Deployment using Docker

This section describes building the Docker image for the SPINDEX C worker and running the full application stack using `docker-compose.yml`. Ensure Docker Desktop or Docker Engine with Docker Compose is running.

### 3.1. Building the SPINDEX C Worker Docker Image

The `SPINDEX/Dockerfile` defines how to build an image for the C worker (`spindex_app` and its plugins). This image can be built as part of `docker-compose up` or manually.

To build it manually (e.g., for testing or pushing to a registry):
```sh
# Navigate to the project root (where docker-compose.yml is, one level above SPINDEX/)
docker build -t spindex-c-worker:latest -f SPINDEX/Dockerfile ./SPINDEX
```
Or, if `docker-compose.yml` is used, it will handle the build if the `spindex-c-worker` service specifies a `build` context.

### 3.2. Running the Application Stack with Docker Compose

The `docker-compose.yml` file (located in the project root) defines and orchestrates the necessary services:
*   NATS message broker
*   Elasticsearch
*   PostgreSQL database
*   Redis cache/broker
*   `spindex-c-worker` (the C application, if run as a separate service)
*   `spindex-backend` (the Python FastAPI application)

**To start the entire stack:**
```sh
# From the project root directory (where docker-compose.yml is located)
docker-compose up --build
```
*   `--build`: Forces Docker Compose to build images before starting containers (e.g., for `spindex-c-worker` and `spindex-backend` if they have `build` instructions).
*   Add `-d` to run in detached mode (background).

**To stop the stack:**
```sh
docker-compose down
```
To stop and remove volumes (deletes persisted data for ES, PG):
```sh
docker-compose down -v
```

### 3.3. Services Overview in `docker-compose.yml`

*   **`nats`**: NATS server for messaging. Accessible on `nats:4222` within the Docker network.
*   **`elasticsearch`**: Elasticsearch instance. Data is persisted in the `es_data` Docker volume. Accessible on `elasticsearch:9200`.
*   **`postgres`**: PostgreSQL database. Data persisted in `pg_data`. Configured with user `spindex_user`, password `spindex_password`, database `spindex_db`. Accessible on `postgres:5432`.
*   **`redis`**: Redis server, typically for Celery broker/backend. Accessible on `redis:6379`.
*   **`spindex-c-worker`**: The C application. Built from `SPINDEX/Dockerfile`. Its role (standalone worker vs. library provider) depends on the final design of `spindex_app`'s `main.c`. Environment variables like `SPINDEX_PLUGIN_DIR` and `SPINDEX_CONFIG_FILE` are set within its container.
*   **`spindex-backend`**: The Python FastAPI application. **Note:** This service definition in `docker-compose.yml` is a placeholder and requires a `SPINDEX/backend/Dockerfile` to be created for it. It depends on the other services. Environment variables are set for database connections, NATS, ES, etc., pointing to the respective service names within the Docker network.

### 3.4. Configuration

*   **C Worker (`spindex-c-worker`):**
    *   Configured via `SPINDEX_CONFIG_FILE` (e.g., `/opt/spindex/config/spindex.conf` inside container). The `SPINDEX/test_spindex_es.conf` is copied as this default config in the Docker image. This file may need adjustment for production paths or settings.
    *   Plugin directory is set by `SPINDEX_PLUGIN_DIR` (e.g., `/opt/spindex/plugins`).
*   **Python Backend (`spindex-backend`):**
    *   Configured primarily via environment variables set in `docker-compose.yml` (e.g., `DATABASE_URL`, `NATS_URL`, `ELASTICSEARCH_HOST`).
    *   If it uses the C components via CFFI, environment variables like `SPINDEX_C_LIB_PATH`, `SPINDEX_PLUGIN_DIR_FOR_C_LIBS`, and `SPINDEX_CONFIG_FILE_FOR_C_LIBS` would need to point to the location of C artifacts within its own container image (potentially copied from an image built by `SPINDEX/Dockerfile`).

## 4. Docker Swarm Considerations (Brief)

The provided `docker-compose.yml` (version 3.8) can be used as a base for Docker Swarm deployments.
To deploy to a Swarm:

1.  **Initialize Swarm (if not already):**
    ```sh
    docker swarm init
    ```
2.  **Deploy the stack:**
    ```sh
    docker stack deploy -c docker-compose.yml spindex_stack
    ```
    You might need to pre-build and push images to a registry accessible by all Swarm nodes, or use a registry service.
    For Swarm-specific scaling, update policies, resource limits, etc., you would add `deploy:` keys to the services in `docker-compose.yml`.

## 5. Production Considerations

Deploying to a production environment requires further considerations beyond this guide:

*   **Externalized Services:** Run NATS, Elasticsearch, PostgreSQL, and Redis as managed services or robust, highly-available clusters rather than single Docker containers.
*   **Configuration Management:** Use a dedicated system for managing configurations and secrets (e.g., HashiCorp Vault, environment variables injected by orchestration platform).
*   **Security:**
    *   Enable security features in Elasticsearch, NATS, PostgreSQL.
    *   Regularly update base images and dependencies.
    *   Securely manage API tokens and other credentials.
*   **Persistent Storage:** Use robust, backed-up storage solutions for Docker volumes or database services.
*   **Monitoring and Logging:** Implement comprehensive monitoring (Prometheus, Grafana) and centralized logging (ELK stack, Splunk) for all components.
*   **CI/CD:** Set up automated build, test, and deployment pipelines.
```
