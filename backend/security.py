"""
SPINDEX Security Module
APT-Level Security Measures for Database and Application Protection
"""

import hashlib
import hmac
import secrets
import logging
import re
from datetime import datetime, timedelta
from typing import Optional, Dict, Any, List
from collections import defaultdict, deque
import bleach
import time

logger = logging.getLogger(__name__)


class SecurityConfig:
    """Security configuration constants"""
    # Rate limiting
    MAX_REQUESTS_PER_MINUTE = 60
    MAX_REQUESTS_PER_HOUR = 1000
    MAX_UPLOAD_PER_HOUR = 20

    # File security
    MAX_FILE_SIZE = 100 * 1024 * 1024  # 100MB
    ALLOWED_EXTENSIONS = {'.json', '.csv', '.geojson', '.txt', '.pdf',
                          '.doc', '.docx', '.xls', '.xlsx', '.rtf',
                          '.sqlite', '.db',
                          # Compressed formats (supported by compression.py)
                          '.zip', '.gz', '.tar', '.tgz', '.bz2', '.xz',
                          '.tar.gz', '.tar.bz2', '.tar.xz'}

    # Input validation
    MAX_QUERY_LENGTH = 10000
    MAX_FIELD_LENGTH = 5000

    # Anomaly detection thresholds
    FAILED_AUTH_THRESHOLD = 5  # Failed attempts before alert
    ANOMALY_WINDOW_MINUTES = 15

    # SQL Injection patterns (blacklist approach - use with parameterized queries)
    SQL_INJECTION_PATTERNS = [
        r"(\bUNION\b.*\bSELECT\b)",
        r"(\bDROP\b.*\bTABLE\b)",
        r"(\bINSERT\b.*\bINTO\b.*\bVALUES\b)",
        r"(\bUPDATE\b.*\bSET\b)",
        r"(\bDELETE\b.*\bFROM\b)",
        r"(--|\#|\/\*|\*\/)",  # SQL comments
        r"(\bEXEC\b|\bEXECUTE\b)",
        r"(\bxp_\w+)",  # SQL Server extended procedures
    ]


class InputSanitizer:
    """Sanitize user inputs to prevent injection attacks"""

    @staticmethod
    def sanitize_text(text: str, max_length: int = SecurityConfig.MAX_FIELD_LENGTH) -> str:
        """
        Sanitize text input to prevent XSS and injection attacks
        """
        if not text:
            return ""

        # Truncate to max length
        text = text[:max_length]

        # Remove potentially dangerous HTML/script tags
        text = bleach.clean(text, tags=[], strip=True)

        return text.strip()

    @staticmethod
    def sanitize_filename(filename: str) -> str:
        """
        Sanitize filename to prevent directory traversal
        """
        if not filename:
            return "unknown"

        # Remove path separators
        filename = filename.replace('/', '_').replace('\\', '_')

        # Remove potentially dangerous characters
        filename = re.sub(r'[^\w\s\-\.]', '', filename)

        # Limit length
        if len(filename) > 255:
            name, ext = filename.rsplit('.', 1) if '.' in filename else (filename, '')
            filename = name[:250] + '.' + ext if ext else name[:255]

        return filename

    @staticmethod
    def check_sql_injection(text: str) -> bool:
        """
        Check if text contains SQL injection patterns
        Returns True if suspicious pattern detected
        """
        if not text:
            return False

        text_upper = text.upper()

        for pattern in SecurityConfig.SQL_INJECTION_PATTERNS:
            if re.search(pattern, text_upper, re.IGNORECASE):
                logger.warning(f"Potential SQL injection detected: {pattern}")
                return True

        return False

    @staticmethod
    def validate_query_input(query: str) -> tuple[bool, str]:
        """
        Validate search query input
        Returns (is_valid, error_message)
        """
        if not query:
            return False, "Query cannot be empty"

        if len(query) > SecurityConfig.MAX_QUERY_LENGTH:
            return False, f"Query exceeds maximum length of {SecurityConfig.MAX_QUERY_LENGTH}"

        if InputSanitizer.check_sql_injection(query):
            return False, "Query contains suspicious patterns"

        return True, ""


class FileSecurityChecker:
    """Check uploaded files for security threats"""

    @staticmethod
    def calculate_hash(file_content: bytes, algorithm: str = 'sha256') -> str:
        """Calculate cryptographic hash of file"""
        hasher = hashlib.new(algorithm)
        hasher.update(file_content)
        return hasher.hexdigest()

    @staticmethod
    def check_file_size(size: int) -> tuple[bool, str]:
        """
        Check if file size is within limits
        Returns (is_valid, error_message)
        """
        if size <= 0:
            return False, "File is empty"

        if size > SecurityConfig.MAX_FILE_SIZE:
            max_mb = SecurityConfig.MAX_FILE_SIZE / (1024 * 1024)
            return False, f"File exceeds maximum size of {max_mb:.0f}MB"

        return True, ""

    @staticmethod
    def check_file_extension(filename: str) -> tuple[bool, str]:
        """
        Check if file extension is allowed
        Returns (is_valid, error_message)
        """
        if not filename:
            return False, "Filename is empty"

        # Get extension
        ext = '.' + filename.rsplit('.', 1)[-1].lower() if '.' in filename else ''

        if ext not in SecurityConfig.ALLOWED_EXTENSIONS:
            allowed = ', '.join(SecurityConfig.ALLOWED_EXTENSIONS)
            return False, f"File type not allowed. Allowed types: {allowed}"

        return True, ""

    @staticmethod
    def check_magic_bytes(file_content: bytes, filename: str) -> tuple[bool, str]:
        """
        Check if file magic bytes match the extension (basic check)
        Returns (is_valid, error_message)
        """
        if not file_content or len(file_content) < 4:
            return False, "File content is too small"

        # Basic magic byte signatures
        magic_bytes = {
            b'\x50\x4B\x03\x04': ['.zip', '.docx', '.xlsx'],  # ZIP-based formats
            b'\x25\x50\x44\x46': ['.pdf'],  # PDF
            b'\x7B': ['.json'],  # JSON (starts with {)
            b'\x5B': ['.json'],  # JSON (starts with [)
            b'SQLite format 3': ['.sqlite', '.db'],  # SQLite
        }

        ext = '.' + filename.rsplit('.', 1)[-1].lower() if '.' in filename else ''

        # Check magic bytes
        header = file_content[:20]
        for magic, extensions in magic_bytes.items():
            if header.startswith(magic):
                if ext in extensions:
                    return True, ""

        # CSV, TXT, and other text formats are harder to validate
        if ext in ['.csv', '.txt', '.geojson', '.rtf']:
            # Check if file is mostly ASCII/UTF-8 text
            try:
                file_content[:1000].decode('utf-8')
                return True, ""
            except UnicodeDecodeError:
                return False, "File appears to be binary but claims to be text"

        logger.warning(f"Magic byte validation inconclusive for {filename}")
        return True, ""  # Allow if we can't determine


class AnomalyDetector:
    """Detect anomalous behavior patterns (APT indicators)"""

    def __init__(self):
        self.failed_auth_attempts: Dict[str, deque] = defaultdict(lambda: deque(maxlen=100))
        self.request_patterns: Dict[str, deque] = defaultdict(lambda: deque(maxlen=1000))
        self.upload_attempts: Dict[str, deque] = defaultdict(lambda: deque(maxlen=100))

    def record_failed_auth(self, identifier: str) -> bool:
        """
        Record failed authentication attempt
        Returns True if threshold exceeded (potential attack)
        """
        now = datetime.utcnow()
        self.failed_auth_attempts[identifier].append(now)

        # Count recent failures
        window_start = now - timedelta(minutes=SecurityConfig.ANOMALY_WINDOW_MINUTES)
        recent_failures = sum(1 for ts in self.failed_auth_attempts[identifier]
                             if ts > window_start)

        if recent_failures >= SecurityConfig.FAILED_AUTH_THRESHOLD:
            logger.warning(f"APT ALERT: {recent_failures} failed auth attempts from {identifier}")
            return True

        return False

    def record_request(self, identifier: str, endpoint: str) -> bool:
        """
        Record API request and detect suspicious patterns
        Returns True if anomaly detected
        """
        now = datetime.utcnow()
        self.request_patterns[identifier].append((now, endpoint))

        # Check for rapid-fire requests (potential DoS or automated scanning)
        recent_requests = [ts for ts, _ in self.request_patterns[identifier]
                          if ts > now - timedelta(seconds=10)]

        if len(recent_requests) > 50:  # 50 requests in 10 seconds
            logger.warning(f"APT ALERT: Rapid request pattern from {identifier}")
            return True

        # Check for sequential endpoint scanning
        recent_endpoints = [ep for ts, ep in self.request_patterns[identifier]
                           if ts > now - timedelta(minutes=5)]
        unique_endpoints = len(set(recent_endpoints))

        if unique_endpoints > 20:  # Accessing 20+ different endpoints in 5 minutes
            logger.warning(f"APT ALERT: Endpoint scanning detected from {identifier}")
            return True

        return False

    def record_upload(self, identifier: str) -> bool:
        """
        Record file upload attempt
        Returns True if threshold exceeded
        """
        now = datetime.utcnow()
        self.upload_attempts[identifier].append(now)

        # Count uploads in last hour
        hour_ago = now - timedelta(hours=1)
        recent_uploads = sum(1 for ts in self.upload_attempts[identifier]
                            if ts > hour_ago)

        if recent_uploads >= SecurityConfig.MAX_UPLOAD_PER_HOUR:
            logger.warning(f"APT ALERT: Upload limit exceeded by {identifier}")
            return True

        return False


class DatabaseSecurityManager:
    """Manage database security measures"""

    @staticmethod
    def generate_secure_token(length: int = 32) -> str:
        """Generate cryptographically secure random token"""
        return secrets.token_urlsafe(length)

    @staticmethod
    def hash_sensitive_data(data: str, salt: Optional[str] = None) -> tuple[str, str]:
        """
        Hash sensitive data with salt
        Returns (hashed_data, salt)
        """
        if salt is None:
            salt = secrets.token_hex(16)

        hashed = hashlib.pbkdf2_hmac('sha256',
                                     data.encode('utf-8'),
                                     salt.encode('utf-8'),
                                     100000)  # 100k iterations

        return hashed.hex(), salt

    @staticmethod
    def verify_hashed_data(data: str, hashed: str, salt: str) -> bool:
        """Verify hashed data"""
        new_hash, _ = DatabaseSecurityManager.hash_sensitive_data(data, salt)
        return hmac.compare_digest(new_hash, hashed)

    @staticmethod
    def sanitize_db_identifier(identifier: str) -> str:
        """
        Sanitize database identifiers (table names, column names)
        Only allow alphanumeric and underscore
        """
        return re.sub(r'[^\w]', '', identifier)


class RequestLogger:
    """Log all requests for audit trail"""

    def __init__(self, log_file: str = "audit.log"):
        self.log_file = log_file
        self.request_log = logging.getLogger("audit")
        handler = logging.FileHandler(log_file)
        handler.setFormatter(logging.Formatter(
            '%(asctime)s - %(message)s'
        ))
        self.request_log.addHandler(handler)
        self.request_log.setLevel(logging.INFO)

    def log_request(self,
                   identifier: str,
                   method: str,
                   endpoint: str,
                   status_code: int,
                   duration_ms: float,
                   user_agent: Optional[str] = None):
        """Log API request"""
        self.request_log.info(
            f"REQUEST | ID: {identifier} | {method} {endpoint} | "
            f"Status: {status_code} | Duration: {duration_ms:.2f}ms | "
            f"UA: {user_agent or 'unknown'}"
        )

    def log_security_event(self,
                          event_type: str,
                          identifier: str,
                          details: str,
                          severity: str = "WARNING"):
        """Log security event"""
        self.request_log.warning(
            f"SECURITY | Type: {event_type} | ID: {identifier} | "
            f"Severity: {severity} | Details: {details}"
        )


# Global instances
anomaly_detector = AnomalyDetector()
request_logger = RequestLogger()
