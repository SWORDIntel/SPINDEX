# Post-Quantum Cryptography Implementation

**Date**: 2025-11-06
**Status**: ✅ Complete and Ready
**Security Level**: NIST Level 5 (256-bit post-quantum security)

---

## Overview

SPINDEX now implements NIST-standardized post-quantum cryptography algorithms to provide protection against both classical and quantum computer attacks. This implementation meets the latest FIPS standards and provides quantum-resistant encryption for all sensitive data.

### Algorithms Implemented

| Algorithm | NIST Standard | Purpose | Security Level |
|-----------|---------------|---------|----------------|
| **ML-KEM-1024** | FIPS 203 | Key Encapsulation | Level 5 (256-bit) |
| **ML-DSA-87** | FIPS 204 | Digital Signatures | Level 5 (256-bit) |
| **AES-256-GCM** | FIPS 197 | Symmetric Encryption | 256-bit |

---

## Architecture

### Hybrid Encryption Design

The implementation uses a **hybrid approach** combining post-quantum key exchange with classical symmetric encryption:

```
┌─────────────────────────────────────────────────────────────┐
│                     Hybrid Encryption Flow                   │
└─────────────────────────────────────────────────────────────┘

1. Key Encapsulation (ML-KEM-1024)
   ├─> Sender encapsulates random shared secret
   ├─> Uses recipient's ML-KEM public key
   └─> Produces: (ciphertext, shared_secret)

2. Key Derivation (HKDF-SHA256)
   ├─> Derives AES-256 key from shared secret
   └─> Produces: 32-byte AES key

3. Symmetric Encryption (AES-256-GCM)
   ├─> Encrypts plaintext with derived key
   ├─> Provides authenticated encryption
   └─> Produces: (ciphertext, nonce, tag)

4. Package
   └─> Returns: {kem_ciphertext, aes_ciphertext, nonce, tag}
```

### Why Hybrid?

- **Performance**: AES-256-GCM is extremely fast for bulk data
- **Security**: ML-KEM-1024 provides quantum-resistant key exchange
- **Compatibility**: Standard approach used by TLS 1.3, Signal, etc.
- **Efficiency**: Only uses PQC for key exchange, not data encryption

---

## Module: `backend/pqcrypto.py`

### Classes

#### 1. MLKEMKeyPair

**Purpose**: ML-KEM-1024 key encapsulation mechanism for quantum-resistant key exchange.

```python
from backend.pqcrypto import MLKEMKeyPair

# Generate key pair
alice_keys = MLKEMKeyPair()
alice_public = alice_keys.export_public_key()

# Bob encapsulates shared secret using Alice's public key
bob_kem = MLKEMKeyPair()
ciphertext, bob_shared_secret = bob_kem.encapsulate(alice_public)

# Alice decapsulates to get the same secret
alice_shared_secret = alice_keys.decapsulate(ciphertext)

# Both now have the same 32-byte shared secret
assert bob_shared_secret == alice_shared_secret
```

**Methods**:
- `__init__()` - Generate new key pair
- `export_public_key() -> bytes` - Export public key
- `encapsulate(recipient_public_key: bytes) -> Tuple[bytes, bytes]` - Encapsulate shared secret
- `decapsulate(ciphertext: bytes) -> bytes` - Decapsulate shared secret
- `save_keys(prefix: str)` - Save keys to files
- `load_keys(prefix: str)` - Load keys from files (class method)

**Key Sizes**:
- Public key: 1,568 bytes
- Private key: 3,168 bytes
- Ciphertext: 1,568 bytes
- Shared secret: 32 bytes

#### 2. MLDSAKeyPair

**Purpose**: ML-DSA-87 digital signatures for data integrity and authentication.

```python
from backend.pqcrypto import MLDSAKeyPair

# Generate signing key pair
signer_keys = MLDSAKeyPair()
public_key = signer_keys.export_public_key()

# Sign a message
message = b"Critical SPINDEX data payload"
signature = signer_keys.sign(message)

# Verify signature (anyone with public key can verify)
is_valid = MLDSAKeyPair.verify(message, signature, public_key)
assert is_valid == True
```

**Methods**:
- `__init__()` - Generate new key pair
- `export_public_key() -> bytes` - Export public key
- `sign(message: bytes) -> bytes` - Sign message
- `verify(message: bytes, signature: bytes, public_key: bytes) -> bool` - Verify signature (static method)
- `save_keys(prefix: str)` - Save keys to files
- `load_keys(prefix: str)` - Load keys from files (class method)

**Key Sizes**:
- Public key: 2,592 bytes
- Private key: 4,896 bytes
- Signature: 4,627 bytes

#### 3. HybridEncryption

**Purpose**: Combines ML-KEM-1024 + AES-256-GCM for authenticated encryption.

```python
from backend.pqcrypto import HybridEncryption, MLKEMKeyPair

# Recipient generates key pair
recipient_keys = MLKEMKeyPair()
recipient_public = recipient_keys.export_public_key()

# Encrypt data
plaintext = b"Classified SPINDEX intelligence data"
aad = b"metadata:classification=top-secret"

encrypted = HybridEncryption.encrypt(
    plaintext=plaintext,
    recipient_public_key=recipient_public,
    associated_data=aad
)

# Decrypt data
decrypted = HybridEncryption.decrypt(
    encrypted_data=encrypted,
    kem_keypair=recipient_keys,
    associated_data=aad
)

assert decrypted == plaintext
```

**Methods**:
- `encrypt(plaintext: bytes, recipient_public_key: bytes, associated_data: Optional[bytes]) -> Dict[str, str]` - Encrypt data
- `decrypt(encrypted_data: Dict[str, str], kem_keypair: MLKEMKeyPair, associated_data: Optional[bytes]) -> bytes` - Decrypt data

**Encrypted Package Format**:
```json
{
  "algorithm": "ML-KEM-1024+AES-256-GCM",
  "version": "1.0",
  "kem_ciphertext": "<base64>",
  "ciphertext": "<base64>",
  "nonce": "<base64>",
  "tag": "<base64>"
}
```

---

## Installation

### Prerequisites

The PQC module requires the **liboqs** C library from the Open Quantum Safe project.

### Install liboqs

```bash
# Automated installation (Ubuntu/Debian)
sudo backend/install_liboqs.sh

# This script will:
# 1. Install build dependencies (cmake, gcc, ninja, etc.)
# 2. Clone liboqs from GitHub
# 3. Build with CMake and Ninja
# 4. Install to /usr/local
# 5. Update library cache with ldconfig
```

### Verify Installation

```bash
# Check library is installed
ldconfig -p | grep liboqs

# Test Python bindings
python3 -c "import oqs; print(oqs.get_enabled_KEM_mechanisms())"

# Should see: ['Kyber512', 'Kyber768', 'Kyber1024', ...]
```

### Python Dependencies

```bash
# Already in requirements.txt
pip install liboqs-python cryptography
```

---

## Usage Examples

### Example 1: Key Exchange

```python
from backend.pqcrypto import MLKEMKeyPair

# Alice generates her key pair
alice_keys = MLKEMKeyPair()
alice_public = alice_keys.export_public_key()

# Bob encapsulates a secret using Alice's public key
bob_kem = MLKEMKeyPair()
ciphertext, bob_secret = bob_kem.encapsulate(alice_public)

# Alice decapsulates to get the same secret
alice_secret = alice_keys.decapsulate(ciphertext)

# Both parties now have the same shared secret
print(f"Match: {bob_secret == alice_secret}")
```

### Example 2: Digital Signatures

```python
from backend.pqcrypto import MLDSAKeyPair

# Generate signing key pair
signer_keys = MLDSAKeyPair()
public_key = signer_keys.export_public_key()

# Sign critical data
data = b"SPINDEX spatial index for region X"
signature = signer_keys.sign(data)

# Anyone can verify with the public key
is_valid = MLDSAKeyPair.verify(data, signature, public_key)
print(f"Valid: {is_valid}")

# Tampered data fails verification
tampered = b"TAMPERED data"
is_valid_tampered = MLDSAKeyPair.verify(tampered, signature, public_key)
print(f"Tampered valid: {is_valid_tampered}")  # False
```

### Example 3: File Encryption

```python
from backend.pqcrypto import HybridEncryption, MLKEMKeyPair
import json

# Generate recipient keys
recipient_keys = MLKEMKeyPair()

# Read file
with open('sensitive_data.txt', 'rb') as f:
    plaintext = f.read()

# Encrypt
encrypted = HybridEncryption.encrypt(
    plaintext=plaintext,
    recipient_public_key=recipient_keys.export_public_key(),
    associated_data=b"filename:sensitive_data.txt"
)

# Save encrypted file
with open('sensitive_data.txt.enc', 'w') as f:
    json.dump(encrypted, f, indent=2)

# Load and decrypt
with open('sensitive_data.txt.enc', 'r') as f:
    encrypted_data = json.load(f)

decrypted = HybridEncryption.decrypt(
    encrypted_data=encrypted_data,
    kem_keypair=recipient_keys,
    associated_data=b"filename:sensitive_data.txt"
)

assert decrypted == plaintext
```

### Example 4: Key Management

```python
from backend.pqcrypto import generate_keypairs, MLKEMKeyPair, MLDSAKeyPair

# Generate and save all keys
paths = generate_keypairs('keys/spindex')

print("Generated keys:")
for key_type, path in paths.items():
    print(f"  {key_type}: {path}")

# Load keys later
kem_keys = MLKEMKeyPair.load_keys('keys/spindex_kem')
dsa_keys = MLDSAKeyPair.load_keys('keys/spindex_dsa')

# Keys are ready to use
data = b"Test data"
signature = dsa_keys.sign(data)
```

---

## Integration with SPINDEX

### Use Cases

1. **Data at Rest Encryption**
   - Encrypt uploaded files before storage
   - Sign processed data for integrity verification
   - Secure key storage with proper permissions

2. **Data in Transit**
   - Sign API responses for authenticity
   - Encrypt sensitive query results
   - Protect inter-service communication

3. **User Authentication**
   - Sign authentication tokens
   - Encrypt session data
   - Secure password storage metadata

4. **Data Integrity**
   - Sign all spatial indexes
   - Verify data hasn't been tampered
   - Audit trail with signed logs

### Recommended Integration Points

```python
# backend/aegis_upload.py - Encrypt uploaded files
from backend.pqcrypto import HybridEncryption, MLKEMKeyPair

def encrypt_uploaded_file(file_path: str, recipient_public_key: bytes) -> dict:
    """Encrypt uploaded file with PQC"""
    with open(file_path, 'rb') as f:
        plaintext = f.read()

    return HybridEncryption.encrypt(
        plaintext=plaintext,
        recipient_public_key=recipient_public_key,
        associated_data=f"file:{file_path}".encode()
    )

# backend/aegis_worker.py - Sign processed data
from backend.pqcrypto import MLDSAKeyPair

def sign_processed_data(data: bytes, signer_keys: MLDSAKeyPair) -> bytes:
    """Sign processed data for integrity"""
    return signer_keys.sign(data)

# backend/main.py - Verify API requests
from backend.pqcrypto import MLDSAKeyPair

def verify_api_request(payload: bytes, signature: bytes, public_key: bytes) -> bool:
    """Verify signed API request"""
    return MLDSAKeyPair.verify(payload, signature, public_key)
```

---

## Security Considerations

### Key Management

1. **Generate Keys Once**
   ```bash
   python3 -c "from backend.pqcrypto import generate_keypairs; generate_keypairs('keys/prod')"
   ```

2. **Secure Storage**
   - Store private keys with `0o600` permissions (owner read/write only)
   - Use environment variables for key paths
   - Consider hardware security modules (HSM) for production

3. **Key Rotation**
   - Rotate keys periodically (e.g., annually)
   - Maintain old keys for decrypting archived data
   - Use key versioning in encrypted packages

### Associated Data (AAD)

Always use associated data with encryption to bind context:

```python
# Good: Context-specific AAD
encrypted = HybridEncryption.encrypt(
    plaintext=data,
    recipient_public_key=public_key,
    associated_data=b"user:alice|file:data.json|timestamp:2025-11-06"
)

# Bad: No AAD (allows ciphertext reuse attacks)
encrypted = HybridEncryption.encrypt(
    plaintext=data,
    recipient_public_key=public_key
)
```

### Error Handling

The module gracefully handles missing dependencies:

```python
try:
    from backend.pqcrypto import MLKEMKeyPair
    keys = MLKEMKeyPair()
except PQCryptoNotAvailableError as e:
    print(f"PQC not available: {e}")
    print("Install liboqs: sudo backend/install_liboqs.sh")
    # Fall back to classical crypto or disable feature
```

---

## Performance Characteristics

### Benchmarks (Approximate)

| Operation | Time (ms) | Notes |
|-----------|-----------|-------|
| ML-KEM KeyGen | ~0.1 | One-time cost |
| ML-KEM Encapsulate | ~0.1 | Per message |
| ML-KEM Decapsulate | ~0.1 | Per message |
| ML-DSA KeyGen | ~0.2 | One-time cost |
| ML-DSA Sign | ~1.5 | Per signature |
| ML-DSA Verify | ~0.5 | Per verification |
| AES-256-GCM Encrypt | ~0.001/KB | Bulk data |
| AES-256-GCM Decrypt | ~0.001/KB | Bulk data |

### Scalability

- **Small files (<1MB)**: Negligible overhead (~2ms)
- **Large files (>100MB)**: ~0.1ms/MB overhead
- **Bulk operations**: Use multiprocessing for parallel signing/verification

---

## Standards Compliance

### NIST FIPS Standards

- **FIPS 203 (ML-KEM)**: Module Lattice Key Encapsulation Mechanism
  - Based on Kyber algorithm (CRYSTALS-Kyber)
  - Standardized August 2024
  - Security reduction to Module-LWE problem

- **FIPS 204 (ML-DSA)**: Module Lattice Digital Signature Algorithm
  - Based on Dilithium algorithm (CRYSTALS-Dilithium)
  - Standardized August 2024
  - Security reduction to Module-LWE and Module-SIS problems

- **FIPS 197 (AES)**: Advanced Encryption Standard
  - 256-bit key size
  - GCM mode for authenticated encryption

### Security Level

**NIST Level 5** - Equivalent to:
- AES-256 against quantum computers
- SHA3-256 against quantum computers
- RSA-15360 against classical computers

---

## Testing

### Run Examples

```bash
# Test all PQC functionality
python3 backend/pqcrypto_example.py

# Expected output:
# ✓ ML-KEM-1024 key exchange works
# ✓ ML-DSA-87 signatures valid
# ✓ Hybrid encryption successful
# ✓ File encryption works
# ✓ Key management functional
```

### Unit Tests

```bash
# Run PQC-specific tests
pytest backend/tests/test_pqcrypto.py -v

# Run with coverage
pytest backend/tests/test_pqcrypto.py --cov=backend.pqcrypto --cov-report=html
```

---

## Troubleshooting

### Problem: `PQCryptoNotAvailableError`

**Cause**: liboqs C library not installed

**Solution**:
```bash
sudo backend/install_liboqs.sh
ldconfig -p | grep liboqs  # Verify installation
```

### Problem: `ModuleNotFoundError: No module named 'oqs'`

**Cause**: liboqs-python package not installed

**Solution**:
```bash
pip install liboqs-python
```

### Problem: Signature verification fails

**Cause**: Message or public key mismatch

**Solution**:
- Verify you're using the correct public key
- Ensure message bytes are identical (no encoding changes)
- Check signature hasn't been truncated or modified

### Problem: Decryption fails with "Authentication tag verification failed"

**Cause**: AAD mismatch, ciphertext tampering, or wrong keys

**Solution**:
- Use identical AAD for encryption and decryption
- Verify ciphertext hasn't been modified
- Ensure using correct recipient private key

---

## References

### Standards Documents

- [NIST FIPS 203 - ML-KEM](https://nvlpubs.nist.gov/nistpubs/FIPS/NIST.FIPS.203.pdf)
- [NIST FIPS 204 - ML-DSA](https://nvlpubs.nist.gov/nistpubs/FIPS/NIST.FIPS.204.pdf)
- [NIST FIPS 197 - AES](https://nvlpubs.nist.gov/nistpubs/FIPS/NIST.FIPS.197.pdf)

### Libraries

- [Open Quantum Safe liboqs](https://github.com/open-quantum-safe/liboqs)
- [liboqs-python bindings](https://github.com/open-quantum-safe/liboqs-python)
- [Python cryptography library](https://cryptography.io/)

### Additional Reading

- [NIST Post-Quantum Cryptography](https://csrc.nist.gov/projects/post-quantum-cryptography)
- [CRYSTALS Suite](https://pq-crystals.org/)
- [Hybrid PQC/Classical Key Exchange](https://datatracker.ietf.org/doc/draft-ietf-tls-hybrid-design/)

---

## Changelog

### v1.0 (2025-11-06)

- ✅ Initial implementation
- ✅ ML-KEM-1024 key encapsulation
- ✅ ML-DSA-87 digital signatures
- ✅ AES-256-GCM symmetric encryption
- ✅ Hybrid encryption combining ML-KEM + AES
- ✅ Key generation, save/load functions
- ✅ Installation script for liboqs
- ✅ Comprehensive examples and documentation
- ✅ Graceful degradation when liboqs unavailable

---

## License

SPINDEX Post-Quantum Cryptography implementation uses:
- **liboqs**: MIT License
- **Python cryptography**: Apache 2.0 / BSD

See individual library licenses for details.

---

**For questions or issues, refer to**:
- `backend/pqcrypto_example.py` - Working code examples
- `backend/install_liboqs.sh` - Installation instructions
- System logs for error details
