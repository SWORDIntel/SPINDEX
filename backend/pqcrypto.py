"""
SPINDEX Post-Quantum Cryptography Module
=========================================

This module implements post-quantum safe encryption and signatures using:
- ML-KEM-1024 (Module-Lattice-Based Key-Encapsulation Mechanism) for key exchange
- ML-DSA-87 (Module-Lattice-Based Digital Signature Algorithm) for signatures
- AES-256-GCM for symmetric encryption

Requirements:
    - cryptography>=41.0.0 (for AES-256-GCM)
    - liboqs-python>=0.14.1 (for ML-KEM and ML-DSA)
    - liboqs C library must be built and installed

Installation of liboqs:
    ```bash
    # Install dependencies
    apt-get install -y cmake gcc ninja-build libssl-dev python3-pytest python3-pytest-xdist unzip xsltproc doxygen graphviz

    # Build and install liboqs
    git clone -b main https://github.com/open-quantum-safe/liboqs.git
    cd liboqs
    mkdir build && cd build
    cmake -GNinja -DCMAKE_INSTALL_PREFIX=/usr/local ..
    ninja
    ninja install
    ldconfig
    ```

NIST Post-Quantum Cryptography Standards (FIPS 203, 204):
    - ML-KEM-1024: FIPS 203 (formerly Kyber-1024) - 256-bit security level
    - ML-DSA-87: FIPS 204 (formerly Dilithium5) - ~256-bit security level
    - AES-256-GCM: FIPS 197 + SP 800-38D - 256-bit security level

Security Level: 256-bit post-quantum security
"""

import os
import base64
import json
from typing import Tuple, Dict, Any, Optional
from cryptography.hazmat.primitives.ciphers.aead import AESGCM
from cryptography.hazmat.primitives import hashes
from cryptography.hazmat.primitives.kdf.hkdf import HKDF

# Try to import liboqs for PQC algorithms
try:
    import oqs
    LIBOQS_AVAILABLE = True

    # Verify ML-KEM-1024 is available
    if 'Kyber1024' in oqs.get_enabled_KEM_mechanisms():
        ML_KEM_ALGORITHM = 'Kyber1024'
    elif 'ML-KEM-1024' in oqs.get_enabled_KEM_mechanisms():
        ML_KEM_ALGORITHM = 'ML-KEM-1024'
    else:
        LIBOQS_AVAILABLE = False
        ML_KEM_ALGORITHM = None

    # Verify ML-DSA-87 is available
    if 'Dilithium5' in oqs.get_enabled_sig_mechanisms():
        ML_DSA_ALGORITHM = 'Dilithium5'
    elif 'ML-DSA-87' in oqs.get_enabled_sig_mechanisms():
        ML_DSA_ALGORITHM = 'ML-DSA-87'
    else:
        LIBOQS_AVAILABLE = False
        ML_DSA_ALGORITHM = None

except ImportError:
    LIBOQS_AVAILABLE = False
    ML_KEM_ALGORITHM = None
    ML_DSA_ALGORITHM = None


class PQCryptoError(Exception):
    """Base exception for post-quantum cryptography errors."""
    pass


class PQCryptoNotAvailableError(PQCryptoError):
    """Raised when PQC libraries are not properly installed."""
    pass


class MLKEMKeyPair:
    """
    ML-KEM-1024 (Kyber-1024) Key Encapsulation Mechanism.

    This provides post-quantum secure key exchange using lattice-based cryptography.
    Security level: NIST Level 5 (~256-bit classical, ~256-bit quantum)
    """

    def __init__(self):
        """Generate a new ML-KEM-1024 key pair."""
        if not LIBOQS_AVAILABLE:
            raise PQCryptoNotAvailableError(
                "liboqs is not available. Please install liboqs C library and liboqs-python. "
                "See module docstring for installation instructions."
            )

        self.kem = oqs.KeyEncapsulation(ML_KEM_ALGORITHM)
        self.public_key = self.kem.generate_keypair()
        self._private_key_initialized = True

    def export_public_key(self) -> bytes:
        """Export the public key for sharing."""
        return self.public_key

    def export_public_key_b64(self) -> str:
        """Export the public key as base64 string."""
        return base64.b64encode(self.public_key).decode('utf-8')

    def encapsulate(self, recipient_public_key: bytes) -> Tuple[bytes, bytes]:
        """
        Encapsulate a shared secret using recipient's public key.

        Args:
            recipient_public_key: The recipient's ML-KEM public key

        Returns:
            Tuple of (ciphertext, shared_secret)
            - ciphertext: To be sent to recipient for decapsulation
            - shared_secret: 32-byte shared secret for AES-256-GCM
        """
        kem_encap = oqs.KeyEncapsulation(ML_KEM_ALGORITHM)
        ciphertext, shared_secret = kem_encap.encap_secret(recipient_public_key)
        return ciphertext, shared_secret[:32]  # Use first 32 bytes for AES-256

    def decapsulate(self, ciphertext: bytes) -> bytes:
        """
        Decapsulate a shared secret from ciphertext.

        Args:
            ciphertext: The encapsulated secret from sender

        Returns:
            32-byte shared secret for AES-256-GCM
        """
        shared_secret = self.kem.decap_secret(ciphertext)
        return shared_secret[:32]  # Use first 32 bytes for AES-256

    def save_keys(self, filepath_prefix: str):
        """
        Save key pair to files.

        Creates:
            {filepath_prefix}_public.key
            {filepath_prefix}_secret.key (permissions: 0o600)
        """
        # Save public key
        with open(f"{filepath_prefix}_public.key", 'wb') as f:
            f.write(self.public_key)

        # Save secret key with restricted permissions
        secret_path = f"{filepath_prefix}_secret.key"
        with open(secret_path, 'wb') as f:
            f.write(self.kem.export_secret_key())
        os.chmod(secret_path, 0o600)

    @classmethod
    def load_keys(cls, filepath_prefix: str) -> 'MLKEMKeyPair':
        """Load key pair from files."""
        if not LIBOQS_AVAILABLE:
            raise PQCryptoNotAvailableError("liboqs is not available")

        with open(f"{filepath_prefix}_public.key", 'rb') as f:
            public_key = f.read()

        with open(f"{filepath_prefix}_secret.key", 'rb') as f:
            secret_key = f.read()

        instance = cls.__new__(cls)
        instance.kem = oqs.KeyEncapsulation(ML_KEM_ALGORITHM, secret_key)
        instance.public_key = public_key
        instance._private_key_initialized = True
        return instance


class MLDSAKeyPair:
    """
    ML-DSA-87 (Dilithium5) Digital Signature Algorithm.

    This provides post-quantum secure digital signatures using lattice-based cryptography.
    Security level: NIST Level 5 (~256-bit classical, ~256-bit quantum)
    """

    def __init__(self):
        """Generate a new ML-DSA-87 key pair."""
        if not LIBOQS_AVAILABLE:
            raise PQCryptoNotAvailableError(
                "liboqs is not available. Please install liboqs C library and liboqs-python. "
                "See module docstring for installation instructions."
            )

        self.signer = oqs.Signature(ML_DSA_ALGORITHM)
        self.public_key = self.signer.generate_keypair()
        self._private_key_initialized = True

    def export_public_key(self) -> bytes:
        """Export the public key for sharing."""
        return self.public_key

    def export_public_key_b64(self) -> str:
        """Export the public key as base64 string."""
        return base64.b64encode(self.public_key).decode('utf-8')

    def sign(self, message: bytes) -> bytes:
        """
        Sign a message.

        Args:
            message: The data to sign

        Returns:
            Digital signature
        """
        return self.signer.sign(message)

    def sign_b64(self, message: bytes) -> str:
        """Sign a message and return base64 encoded signature."""
        signature = self.sign(message)
        return base64.b64encode(signature).decode('utf-8')

    @staticmethod
    def verify(message: bytes, signature: bytes, public_key: bytes) -> bool:
        """
        Verify a signature.

        Args:
            message: The original data
            signature: The signature to verify
            public_key: The signer's public key

        Returns:
            True if signature is valid, False otherwise
        """
        if not LIBOQS_AVAILABLE:
            raise PQCryptoNotAvailableError("liboqs is not available")

        verifier = oqs.Signature(ML_DSA_ALGORITHM)
        return verifier.verify(message, signature, public_key)

    @staticmethod
    def verify_b64(message: bytes, signature_b64: str, public_key_b64: str) -> bool:
        """Verify a base64 encoded signature."""
        signature = base64.b64decode(signature_b64)
        public_key = base64.b64decode(public_key_b64)
        return MLDSAKeyPair.verify(message, signature, public_key)

    def save_keys(self, filepath_prefix: str):
        """
        Save key pair to files.

        Creates:
            {filepath_prefix}_public.key
            {filepath_prefix}_secret.key (permissions: 0o600)
        """
        # Save public key
        with open(f"{filepath_prefix}_public.key", 'wb') as f:
            f.write(self.public_key)

        # Save secret key with restricted permissions
        secret_path = f"{filepath_prefix}_secret.key"
        with open(secret_path, 'wb') as f:
            f.write(self.signer.export_secret_key())
        os.chmod(secret_path, 0o600)

    @classmethod
    def load_keys(cls, filepath_prefix: str) -> 'MLDSAKeyPair':
        """Load key pair from files."""
        if not LIBOQS_AVAILABLE:
            raise PQCryptoNotAvailableError("liboqs is not available")

        with open(f"{filepath_prefix}_public.key", 'rb') as f:
            public_key = f.read()

        with open(f"{filepath_prefix}_secret.key", 'rb') as f:
            secret_key = f.read()

        instance = cls.__new__(cls)
        instance.signer = oqs.Signature(ML_DSA_ALGORITHM, secret_key)
        instance.public_key = public_key
        instance._private_key_initialized = True
        return instance


class HybridEncryption:
    """
    Hybrid post-quantum encryption combining ML-KEM-1024 and AES-256-GCM.

    Process:
        1. Generate ephemeral ML-KEM-1024 key pair
        2. Use recipient's public key to encapsulate a shared secret (KEM)
        3. Derive AES-256-GCM key from shared secret using HKDF
        4. Encrypt data with AES-256-GCM
        5. Package: KEM ciphertext + AES nonce + AES ciphertext + AES tag

    This provides both post-quantum security (ML-KEM) and efficient bulk encryption (AES-GCM).
    """

    @staticmethod
    def encrypt(
        plaintext: bytes,
        recipient_public_key: bytes,
        associated_data: Optional[bytes] = None
    ) -> Dict[str, str]:
        """
        Encrypt data for a recipient using hybrid PQC.

        Args:
            plaintext: Data to encrypt
            recipient_public_key: Recipient's ML-KEM-1024 public key
            associated_data: Optional authenticated associated data (AAD)

        Returns:
            Dictionary containing:
                - kem_ciphertext: Encapsulated key (base64)
                - nonce: AES-GCM nonce (base64)
                - ciphertext: Encrypted data (base64)
                - tag: AES-GCM authentication tag (base64)
        """
        if not LIBOQS_AVAILABLE:
            raise PQCryptoNotAvailableError("liboqs is not available")

        # Step 1: Generate ephemeral KEM key pair and encapsulate
        kem_encap = oqs.KeyEncapsulation(ML_KEM_ALGORITHM)
        kem_ciphertext, shared_secret = kem_encap.encap_secret(recipient_public_key)

        # Step 2: Derive AES-256 key from shared secret using HKDF
        hkdf = HKDF(
            algorithm=hashes.SHA3_256(),
            length=32,
            salt=None,
            info=b'SPINDEX-PQC-AES-256-GCM'
        )
        aes_key = hkdf.derive(shared_secret[:32])

        # Step 3: Encrypt with AES-256-GCM
        aesgcm = AESGCM(aes_key)
        nonce = os.urandom(12)  # 96-bit nonce for GCM
        ciphertext = aesgcm.encrypt(nonce, plaintext, associated_data)

        # GCM ciphertext includes the tag at the end
        # Split ciphertext and tag (last 16 bytes)
        encrypted_data = ciphertext[:-16]
        tag = ciphertext[-16:]

        return {
            'kem_ciphertext': base64.b64encode(kem_ciphertext).decode('utf-8'),
            'nonce': base64.b64encode(nonce).decode('utf-8'),
            'ciphertext': base64.b64encode(encrypted_data).decode('utf-8'),
            'tag': base64.b64encode(tag).decode('utf-8'),
            'algorithm': 'ML-KEM-1024+AES-256-GCM'
        }

    @staticmethod
    def decrypt(
        encrypted_data: Dict[str, str],
        kem_keypair: MLKEMKeyPair,
        associated_data: Optional[bytes] = None
    ) -> bytes:
        """
        Decrypt data using hybrid PQC.

        Args:
            encrypted_data: Dictionary from encrypt() containing all components
            kem_keypair: Recipient's ML-KEM-1024 key pair
            associated_data: Optional AAD (must match encryption)

        Returns:
            Decrypted plaintext

        Raises:
            PQCryptoError: If decryption fails (wrong key, corrupted data, etc.)
        """
        if not LIBOQS_AVAILABLE:
            raise PQCryptoNotAvailableError("liboqs is not available")

        try:
            # Step 1: Decode components
            kem_ciphertext = base64.b64decode(encrypted_data['kem_ciphertext'])
            nonce = base64.b64decode(encrypted_data['nonce'])
            ciphertext = base64.b64decode(encrypted_data['ciphertext'])
            tag = base64.b64decode(encrypted_data['tag'])

            # Step 2: Decapsulate shared secret
            shared_secret = kem_keypair.decapsulate(kem_ciphertext)

            # Step 3: Derive AES-256 key
            hkdf = HKDF(
                algorithm=hashes.SHA3_256(),
                length=32,
                salt=None,
                info=b'SPINDEX-PQC-AES-256-GCM'
            )
            aes_key = hkdf.derive(shared_secret)

            # Step 4: Decrypt with AES-256-GCM
            aesgcm = AESGCM(aes_key)
            # Reconstruct full ciphertext with tag
            full_ciphertext = ciphertext + tag
            plaintext = aesgcm.decrypt(nonce, full_ciphertext, associated_data)

            return plaintext

        except Exception as e:
            raise PQCryptoError(f"Decryption failed: {str(e)}")


def generate_keypairs(base_path: str = "keys/spindex") -> Dict[str, str]:
    """
    Generate and save both ML-KEM and ML-DSA key pairs.

    Args:
        base_path: Base path for key files (without extension)

    Returns:
        Dictionary with paths to generated key files

    Creates files:
        {base_path}_kem_public.key
        {base_path}_kem_secret.key
        {base_path}_dsa_public.key
        {base_path}_dsa_secret.key
    """
    if not LIBOQS_AVAILABLE:
        raise PQCryptoNotAvailableError(
            "liboqs is not available. Cannot generate keys.\n"
            "Please install liboqs C library and liboqs-python."
        )

    # Create directory if needed
    os.makedirs(os.path.dirname(base_path) if os.path.dirname(base_path) else '.', exist_ok=True)

    # Generate ML-KEM-1024 key pair
    kem_keys = MLKEMKeyPair()
    kem_keys.save_keys(f"{base_path}_kem")

    # Generate ML-DSA-87 key pair
    dsa_keys = MLDSAKeyPair()
    dsa_keys.save_keys(f"{base_path}_dsa")

    return {
        'kem_public': f"{base_path}_kem_public.key",
        'kem_secret': f"{base_path}_kem_secret.key",
        'dsa_public': f"{base_path}_dsa_public.key",
        'dsa_secret': f"{base_path}_dsa_secret.key"
    }


def get_pqc_status() -> Dict[str, Any]:
    """
    Get the current status of PQC implementation.

    Returns:
        Dictionary with availability and algorithm information
    """
    status = {
        'liboqs_available': LIBOQS_AVAILABLE,
        'ml_kem_algorithm': ML_KEM_ALGORITHM,
        'ml_dsa_algorithm': ML_DSA_ALGORITHM,
        'aes_gcm_available': True,  # Always available via cryptography
        'security_level': '256-bit post-quantum' if LIBOQS_AVAILABLE else 'N/A'
    }

    if LIBOQS_AVAILABLE:
        status['available_kems'] = [k for k in oqs.get_enabled_KEM_mechanisms() if 'Kyber' in k or 'ML-KEM' in k]
        status['available_signatures'] = [s for s in oqs.get_enabled_sig_mechanisms() if 'Dilithium' in s or 'ML-DSA' in s]

    return status


# Module-level convenience functions
def encrypt_data(plaintext: bytes, recipient_public_key: bytes, aad: Optional[bytes] = None) -> Dict[str, str]:
    """Convenience function for hybrid encryption."""
    return HybridEncryption.encrypt(plaintext, recipient_public_key, aad)


def decrypt_data(encrypted_data: Dict[str, str], kem_keypair: MLKEMKeyPair, aad: Optional[bytes] = None) -> bytes:
    """Convenience function for hybrid decryption."""
    return HybridEncryption.decrypt(encrypted_data, kem_keypair, aad)


if __name__ == "__main__":
    # Display PQC status
    print("SPINDEX Post-Quantum Cryptography Status")
    print("=" * 50)
    status = get_pqc_status()
    for key, value in status.items():
        print(f"{key}: {value}")
