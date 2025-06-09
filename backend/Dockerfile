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

# Set environment variables to help GDAL Python package find GDAL headers and library.
# These paths might vary slightly depending on the exact OS version and how GDAL is installed.
# For Debian Bookworm (which python:3.11-slim is based on), gdal-config and geos-config should handle this,
# but setting them explicitly can sometimes help if pip has trouble.
# ENV CPLUS_INCLUDE_PATH=/usr/include/gdal:/usr/include/geos
# ENV C_INCLUDE_PATH=/usr/include/gdal:/usr/include/geos
# It's often better if gdal-config/geos-config are found and used by pip directly.

# Copy the requirements file into the container at /app/backend
COPY requirements.txt .

# Install any needed packages specified in requirements.txt
# GDAL and Shapely Python packages will be built against the system libraries installed above.
RUN pip install --no-cache-dir --upgrade pip && \
    pip install --no-cache-dir -r requirements.txt

# Copy the rest of the backend application code into the container at /app/backend
COPY . .

# Copy the entrypoint script
COPY entrypoint.sh /app/backend/entrypoint.sh
RUN chmod +x /app/backend/entrypoint.sh

# Expose port 8000 for Uvicorn
EXPOSE 8000
# Redis port 6379 is not exposed externally by default, as it's used internally.

# Set the entrypoint script to run when the container starts
ENTRYPOINT ["/app/backend/entrypoint.sh"]

# CMD is removed as entrypoint.sh now handles the processes.
# Note on GDAL/GEOS Installation:
# Installation of these libraries can be complex due to C/C++ dependencies and the need for
# Python bindings to correctly link against the installed system libraries.
# Using a base image that already includes them (like some from osgeo or gis-focused images)
# can simplify this, but adds to image size. The method used here (installing -dev
# packages via package manager, then Python packages via pip) is common but can be fragile
# if versions mismatch or build tools are not configured correctly.

# Note on Production Setup:
# For robust production environments, it's highly recommended to:
# 1. Run Redis in a separate, dedicated container.
# 2. Use Docker Compose or Kubernetes to manage multi-container applications.
# 3. Employ a proper process manager like supervisord inside the application container
#    if running multiple services (Uvicorn, Celery worker) within a single container is necessary.
# This Dockerfile provides a simplified setup for demonstration and basic testing.
