# SPINDEX Quick Start Guide

## 🎉 System is Ready!

The SPINDEX backend is now configured and running. This guide will show you how to use it.

## ✅ What's Currently Running

- **Backend API**: http://localhost:8000
- **API Documentation**: http://localhost:8000/docs
- **Redis**: localhost:6379
- **Database**: SQLite at `backend/spindex_aegis.db`

## 🚀 Quick Test

### 1. View API Documentation
Open your browser to:
```
http://localhost:8000/docs
```

You'll see the interactive Swagger UI with all available endpoints.

### 2. Upload Test Data

Upload the included test GeoJSON file:

```bash
curl -X POST http://localhost:8000/aegis/upload \
  -H "X-Auth-Token: dev-token-admin" \
  -F "file=@test_data.geojson" \
  -F "user_id=test_user"
```

This will:
- Upload the file to the staging area
- Create an ingestion job
- Trigger asynchronous processing via Celery
- Return a job ID for tracking

Response example:
```json
{
  "job_id": "550e8400-e29b-41d4-a716-446655440000",
  "filename": "test_data.geojson",
  "message": "File uploaded successfully and queued for processing.",
  "staged_filepath": "staging_area/550e8400-e29b-41d4-a716-446655440000_test_data.geojson"
}
```

### 3. Check Job Status

```bash
curl -X GET http://localhost:8000/aegis/jobs/{job_id} \
  -H "X-Auth-Token: dev-token-admin"
```

Replace `{job_id}` with the ID from the upload response.

### 4. List All Jobs

```bash
curl -X GET "http://localhost:8000/aegis/jobs?skip=0&limit=10" \
  -H "X-Auth-Token: dev-token-admin"
```

## 🔑 Authentication

The system uses header-based authentication with development tokens:

| Role | Token | Permissions |
|------|-------|-------------|
| Admin | `dev-token-admin` | Full access |
| Analyst | `dev-token-analyst` | Create, query, upload |
| Viewer | `dev-token-viewer` | Query only |

Include the token in requests:
```bash
-H "X-Auth-Token: dev-token-admin"
```

## 📊 Available Endpoints

### Data Ingestion (AEGIS Pipeline)
- `POST /aegis/upload` - Upload and process data files
- `GET /aegis/jobs` - List ingestion jobs
- `GET /aegis/jobs/{job_id}` - Get job details
- `POST /aegis/jobs/{job_id}/promote` - Promote processed data

### Index Management
- `POST /indexes/create` - Create search index
- `GET /indexes` - List all indexes
- `GET /indexes/{index_id}` - Get index details

### Search (requires Elasticsearch)
- `POST /search/avx` - Perform AVX-512 accelerated search
- `GET /query-history` - View query history

## 🔄 Starting Celery Worker

For full data processing, start the Celery worker:

```bash
cd /home/user/SPINDEX
celery -A backend.aegis_worker worker --loglevel=info
```

This enables:
- Asynchronous file processing
- Format conversion
- Data validation
- Security scanning

## 📝 Data Processing Pipeline

When you upload a file, it goes through these stages:

1. **Upload** → Staging area
2. **Security Scan** → Threat detection
3. **Format Detection** → Identify file type
4. **Format Conversion** → Convert to GeoJSON (if GDAL available)
5. **CRS Detection** → Identify coordinate system
6. **CRS Transformation** → Reproject to EPSG:4326
7. **Geocoding** → Add coordinates for addresses (if Geopy available)
8. **Geometry Handling** → Process complex geometries (if Shapely available)
9. **Data Promotion** → Move to trusted area

**Note**: Without GDAL, Shapely, and Geopy, the system works in limited mode:
- Files are uploaded and tracked
- Security scanning works
- Database operations work
- Full geospatial processing is deferred

## 🛠️ System Components Status

| Component | Status | Notes |
|-----------|---------|-------|
| Backend API | ✅ Running | Port 8000 |
| Redis | ✅ Running | Port 6379 |
| SQLite DB | ✅ Ready | Tables created |
| Elasticsearch | ⚠️ Optional | Not running (search disabled) |
| GDAL/Geospatial | ⚠️ Optional | Not installed (geo features limited) |
| C Modules (AVX-512) | ⚠️ Optional | Not built (advanced search disabled) |
| Celery Worker | ❌ Manual | Run separately |

## 📦 Supported File Formats

The system is designed to handle:

- **GeoJSON** (.geojson, .json)
- **KML** (.kml)
- **Shapefiles** (.zip containing .shp, .shx, .dbf)
- **GML** (.gml, .xml)
- **CSV** with spatial data (.csv)
- **GeoPackage** (.gpkg)

**Current limitation**: Without GDAL, only GeoJSON can be processed. Install GDAL to unlock all formats.

## 🔧 Installing Full Geospatial Support

To enable full geospatial features, install GDAL:

```bash
# Ubuntu/Debian
apt-get install -y gdal-bin libgdal-dev libgeos-dev libproj-dev

# Then install Python bindings
pip3 install GDAL shapely geopy
```

## 📚 Next Steps

1. **Install Elasticsearch** - For advanced search: `docker run -d -p 9200:9200 -e "discovery.type=single-node" elasticsearch:7.17.10`
2. **Install GDAL** - For geospatial processing (see above)
3. **Build C Modules** - For AVX-512 search: `cd SPINDEX && make all`
4. **Configure Production** - Set environment variables for tokens, DB, etc.

## 🐛 Troubleshooting

### Backend won't start
```bash
# Check if port 8000 is already in use
lsof -i :8000

# View backend logs
tail -f backend.log
```

### Redis not running
```bash
redis-server --daemonize yes
redis-cli ping  # Should return PONG
```

### Celery worker errors
```bash
# Make sure Redis is running first
redis-cli ping

# Check worker logs for specific errors
celery -A backend.aegis_worker worker --loglevel=debug
```

## 📖 Full Documentation

- **API Docs**: http://localhost:8000/docs
- **Setup Status**: See `SETUP_STATUS.md`
- **Architecture**: See `README.md`

## 💡 Example Workflows

### Basic File Upload Test
```bash
# 1. Upload
curl -X POST http://localhost:8000/aegis/upload \
  -H "X-Auth-Token: dev-token-admin" \
  -F "file=@test_data.geojson" \
  -F "user_id=admin"

# 2. Check status (use job_id from response)
curl http://localhost:8000/aegis/jobs/YOUR_JOB_ID \
  -H "X-Auth-Token: dev-token-admin"

# 3. List all jobs
curl http://localhost:8000/aegis/jobs \
  -H "X-Auth-Token: dev-token-admin"
```

### Creating a Search Index
```bash
curl -X POST http://localhost:8000/indexes/create \
  -H "X-Auth-Token: dev-token-admin" \
  -H "Content-Type: application/json" \
  -d '{
    "promoted_data_path": "trusted_data_area/your_file.geojson",
    "index_name": "my_index",
    "index_type": "elasticsearch"
  }'
```

## 🎯 Current Capabilities

✅ **Working Now:**
- File upload and tracking
- Job management and status
- Database operations
- Security scanning
- API authentication
- REST API with documentation

⏳ **Available with Additional Setup:**
- Geospatial processing (install GDAL)
- Advanced search (install Elasticsearch)
- Geocoding (install Geopy)
- AVX-512 search (build C modules)

The system is functional and ready for basic data ingestion and management. Install additional components as needed for full features.
