# 🐳 SPINDEX Docker Quickstart

One-command startup for the complete SPINDEX stack.

## 🚀 Quick Start

### Start Everything
```bash
docker-compose up -d
```

This single command will:
- ✅ Start PostgreSQL database
- ✅ Start Redis (for Celery)
- ✅ Start Elasticsearch
- ✅ Build and start the FastAPI backend
- ✅ Build and start the Vue.js frontend
- ✅ Run database migrations
- ✅ Start Celery worker

**Access the application**: http://localhost

**Access the API**: http://localhost:8000

**API Documentation**: http://localhost:8000/docs

### View Logs
```bash
# All services
docker-compose logs -f

# Specific service
docker-compose logs -f backend
docker-compose logs -f frontend
```

### Check Status
```bash
docker-compose ps
```

### Stop Everything
```bash
docker-compose down
```

### Stop and Remove Volumes (⚠️ deletes all data)
```bash
docker-compose down -v
```

## 📦 Services

| Service | Port | Container Name | Purpose |
|---------|------|----------------|---------|
| Frontend | 80 | spindex_frontend | Vue.js web UI |
| Backend | 8000 | spindex_backend | FastAPI + Celery |
| PostgreSQL | 5432 | spindex_postgres | Primary database |
| Redis | 6379 | spindex_redis | Celery broker |
| Elasticsearch | 9200 | spindex_elasticsearch | Search engine |

## 🔧 Configuration

### Default Credentials

**PostgreSQL:**
- User: `spindex_user`
- Password: `spindex_password`
- Database: `spindex_db`

**API Tokens (development only):**
- Admin: `secure-dev-token-admin`
- Analyst: `secure-dev-token-analyst`
- Viewer: `secure-dev-token-viewer`

⚠️ **Change these in production!**

### Environment Variables

Backend environment variables can be customized in `docker-compose.yml` under the `backend` service.

Key variables:
- `DATABASE_URL` - PostgreSQL connection string
- `CELERY_BROKER_URL` - Redis URL for Celery
- `ELASTICSEARCH_HOST` - Elasticsearch endpoint
- `AEGIS_TOKEN_*` - Authentication tokens
- `RATE_LIMIT_ENABLED` - Enable/disable rate limiting

## 🏥 Health Checks

All services include health checks:

```bash
# Backend health
curl http://localhost:8000/health

# Frontend health
curl http://localhost/health

# Elasticsearch health
curl http://localhost:9200/_cluster/health

# PostgreSQL (from host)
docker exec spindex_postgres pg_isready -U spindex_user

# Redis
docker exec spindex_redis redis-cli ping
```

## 🔄 Rebuild After Code Changes

### Backend changes:
```bash
docker-compose up -d --build backend
```

### Frontend changes:
```bash
docker-compose up -d --build frontend
```

### Rebuild everything:
```bash
docker-compose up -d --build
```

## 📁 Data Persistence

The following volumes persist data between container restarts:

- `postgres_data` - PostgreSQL database
- `elasticsearch_data` - Elasticsearch indices
- `./staging_area` - Uploaded files (staging)
- `./processed_area` - Processed files
- `./trusted_data_area` - Trusted data

## 🐛 Troubleshooting

### Services won't start
```bash
# Check logs
docker-compose logs

# Check specific service
docker-compose logs backend

# Restart all services
docker-compose restart
```

### Port already in use
Edit `docker-compose.yml` and change the port mappings:
```yaml
ports:
  - "8080:80"  # Change frontend to port 8080
```

### Out of disk space
```bash
# Remove unused images
docker image prune -a

# Remove unused volumes
docker volume prune

# Remove everything (⚠️ careful!)
docker system prune -a --volumes
```

### Backend can't connect to database
Wait for PostgreSQL to be fully ready (health check takes ~10s).

Check logs:
```bash
docker-compose logs postgres
docker-compose logs backend
```

### Frontend shows API errors
Ensure backend is healthy:
```bash
curl http://localhost:8000/health
```

Check nginx proxy configuration in `gui/nginx.conf`.

## 🚀 Production Deployment

For production, make these changes in `docker-compose.yml`:

1. **Change authentication tokens** - Use strong random tokens
2. **Enable Elasticsearch security** - Set `xpack.security.enabled=true`
3. **Use environment files** - Create `.env` file instead of hardcoding values
4. **Add SSL/TLS** - Configure nginx with SSL certificates
5. **Increase resources** - Adjust Elasticsearch heap size for your workload
6. **Use external databases** - Point to managed PostgreSQL/Elasticsearch
7. **Set up monitoring** - Add Prometheus/Grafana containers
8. **Enable log aggregation** - Configure centralized logging

Example production `.env` file:
```env
POSTGRES_PASSWORD=<strong-random-password>
AEGIS_TOKEN_ADMIN=<strong-random-token>
AEGIS_TOKEN_ANALYST=<strong-random-token>
AEGIS_TOKEN_VIEWER=<strong-random-token>
ES_JAVA_OPTS=-Xms4g -Xmx4g
```

## 📊 Monitoring

### Container resources
```bash
docker stats
```

### Service-specific metrics
```bash
# Backend metrics (if enabled)
curl http://localhost:8000/metrics

# Elasticsearch cluster stats
curl http://localhost:9200/_cluster/stats?pretty
```

## 🔐 Security Notes

- Default setup is for **development only**
- All passwords/tokens are **publicly visible** in docker-compose.yml
- Elasticsearch security is **disabled** by default
- No SSL/TLS encryption by default
- **Never use default credentials in production**

## 📚 Additional Resources

- [Docker Compose Documentation](https://docs.docker.com/compose/)
- [SPINDEX Documentation](./README.md)
- [API Documentation](./backend/README.md)
- [Frontend Documentation](./gui/README.md)
- [Security Guide](./SECURITY.md)

## 💡 Tips

**Speed up rebuilds:** Docker caches layers, so order your Dockerfile commands from least to most frequently changing.

**Development mode:** Mount source code as volumes for live reload:
```yaml
volumes:
  - ./backend:/app/backend
  - ./gui:/app
```

**Resource limits:** Add resource constraints to prevent runaway containers:
```yaml
deploy:
  resources:
    limits:
      cpus: '2'
      memory: 4G
```
