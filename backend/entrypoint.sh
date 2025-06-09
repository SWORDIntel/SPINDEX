#!/bin/sh

# Exit immediately if a command exits with a non-zero status.
set -e

# Start Redis server in the background
echo "Starting Redis server..."
redis-server --daemonize yes --loglevel notice
# Small delay to ensure Redis is up, though ideally a proper wait-for-it script or check would be used
sleep 2

# Apply database migrations
echo "Applying database migrations..."
alembic upgrade head
echo "Database migrations applied."

# Start Uvicorn server in the background
# Using --host 0.0.0.0 to be accessible externally
# Logging to stdout/stderr for Docker logs
echo "Starting Uvicorn server..."
uvicorn main:app --host 0.0.0.0 --port 8000 --workers 1 &
# Using --workers 1 as specified in original, can be tuned.

# Start Celery worker in the foreground
# The Celery worker will take over the foreground, and Docker logs will primarily show Celery output.
echo "Starting Celery worker..."
celery -A main.celery_app worker -l info -P solo # Using -P solo for simplicity

# If Celery worker exits, the script will exit, and the container will stop.
# This is a simple setup. For production, you'd want more robust process management.
