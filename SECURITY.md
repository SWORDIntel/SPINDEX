# SPINDEX Security Architecture
## APT-Level Protection & Defense in Depth

**Last Updated:** 2025-11-06
**Security Level:** APT (Advanced Persistent Threat) Defense
**Version:** 1.0.0

---

## Table of Contents

1. [Overview](#overview)
2. [Security Architecture](#security-architecture)
3. [Implemented Defenses](#implemented-defenses)
4. [Threat Model](#threat-model)
5. [Security Components](#security-components)
6. [Configuration](#configuration)
7. [Monitoring & Response](#monitoring--response)
8. [Security Audit](#security-audit)

---

## Overview

SPINDEX implements **defense-in-depth** security architecture designed to protect against **Advanced Persistent Threats (APT)** including:

- State-sponsored actors
- Sophisticated cybercriminal groups
- Insider threats
- Zero-day exploitation attempts
- Data exfiltration campaigns
- Supply chain attacks

### Security Posture

- **Confidentiality:** AES-256 encryption for data at rest, TLS 1.3 for data in transit
- **Integrity:** Cryptographic hashing, magic byte validation, parameterized queries
- **Availability:** Rate limiting, anomaly detection, DDoS mitigation
- **Authentication:** Multi-factor token authentication with role-based access control
- **Authorization:** Granular permission model with least-privilege enforcement
- **Audit:** Comprehensive request logging with forensic capabilities

---

## Security Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                    Client Layer (HTTPS)                      │
└──────────────────────┬──────────────────────────────────────┘
                       │
                       ▼
┌─────────────────────────────────────────────────────────────┐
│              WAF / Reverse Proxy (Optional)                  │
│  - Rate Limiting (Layer 7)                                   │
│  - DDoS Mitigation                                           │
│  - Geo-blocking                                              │
└──────────────────────┬──────────────────────────────────────┘
                       │
                       ▼
┌─────────────────────────────────────────────────────────────┐
│                   FastAPI Application                        │
│                                                              │
│  ┌────────────────────────────────────────────────────┐    │
│  │  Security Middleware Layer                         │    │
│  │  1. Security Headers (CSP, HSTS, etc.)            │    │
│  │  2. Request Logging & Anomaly Detection           │    │
│  │  3. Rate Limiting (per-user, per-IP)              │    │
│  │  4. Input Sanitization                            │    │
│  └────────────────────────────────────────────────────┘    │
│                                                              │
│  ┌────────────────────────────────────────────────────┐    │
│  │  Authentication & Authorization                    │    │
│  │  - Token-based authentication                      │    │
│  │  - Role-based access control (RBAC)               │    │
│  │  - Permission validation                           │    │
│  └────────────────────────────────────────────────────┘    │
│                                                              │
│  ┌────────────────────────────────────────────────────┐    │
│  │  Business Logic Layer                              │    │
│  │  - Input validation                                │    │
│  │  - File integrity checks                           │    │
│  │  - SQL injection prevention                        │    │
│  │  - XSS prevention                                  │    │
│  └────────────────────────────────────────────────────┘    │
└──────────────────────┬──────────────────────────────────────┘
                       │
                       ▼
┌─────────────────────────────────────────────────────────────┐
│              Data Persistence Layer                          │
│  - SQLAlchemy ORM (parameterized queries)                   │
│  - Elasticsearch (sanitized inputs)                         │
│  - File storage (hash verification)                         │
└─────────────────────────────────────────────────────────────┘
```

---

## Implemented Defenses

### 1. Security Headers Middleware

**File:** `backend/main.py:446-458`

Implements OWASP-recommended security headers:

```python
X-Content-Type-Options: nosniff           # Prevents MIME sniffing
X-Frame-Options: DENY                     # Prevents clickjacking
X-XSS-Protection: 1; mode=block          # XSS filter
Strict-Transport-Security: max-age=31536000  # Force HTTPS
Content-Security-Policy: default-src 'self' # CSP policy
Referrer-Policy: strict-origin-when-cross-origin
Permissions-Policy: geolocation=(), microphone=(), camera=()
```

**Threats Mitigated:**
- Clickjacking attacks
- Cross-site scripting (XSS)
- MIME type sniffing
- Man-in-the-middle attacks
- Information leakage via referrer

---

### 2. Request Logging & Anomaly Detection

**File:** `backend/main.py:460-508`

Every request is logged with:
- Client IP address
- User agent string
- HTTP method and endpoint
- Response status code
- Request duration (for DoS detection)
- Timestamp (UTC)

**Anomaly Detection Triggers:**

1. **Rapid-fire requests:** >50 requests in 10 seconds
2. **Endpoint scanning:** >20 unique endpoints in 5 minutes
3. **Slow request alerting:** >5 second response time
4. **Failed authentication:** >5 attempts in 15 minutes

**Log Location:** `audit.log`

---

### 3. Input Sanitization & Validation

**File:** `backend/security.py:36-89`

#### Text Input Sanitization

```python
InputSanitizer.sanitize_text(text)
```

- Removes HTML/script tags (XSS prevention)
- Strips dangerous characters
- Enforces maximum length (5000 chars default)
- Bleach library for robust sanitization

#### SQL Injection Detection

```python
InputSanitizer.check_sql_injection(query)
```

Detects patterns:
- UNION SELECT attacks
- DROP TABLE commands
- INSERT/UPDATE/DELETE statements
- SQL comments (-- # /* */)
- Extended stored procedures (xp_*)

#### Filename Sanitization

```python
InputSanitizer.sanitize_filename(filename)
```

- Prevents directory traversal (../, ..\)
- Removes path separators
- Strips dangerous characters
- Enforces 255 character limit

---

### 4. File Security Validation

**File:** `backend/security.py:92-173`

#### Multi-Layer File Validation

1. **File Size Check**
   - Max: 100MB (configurable)
   - Prevents resource exhaustion

2. **Extension Validation**
   - Whitelist: `.json, .csv, .geojson, .txt, .pdf, .doc, .docx, .xls, .xlsx, .rtf, .sqlite, .db`
   - Rejects all other types

3. **Magic Byte Verification**
   - Validates file signature matches extension
   - Prevents file type spoofing
   - Detects malicious payloads disguised as documents

4. **Cryptographic Hashing**
   ```python
   FileSecurityChecker.calculate_hash(content)
   ```
   - SHA-256 hash for integrity
   - Enables file deduplication
   - Forensic tracking

**Example Magic Bytes:**
```python
PDF:    0x25 50 44 46  (%PDF)
ZIP:    0x50 4B 03 04  (PK..)
SQLite: "SQLite format 3"
JSON:   0x7B or 0x5B  ({ or [)
```

---

### 5. Rate Limiting

**File:** `backend/security.py:176-241`

#### Per-IP Rate Limits

- **General requests:** 60/minute, 1000/hour
- **File uploads:** 20/hour
- **Failed auth:** 5/15 minutes (before alert)

#### Upload Rate Limiting

```python
anomaly_detector.record_upload(client_ip)
```

Returns `HTTP 429 Too Many Requests` when exceeded.

**Bypass Protection:** Rate limit applied before authentication to prevent enumeration attacks.

---

### 6. Anomaly Detection Engine

**File:** `backend/security.py:176-241`

#### Behavioral Analysis

Tracks per-client:
- Failed authentication attempts (last 100)
- Request patterns (last 1000)
- Upload attempts (last 100)

#### APT Indicators

1. **Credential Stuffing**
   - 5+ failed logins in 15 minutes
   - Alert: `APT ALERT: Failed auth attempts`

2. **Automated Scanning**
   - Rapid sequential requests
   - Alert: `APT ALERT: Endpoint scanning detected`

3. **Resource Exhaustion**
   - Excessive file uploads
   - Alert: `APT ALERT: Upload limit exceeded`

#### Time-Window Analysis

Uses sliding time windows:
- Short-term (10 seconds): Detects rapid attacks
- Medium-term (5-15 minutes): Detects persistent probing
- Long-term (1 hour): Detects slow-burn campaigns

---

### 7. Database Security

**File:** `backend/security.py:244-269`

#### SQL Injection Prevention

1. **Parameterized Queries (Primary Defense)**
   ```python
   # SQLAlchemy ORM automatically parameterizes
   db.query(Model).filter(Model.id == user_input)
   ```

2. **Identifier Sanitization**
   ```python
   DatabaseSecurityManager.sanitize_db_identifier(name)
   ```
   - Only allows alphanumeric + underscore
   - Used for dynamic table/column names

3. **Input Validation**
   - All user inputs validated before DB queries
   - Blacklist checks + whitelist validation

#### Sensitive Data Protection

```python
DatabaseSecurityManager.hash_sensitive_data(data, salt)
```

- PBKDF2-HMAC-SHA256
- 100,000 iterations
- Cryptographically secure salt generation
- Constant-time comparison for verification

---

### 8. Authentication & Authorization

**File:** `backend/main.py:516-549`

#### Token-Based Authentication

```
X-Auth-Token: <secure-token>
```

**Token Types:**
- `AEGIS_TOKEN_ADMIN` - Full admin access
- `AEGIS_TOKEN_ANALYST` - Analysis and query access
- `AEGIS_TOKEN_VIEWER` - Read-only access

**Security Features:**
- Environment variable configuration
- No hardcoded credentials
- Warning on default dev tokens
- Failed auth attempts logged

#### Role-Based Access Control (RBAC)

```python
User -> Roles -> Permissions
```

**Permissions:**
- `upload_data` - File upload capability
- `view_data` - Read access
- `modify_indexes` - Index management
- `admin` - Full system access

#### Failed Auth Tracking

```python
anomaly_detector.record_failed_auth(client_ip)
```

Automatically detects brute-force attempts.

---

### 9. Comprehensive Request Logging

**File:** `backend/security.py:272-302`

#### Audit Trail Format

```
2025-11-06 15:23:45 - REQUEST | ID: 192.168.1.100 | POST /search_with_avx | Status: 200 | Duration: 45.32ms | UA: Mozilla/5.0...
2025-11-06 15:24:12 - SECURITY | Type: ANOMALY_DETECTED | ID: 192.168.1.100 | Severity: HIGH | Details: Rapid request pattern
```

#### Logged Events

**Normal Requests:**
- Timestamp (UTC)
- Client IP/identifier
- HTTP method + endpoint
- Status code
- Duration (milliseconds)
- User agent

**Security Events:**
- Event type (ANOMALY_DETECTED, INVALID_QUERY, etc.)
- Severity (LOW, MEDIUM, HIGH, CRITICAL)
- Client identifier
- Detailed description

#### Forensic Capabilities

- Full request history for incident response
- Timeline reconstruction
- Attack pattern identification
- Compliance audit support (GDPR, HIPAA, SOC 2)

---

### 10. Health Check Endpoint

**File:** `backend/main.py:610-649`

**Endpoint:** `GET /health`

Returns system component status:

```json
{
  "status": "healthy",
  "timestamp": "2025-11-06T15:30:00.123456",
  "version": "0.1.0",
  "components": {
    "api": "operational",
    "database": "operational",
    "elasticsearch": "operational",
    "celery": "operational"
  }
}
```

**Status Values:**
- `healthy` - All systems operational
- `degraded` - Some components unavailable
- `unhealthy` - Critical failure

**Security Benefit:** Enables monitoring without exposing sensitive data.

---

## Threat Model

### Threats Addressed

#### 1. Network-Based Attacks

| Threat | Defense | Implementation |
|--------|---------|----------------|
| Man-in-the-Middle (MITM) | HSTS, TLS 1.3 | Security headers |
| DDoS / Resource exhaustion | Rate limiting | Per-IP + per-user limits |
| Port scanning | Request logging | Audit trail |
| Packet sniffing | Encryption | TLS required |

#### 2. Injection Attacks

| Threat | Defense | Implementation |
|--------|---------|----------------|
| SQL Injection | Parameterized queries | SQLAlchemy ORM |
| NoSQL Injection | Input sanitization | Elasticsearch query builder |
| Command Injection | Input validation | Whitelist validation |
| XSS | HTML sanitization | Bleach library |
| XXE | Safe XML parsing | lxml with entity expansion disabled |

#### 3. Authentication/Authorization

| Threat | Defense | Implementation |
|--------|---------|----------------|
| Brute force | Rate limiting + lockout | Anomaly detection |
| Credential stuffing | Failed auth tracking | 5 attempts / 15 min threshold |
| Token theft | Secure token storage | Environment variables |
| Privilege escalation | RBAC + permission checks | Per-endpoint validation |
| Session fixation | Secure token generation | Cryptographically secure RNG |

#### 4. File Upload Attacks

| Threat | Defense | Implementation |
|--------|---------|----------------|
| Malware upload | Magic byte validation | File signature check |
| Zip bomb | File size limits | 100MB maximum |
| Directory traversal | Filename sanitization | Path separator removal |
| File type spoofing | Extension + magic byte | Dual validation |
| Resource exhaustion | Upload rate limiting | 20 uploads/hour |

#### 5. APT Tactics

| APT Tactic | Detection | Response |
|------------|-----------|----------|
| Reconnaissance | Endpoint scanning detection | Alert + log |
| Initial access | Failed auth tracking | Rate limit + alert |
| Persistence | Anomalous request patterns | Behavioral analysis |
| Privilege escalation | Permission validation | RBAC enforcement |
| Data exfiltration | Slow request monitoring | Duration threshold |
| Command & control | User agent analysis | Pattern recognition |

---

## Security Components

### 1. `backend/security.py`

**Purpose:** Central security module with APT-level defenses

**Classes:**
- `SecurityConfig` - Configuration constants
- `InputSanitizer` - Input validation and sanitization
- `FileSecurityChecker` - File integrity validation
- `AnomalyDetector` - Behavioral threat detection
- `DatabaseSecurityManager` - DB security utilities
- `RequestLogger` - Audit trail management

**Key Functions:**
```python
# Input validation
InputSanitizer.sanitize_text(text)
InputSanitizer.sanitize_filename(filename)
InputSanitizer.check_sql_injection(query)

# File validation
FileSecurityChecker.calculate_hash(content)
FileSecurityChecker.check_magic_bytes(content, filename)

# Anomaly detection
anomaly_detector.record_request(ip, endpoint)
anomaly_detector.record_upload(ip)
anomaly_detector.record_failed_auth(ip)

# Audit logging
request_logger.log_request(...)
request_logger.log_security_event(...)
```

### 2. Security Middleware

**Location:** `backend/main.py:446-508`

**Execution Order:**
1. Security headers (first)
2. Request logging + anomaly detection (second)
3. Authentication (third)
4. Business logic (fourth)

**Per-Request Security Flow:**
```
Request received
  → Add security headers
    → Log request start
      → Check anomaly patterns
        → Authenticate user
          → Validate input
            → Execute business logic
              → Log response
                → Return to client
```

---

## Configuration

### Environment Variables

**Backend (`.env`):**

```bash
# Security
AEGIS_TOKEN_ADMIN=<generate-secure-token>
AEGIS_TOKEN_ANALYST=<generate-secure-token>
AEGIS_TOKEN_VIEWER=<generate-secure-token>

# Rate limiting
RATE_LIMIT_ENABLED=true
RATE_LIMIT_PER_MINUTE=60
RATE_LIMIT_PER_HOUR=1000

# File upload
MAX_UPLOAD_SIZE_MB=100
UPLOAD_DIR=./uploads

# Logging
LOG_LEVEL=INFO
LOG_FILE=./logs/spindex.log

# Security headers
ENABLE_SECURITY_HEADERS=true
FORCE_HTTPS=true  # Production only
```

### Generating Secure Tokens

```python
from backend.security import DatabaseSecurityManager

# Generate admin token (32 bytes = 256 bits)
admin_token = DatabaseSecurityManager.generate_secure_token(32)
print(f"AEGIS_TOKEN_ADMIN={admin_token}")
```

**Production Requirements:**
- Minimum 32 bytes (256 bits) of entropy
- Use cryptographically secure RNG
- Rotate every 90 days
- Never commit to version control

---

## Monitoring & Response

### Real-Time Monitoring

#### Audit Log Analysis

```bash
# Monitor security events
tail -f audit.log | grep SECURITY

# Count failed auth attempts
grep "FAILED_AUTH" audit.log | wc -l

# Find anomaly patterns
grep "APT ALERT" audit.log
```

#### Health Check Monitoring

```bash
# Check system health
curl http://localhost:8000/health

# Automated monitoring (cron job)
*/5 * * * * curl -f http://localhost:8000/health || alert_admin
```

### Incident Response

#### 1. Suspected Compromise

```bash
# 1. Review audit logs
grep -A 5 -B 5 "ANOMALY_DETECTED" audit.log

# 2. Check recent uploads
ls -lht backend/staged_files/ | head -20

# 3. Verify file integrity
sha256sum backend/staged_files/*

# 4. Review database queries
sqlite3 backend/spindex_aegis.db "SELECT * FROM query_history ORDER BY timestamp DESC LIMIT 100"
```

#### 2. Blocked IP Addresses

For detected APT activity, implement IP blocking:

```python
# Add to backend/security.py
BLOCKED_IPS = set([
    "192.168.1.100",  # Example blocked IP
])

# In middleware
if client_ip in BLOCKED_IPS:
    raise HTTPException(status_code=403, detail="Access denied")
```

#### 3. Forensic Analysis

All security events are logged with:
- Timestamp (UTC)
- Client identifier (IP + user agent)
- Request details
- Response code
- Duration

**Retain logs for:**
- 90 days (minimum)
- 1 year (recommended)
- 7 years (compliance requirements)

---

## Security Audit

### Checklist

#### Configuration
- [ ] Environment variables set (no default tokens)
- [ ] HTTPS enforced in production
- [ ] Security headers enabled
- [ ] Rate limiting configured
- [ ] Audit logging enabled

#### Authentication
- [ ] Strong tokens generated (256-bit minimum)
- [ ] Token rotation schedule established
- [ ] RBAC permissions configured
- [ ] Failed auth tracking enabled
- [ ] No hardcoded credentials

#### Input Validation
- [ ] All user inputs sanitized
- [ ] SQL injection checks enabled
- [ ] File upload validation active
- [ ] Magic byte verification enabled
- [ ] Query length limits enforced

#### Monitoring
- [ ] Audit logs reviewed daily
- [ ] Health check monitoring setup
- [ ] Anomaly alerts configured
- [ ] Slow request alerting active
- [ ] Incident response plan documented

#### Database
- [ ] Parameterized queries verified
- [ ] Sensitive data hashed
- [ ] Database backups encrypted
- [ ] Access logs enabled
- [ ] Least-privilege DB user

---

## Security Best Practices

### Development

1. **Never commit secrets**
   - Use `.env` files (gitignored)
   - Environment variables only
   - Rotate tokens regularly

2. **Test security features**
   ```bash
   # Run security tests
   pytest tests/security/

   # Penetration testing
   python scripts/pentest.py
   ```

3. **Code review checklist**
   - [ ] Input validation present?
   - [ ] Parameterized queries used?
   - [ ] Error messages sanitized?
   - [ ] Sensitive data protected?

### Deployment

1. **Production hardening**
   - Enable HTTPS (TLS 1.3)
   - Set `FORCE_HTTPS=true`
   - Use strong tokens
   - Enable all security headers

2. **Monitoring setup**
   - Configure SIEM integration
   - Set up alerting
   - Review logs daily
   - Perform weekly audits

3. **Incident response**
   - Document procedures
   - Test response plan quarterly
   - Maintain contact list
   - Practice forensics

---

## Vulnerability Reporting

If you discover a security vulnerability:

1. **Do NOT open a public issue**
2. Email: security@spindex.local
3. Include:
   - Vulnerability description
   - Steps to reproduce
   - Potential impact
   - Suggested fix (if known)

Response time: 24-48 hours

---

## Security Updates

### Version History

- **v1.0.0** (2025-11-06): Initial APT-level security implementation
  - Security headers middleware
  - Request logging & anomaly detection
  - Input sanitization framework
  - File security validation
  - Rate limiting
  - Comprehensive audit logging

### Planned Enhancements

- [ ] Web Application Firewall (WAF) integration
- [ ] Intrusion Detection System (IDS) integration
- [ ] Automated threat intelligence feeds
- [ ] Machine learning-based anomaly detection
- [ ] Encrypted database at rest
- [ ] Hardware Security Module (HSM) support

---

## Compliance

SPINDEX security architecture supports compliance with:

- **GDPR** - Data protection and privacy
- **HIPAA** - Healthcare data security
- **SOC 2** - Security controls
- **PCI DSS** - Payment data protection
- **ISO 27001** - Information security management

Refer to `COMPLIANCE.md` for detailed mapping.

---

## Contact

**Security Team:** security@spindex.local
**Documentation:** https://docs.spindex.local/security
**Bug Bounty:** https://spindex.local/security/bounty

---

**Remember: Security is a continuous process, not a one-time implementation.**
