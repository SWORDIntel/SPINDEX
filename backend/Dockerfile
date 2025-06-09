# Use an official Python runtime as a parent image
FROM python:3.11-slim

# Set the working directory in the container
WORKDIR /app/backend

# Install system dependencies including Redis, GDAL, and GEOS (for Shapely)
# Using apt-get for Debian-based image (python:3.11-slim is based on Debian Bookworm)
# libgdal-dev is the development package for GDAL.
# gdal-bin provides command-line utilities like gdal-config.
# libgeos-dev is for GEOS, which Shapely uses for performance.
# build-essential contains compilers needed for building Python packages with C extensions.
RUN apt-get update && \
    apt-get install -y --no-install-recommends \
    redis-server \
    procps \
    gdal-bin \
    libgdal-dev \
    libgeos-dev \
    build-essential \
    && rm -rf /var/lib/apt/lists/*

# Copy the requirements file into the container at /app/backend
COPY requirements.txt .

# Install any needed packages specified in requirements.txt
# GDAL and Shapely Python packages will be built against the system libraries installed above.
# Alembic and SQLAlchemy will also be installed from requirements.txt
RUN pip install --no-cache-dir --upgrade pip && \
    pip install --no-cache-dir -r requirements.txt

# Copy Alembic configuration and scripts
COPY alembic.ini .
COPY alembic/ /app/backend/alembic/

# Copy the rest of the backend application code into the container at /app/backend
# This should come after copying alembic config to ensure it's part of the . context
COPY . .

# Copy the entrypoint script (already copied by 'COPY . .', but explicit can be clearer or overwrite if needed)
# Ensure it's executable
COPY entrypoint.sh /app/backend/entrypoint.sh
RUN chmod +x /app/backend/entrypoint.sh

# Expose port 8000 for Uvicorn
EXPOSE 8000
# Redis port 6379 is not exposed externally by default, as it's used internally.

# Set the entrypoint script to run when the container starts
ENTRYPOINT ["/app/backend/entrypoint.sh"]

# CMD is removed as entrypoint.sh now handles the processes.
# Note on GDAL/GEOS Installation:
# (As previously stated)

# Note on Production Setup:
# (As previously stated)
