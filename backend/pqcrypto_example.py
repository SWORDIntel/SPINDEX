#!/usr/bin/env python3
"""
SPINDEX Post-Quantum Cryptography Usage Examples
=================================================

This script demonstrates how to use the SPINDEX PQC module for:
- ML-KEM-1024 key encapsulation
- ML-DSA-87 digital signatures
- Hybrid encryption with AES-256-GCM
"""

import json
from backend.pqcrypto import (
    MLKEMKeyPair,
    MLDSAKeyPair,
    HybridEncryption,
    get_pqc_status,
    PQCryptoNotAvailableError,
    generate_keypairs
)


def example_kem_key_exchange():
    """Example: ML-KEM-1024 Key Encapsulation"""
    print("\n" + "="*60)
    print("Example 1: ML-KEM-1024 Key Encapsulation")
    print("="*60)

    try:
        # Alice generates her key pair
        print("Alice: Generating ML-KEM-1024 key pair...")
        alice_keys = MLKEMKeyPair()
        alice_public = alice_keys.export_public_key()
        print(f"  Public key size: {len(alice_public)} bytes")

        # Bob encapsulates a secret using Alice's public key
        print("\nBob: Encapsulating shared secret...")
        bob_kem = MLKEMKeyPair()
        ciphertext, bob_secret = bob_kem.encapsulate(alice_public)
        print(f"  Ciphertext size: {len(ciphertext)} bytes")
        print(f"  Shared secret: {bob_secret.hex()[:64]}...")

        # Alice decapsulates to get the same secret
        print("\nAlice: Decapsulating shared secret...")
        alice_secret = alice_keys.decapsulate(ciphertext)
        print(f"  Shared secret: {alice_secret.hex()[:64]}...")

        # Verify secrets match
        if bob_secret == alice_secret:
            print("\n✓ Success! Shared secrets match")
            print(f"  Both parties now have: {len(alice_secret)} byte shared secret")
        else:
            print("\n✗ Error: Secrets don't match!")

    except PQCryptoNotAvailableError as e:
        print(f"\n✗ PQC not available: {e}")
        print("  Run: sudo backend/install_liboqs.sh")


def example_digital_signature():
    """Example: ML-DSA-87 Digital Signatures"""
    print("\n" + "="*60)
    print("Example 2: ML-DSA-87 Digital Signatures")
    print("="*60)

    try:
        # Generate signing key pair
        print("Generating ML-DSA-87 key pair...")
        signer_keys = MLDSAKeyPair()
        public_key = signer_keys.export_public_key()
        print(f"  Public key size: {len(public_key)} bytes")

        # Sign a message
        message = b"This is a critical SPINDEX data integrity guarantee"
        print(f"\nSigning message: '{message.decode()}'")
        signature = signer_keys.sign(message)
        print(f"  Signature size: {len(signature)} bytes")
        print(f"  Signature: {signature.hex()[:64]}...")

        # Verify signature
        print("\nVerifying signature...")
        is_valid = MLDSAKeyPair.verify(message, signature, public_key)

        if is_valid:
            print("✓ Signature is VALID")
        else:
            print("✗ Signature is INVALID")

        # Try to verify with wrong message
        print("\nTrying to verify with tampered message...")
        tampered = b"This is a TAMPERED message"
        is_valid_tampered = MLDSAKeyPair.verify(tampered, signature, public_key)

        if not is_valid_tampered:
            print("✓ Correctly rejected tampered message")
        else:
            print("✗ Error: Accepted tampered message!")

    except PQCryptoNotAvailableError as e:
        print(f"\n✗ PQC not available: {e}")
        print("  Run: sudo backend/install_liboqs.sh")


def example_hybrid_encryption():
    """Example: Hybrid ML-KEM-1024 + AES-256-GCM Encryption"""
    print("\n" + "="*60)
    print("Example 3: Hybrid Encryption (ML-KEM + AES-256-GCM)")
    print("="*60)

    try:
        # Recipient generates key pair
        print("Recipient: Generating ML-KEM-1024 key pair...")
        recipient_keys = MLKEMKeyPair()
        recipient_public = recipient_keys.export_public_key()

        # Sender encrypts data
        plaintext = b"Classified: SPINDEX quantum-resistant encrypted data payload"
        aad = b"metadata:classification=top-secret"

        print(f"\nSender: Encrypting data...")
        print(f"  Plaintext: '{plaintext.decode()}'")
        print(f"  AAD: '{aad.decode()}'")

        encrypted = HybridEncryption.encrypt(
            plaintext=plaintext,
            recipient_public_key=recipient_public,
            associated_data=aad
        )

        print(f"\n  Encrypted package:")
        print(f"    Algorithm: {encrypted['algorithm']}")
        print(f"    KEM ciphertext: {len(encrypted['kem_ciphertext'])} chars (base64)")
        print(f"    AES ciphertext: {len(encrypted['ciphertext'])} chars (base64)")
        print(f"    Nonce: {encrypted['nonce'][:32]}...")
        print(f"    Tag: {encrypted['tag'][:32]}...")

        # Recipient decrypts
        print(f"\nRecipient: Decrypting data...")
        decrypted = HybridEncryption.decrypt(
            encrypted_data=encrypted,
            kem_keypair=recipient_keys,
            associated_data=aad
        )

        print(f"  Decrypted: '{decrypted.decode()}'")

        if decrypted == plaintext:
            print("\n✓ Success! Encryption/Decryption works correctly")
        else:
            print("\n✗ Error: Decrypted data doesn't match original!")

        # Try with wrong AAD
        print("\nTrying to decrypt with wrong AAD...")
        try:
            HybridEncryption.decrypt(
                encrypted_data=encrypted,
                kem_keypair=recipient_keys,
                associated_data=b"wrong-metadata"
            )
            print("✗ Error: Accepted wrong AAD!")
        except Exception:
            print("✓ Correctly rejected wrong AAD (authentication failed)")

    except PQCryptoNotAvailableError as e:
        print(f"\n✗ PQC not available: {e}")
        print("  Run: sudo backend/install_liboqs.sh")


def example_file_encryption():
    """Example: Encrypting and Decrypting Files"""
    print("\n" + "="*60)
    print("Example 4: File Encryption")
    print("="*60)

    try:
        import os

        # Generate recipient keys
        print("Generating keys for file encryption...")
        recipient_keys = MLKEMKeyPair()

        # Create a test file
        test_file = "/tmp/spindex_test_secret.txt"
        test_content = b"SPINDEX SECRET DATA\n" * 100

        with open(test_file, 'wb') as f:
            f.write(test_content)

        print(f"Created test file: {test_file}")
        print(f"  Size: {len(test_content)} bytes")

        # Read and encrypt file
        print("\nEncrypting file...")
        with open(test_file, 'rb') as f:
            plaintext = f.read()

        encrypted = HybridEncryption.encrypt(
            plaintext=plaintext,
            recipient_public_key=recipient_keys.export_public_key(),
            associated_data=b"filename:spindex_test_secret.txt"
        )

        # Save encrypted data
        encrypted_file = "/tmp/spindex_test_secret.txt.enc"
        with open(encrypted_file, 'w') as f:
            json.dump(encrypted, f, indent=2)

        print(f"Encrypted file saved: {encrypted_file}")
        print(f"  Size: {os.path.getsize(encrypted_file)} bytes")

        # Read and decrypt
        print("\nDecrypting file...")
        with open(encrypted_file, 'r') as f:
            encrypted_data = json.load(f)

        decrypted = HybridEncryption.decrypt(
            encrypted_data=encrypted_data,
            kem_keypair=recipient_keys,
            associated_data=b"filename:spindex_test_secret.txt"
        )

        # Verify
        if decrypted == test_content:
            print("✓ File encryption/decryption successful!")
            print(f"  Verified {len(decrypted)} bytes")
        else:
            print("✗ Error: Decrypted content doesn't match!")

        # Cleanup
        os.remove(test_file)
        os.remove(encrypted_file)
        print(f"\nCleaned up test files")

    except PQCryptoNotAvailableError as e:
        print(f"\n✗ PQC not available: {e}")
        print("  Run: sudo backend/install_liboqs.sh")


def example_key_management():
    """Example: Saving and Loading Keys"""
    print("\n" + "="*60)
    print("Example 5: Key Management (Save/Load)")
    print("="*60)

    try:
        import os
        import tempfile

        # Create temporary directory for keys
        with tempfile.TemporaryDirectory() as tmpdir:
            key_prefix = os.path.join(tmpdir, "test_keys")

            # Generate and save all keys
            print("Generating and saving key pairs...")
            paths = generate_keypairs(key_prefix)

            print("\nGenerated keys:")
            for key_type, path in paths.items():
                size = os.path.getsize(path)
                perms = oct(os.stat(path).st_mode)[-3:]
                print(f"  {key_type}: {path}")
                print(f"    Size: {size} bytes, Permissions: {perms}")

            # Load and use KEM keys
            print("\nLoading ML-KEM keys...")
            loaded_kem = MLKEMKeyPair.load_keys(f"{key_prefix}_kem")
            print("  ✓ KEM keys loaded successfully")

            # Load and use DSA keys
            print("\nLoading ML-DSA keys...")
            loaded_dsa = MLDSAKeyPair.load_keys(f"{key_prefix}_dsa")
            print("  ✓ DSA keys loaded successfully")

            # Test loaded keys work
            print("\nTesting loaded keys...")
            test_msg = b"Test message for loaded keys"
            signature = loaded_dsa.sign(test_msg)
            is_valid = MLDSAKeyPair.verify(
                test_msg,
                signature,
                loaded_dsa.export_public_key()
            )

            if is_valid:
                print("  ✓ Loaded keys work correctly")
            else:
                print("  ✗ Error: Loaded keys don't work!")

            print("\nKeys are automatically cleaned up from temporary directory")

    except PQCryptoNotAvailableError as e:
        print(f"\n✗ PQC not available: {e}")
        print("  Run: sudo backend/install_liboqs.sh")


def main():
    """Run all examples"""
    print("\n" + "="*60)
    print("SPINDEX POST-QUANTUM CRYPTOGRAPHY EXAMPLES")
    print("="*60)

    # Check PQC status
    status = get_pqc_status()
    print("\nPQC Status:")
    print(f"  liboqs available: {status['liboqs_available']}")
    print(f"  ML-KEM algorithm: {status['ml_kem_algorithm']}")
    print(f"  ML-DSA algorithm: {status['ml_dsa_algorithm']}")
    print(f"  AES-256-GCM available: {status['aes_gcm_available']}")
    print(f"  Security level: {status['security_level']}")

    if not status['liboqs_available']:
        print("\n" + "⚠"*30)
        print("WARNING: liboqs is not installed!")
        print("Examples will show errors. To install:")
        print("  sudo backend/install_liboqs.sh")
        print("⚠"*30)

    # Run examples
    example_kem_key_exchange()
    example_digital_signature()
    example_hybrid_encryption()
    example_file_encryption()
    example_key_management()

    print("\n" + "="*60)
    print("Examples completed!")
    print("="*60)
    print("\nFor production use:")
    print("  1. Generate persistent keys: generate_keypairs('keys/spindex')")
    print("  2. Secure key storage with proper file permissions (0o600)")
    print("  3. Use AAD for context binding in encryption")
    print("  4. Sign all critical data with ML-DSA-87")
    print("  5. Use hybrid encryption for all sensitive data")
    print("="*60 + "\n")


if __name__ == "__main__":
    main()
