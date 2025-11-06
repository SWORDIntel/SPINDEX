SPINDEX Sample Data Collection
==============================

This directory contains sample files for testing SPINDEX processing capabilities.

Files included:
- documents.json: Sample JSON documents with metadata
- locations.csv: Geographic locations in CSV format
- regions.geojson: Spatial features in GeoJSON format
- readme.txt: This file

Processing these files demonstrates:
1. JSON parsing and extraction
2. CSV data conversion
3. GeoJSON geometry validation
4. Text content indexing
5. Multi-format pipeline processing

The SPINDEX system can process thousands of files like these in parallel,
utilizing all 21 plugin modules for preprocessing, parsing, and indexing.

For best performance with large datasets:
- Increase max_concurrent_files in config.ini
- Use SSD storage for input files
- Configure Elasticsearch cluster for indexing
- Enable AVX-512 acceleration for search operations

System Capabilities:
- Designed for 500TB+ datasets
- Multi-threaded processing pipeline
- Plugin-based extensibility
- Post-quantum cryptographic security
- Real-time indexing and search
