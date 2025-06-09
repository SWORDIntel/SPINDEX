#!/bin/sh

# Exit immediately if a command exits with a non-zero status.
set -e

# Start Redis server in the background
redis-server --daemonize yes --loglevel notice

# Start Uvicorn server in the background
# Using --host 0.0.0.0 to be accessible externally
# Logging to stdout/stderr for Docker logs
uvicorn main:app --host 0.0.0.0 --port 8000 --workers 1 &

# Start Celery worker in the foreground
# Replace 'main.celery_app' with the actual path to your Celery app instance if different
# The Celery worker will take over the foreground, and Docker logs will primarily show Celery output.
# For more robust logging and process management in production, consider supervisord or similar.
celery -A main.celery_app worker -l info -P solo # Using -P solo for simplicity without needing a separate concurrency lib for this basic setup

# If Celery worker exits, the script will exit, and the container will stop.
# This is a simple setup. For production, you'd want more robust process management.
