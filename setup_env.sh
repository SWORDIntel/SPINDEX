#!/bin/bash
# Script to set up the build environment for SPINDEX

# Exit immediately if a command exits with a non-zero status.
set -e

# Update package lists
sudo apt-get update

# Install essential build tools
sudo apt-get install -y cmake gcc pkg-config

# Install library dependencies
sudo apt-get install -y libjansson-dev libgeos-dev libcurl4-openssl-dev unrtf

# Install Python and pip
sudo apt-get install -y python3 python3-pip

# Install Python libraries (e.g., spacy)
# Note: You might want to use a virtual environment for Python projects
sudo pip3 install spacy

# Download a spacy model (example: en_core_web_sm)
# This might require root privileges if installing globally,
# or you can run it as a user if you have write permissions to the Python lib directory.
# Consider running 'python3 -m spacy download en_core_web_sm' manually if this fails.
sudo python3 -m spacy download en_core_web_sm

echo "Build environment setup complete."
echo "Please ensure you have necessary permissions for all installations."
echo "For Python spaCy model download, you might need to run 'python3 -m spacy download en_core_web_sm' manually if issues occur."
