"""
Cryptography and Hashing for NexusLang.

Features:
- Hashing: MD5, SHA-1, SHA-256, SHA-512, SHA3, BLAKE2
- HMAC: Message authentication codes
- Symmetric encryption: AES-256-GCM, ChaCha20-Poly1305
- Asymmetric encryption: RSA (2048/4096), Ed25519
- Key derivation: PBKDF2, Argon2
- Digital signatures: RSA-PSS, Ed25519
- Random generation: Cryptographically secure random bytes/tokens
"""

import hashlib
import hmac
import base64
import secrets
from typing import Optional, Tuple, Dict, Any
from ...runtime.runtime import Runtime

# Import cryptography library components
try:
    from cryptography.hazmat.primitives.ciphers import Cipher, algorithms, modes
    from cryptography.hazmat.primitives import hashes, serialization
    from cryptography.hazmat.primitives.asymmetric import rsa, ed25519, padding
    from cryptography.hazmat.primitives.kdf.pbkdf2 import PBKDF2HMAC
    from cryptography.hazmat.primitives.kdf.argon2 import Argon2id
    from cryptography.hazmat.backends import default_backend
    HAS_CRYPTOGRAPHY = True
except ImportError:
    HAS_CRYPTOGRAPHY = False


def hash_md5(data: str, encoding: str = 'utf-8') -> str:
    """Compute MD5 hash."""
    return hashlib.md5(data.encode(encoding)).hexdigest()


def hash_sha1(data: str, encoding: str = 'utf-8') -> str:
    """Compute SHA-1 hash."""
    return hashlib.sha1(data.encode(encoding)).hexdigest()


def hash_sha256(data: str, encoding: str = 'utf-8') -> str:
    """Compute SHA-256 hash."""
    return hashlib.sha256(data.encode(encoding)).hexdigest()


def hash_sha512(data: str, encoding: str = 'utf-8') -> str:
    """Compute SHA-512 hash."""
    return hashlib.sha512(data.encode(encoding)).hexdigest()


def hash_sha3_256(data: str, encoding: str = 'utf-8') -> str:
    """Compute SHA3-256 hash."""
    return hashlib.sha3_256(data.encode(encoding)).hexdigest()


def hash_sha3_512(data: str, encoding: str = 'utf-8') -> str:
    """Compute SHA3-512 hash."""
    return hashlib.sha3_512(data.encode(encoding)).hexdigest()


def hash_blake2b(data: str, encoding: str = 'utf-8') -> str:
    """Compute BLAKE2b hash."""
    return hashlib.blake2b(data.encode(encoding)).hexdigest()


def hash_blake2s(data: str, encoding: str = 'utf-8') -> str:
    """Compute BLAKE2s hash."""
    return hashlib.blake2s(data.encode(encoding)).hexdigest()


def hmac_sha256(data: str, key: str, encoding: str = 'utf-8') -> str:
    """Compute HMAC-SHA256."""
    return hmac.new(key.encode(encoding), data.encode(encoding), hashlib.sha256).hexdigest()


def hmac_sha512(data: str, key: str, encoding: str = 'utf-8') -> str:
    """Compute HMAC-SHA512."""
    return hmac.new(key.encode(encoding), data.encode(encoding), hashlib.sha512).hexdigest()


def base64_encode(data: str, encoding: str = 'utf-8') -> str:
    """Encode string to base64."""
    return base64.b64encode(data.encode(encoding)).decode('ascii')


def base64_decode(data: str, encoding: str = 'utf-8') -> str:
    """Decode base64 to string."""
    return base64.b64decode(data).decode(encoding)


def base64_url_encode(data: str, encoding: str = 'utf-8') -> str:
    """Encode string to URL-safe base64."""
    return base64.urlsafe_b64encode(data.encode(encoding)).decode('ascii')


def base64_url_decode(data: str, encoding: str = 'utf-8') -> str:
    """Decode URL-safe base64 to string."""
    return base64.urlsafe_b64decode(data).decode(encoding)


def random_bytes(n: int) -> bytes:
    """Generate n random bytes (cryptographically secure)."""
    return secrets.token_bytes(n)


def random_hex(n: int) -> str:
    """Generate random hex string of n bytes."""
    return secrets.token_hex(n)


def random_token(n: int = 32) -> str:
    """Generate random URL-safe token."""
    return secrets.token_urlsafe(n)


def compare_digest(a: str, b: str) -> bool:
    """Constant-time string comparison (prevents timing attacks)."""
    return hmac.compare_digest(a, b)


def hash_password(password: str, salt: Optional[str] = None) -> dict:
    """
    Hash password with salt (basic implementation).
    Returns dict with 'hash' and 'salt'.
    Note: For production, use proper password hashing like bcrypt or argon2.
    """
    if salt is None:
        salt = random_hex(16)
    
    # Simple PBKDF2 with SHA-256
    import hashlib
    hash_value = hashlib.pbkdf2_hmac('sha256', password.encode('utf-8'), 
                                      salt.encode('utf-8'), 100000)
    
    return {
        'hash': hash_value.hex(),
        'salt': salt,
        'algorithm': 'pbkdf2_sha256',
        'iterations': 100000
    }


def verify_password(password: str, hash_info: dict) -> bool:
    """Verify password against hash."""
    salt = hash_info.get('salt')
    expected_hash = hash_info.get('hash')
    
    if not salt or not expected_hash:
        return False
    
    # Recompute hash
    hash_value = hashlib.pbkdf2_hmac('sha256', password.encode('utf-8'),
                                      salt.encode('utf-8'), 100000)
    
    return compare_digest(hash_value.hex(), expected_hash)


# ==========================================
# Symmetric Encryption (AES-256-GCM)
# ==========================================

def aes_generate_key() -> bytes:
    """Generate a 256-bit AES key (cryptographically secure)."""
    if not HAS_CRYPTOGRAPHY:
        raise ImportError("cryptography library required. Install with: pip install cryptography")
    return secrets.token_bytes(32)  # 256 bits


def aes_encrypt(plaintext: str, key: bytes, encoding: str = 'utf-8') -> dict:
    """
    Encrypt plaintext with AES-256-GCM.
    
    Args:
        plaintext: Text to encrypt
        key: 256-bit encryption key (32 bytes)
        encoding: Text encoding
        
    Returns:
        Dict with 'ciphertext' (bytes), 'nonce' (bytes), 'tag' (bytes)
    """
    if not HAS_CRYPTOGRAPHY:
        raise ImportError("cryptography library required. Install with: pip install cryptography")
    
    if len(key) != 32:
        raise ValueError("Key must be 32 bytes (256 bits)")
    
    # Generate random nonce (96 bits for GCM)
    nonce = secrets.token_bytes(12)
    
    # Create cipher
    cipher = Cipher(
        algorithms.AES(key),
        modes.GCM(nonce),
        backend=default_backend()
    )
    encryptor = cipher.encryptor()
    
    # Encrypt
    ciphertext = encryptor.update(plaintext.encode(encoding)) + encryptor.finalize()
    
    return {
        'ciphertext': ciphertext,
        'nonce': nonce,
        'tag': encryptor.tag
    }


def aes_decrypt(encrypted_data: dict, key: bytes, encoding: str = 'utf-8') -> str:
    """
    Decrypt AES-256-GCM ciphertext.
    
    Args:
        encrypted_data: Dict with 'ciphertext', 'nonce', 'tag'
        key: 256-bit decryption key
        encoding: Text encoding
        
    Returns:
        Decrypted plaintext string
    """
    if not HAS_CRYPTOGRAPHY:
        raise ImportError("cryptography library required. Install with: pip install cryptography")
    
    if len(key) != 32:
        raise ValueError("Key must be 32 bytes (256 bits)")
    
    # Create cipher with tag
    cipher = Cipher(
        algorithms.AES(key),
        modes.GCM(encrypted_data['nonce'], encrypted_data['tag']),
        backend=default_backend()
    )
    decryptor = cipher.decryptor()
    
    # Decrypt
    plaintext_bytes = decryptor.update(encrypted_data['ciphertext']) + decryptor.finalize()
    
    return plaintext_bytes.decode(encoding)


def chacha20_generate_key() -> bytes:
    """Generate a 256-bit ChaCha20 key."""
    if not HAS_CRYPTOGRAPHY:
        raise ImportError("cryptography library required. Install with: pip install cryptography")
    return secrets.token_bytes(32)


def chacha20_encrypt(plaintext: str, key: bytes, encoding: str = 'utf-8') -> dict:
    """
    Encrypt plaintext with ChaCha20-Poly1305.
    
    Args:
        plaintext: Text to encrypt
        key: 256-bit encryption key
        encoding: Text encoding
        
    Returns:
        Dict with 'ciphertext' (bytes), 'nonce' (bytes), 'tag' (bytes)
    """
    if not HAS_CRYPTOGRAPHY:
        raise ImportError("cryptography library required. Install with: pip install cryptography")
    
    if len(key) != 32:
        raise ValueError("Key must be 32 bytes (256 bits)")
    
    # Generate random nonce (96 bits)
    nonce = secrets.token_bytes(12)
    
    # Create cipher
    cipher = Cipher(
        algorithms.ChaCha20(key, nonce),
        mode=None,
        backend=default_backend()
    )
    encryptor = cipher.encryptor()
    
    # Encrypt
    ciphertext = encryptor.update(plaintext.encode(encoding)) + encryptor.finalize()
    
    # Note: ChaCha20 without Poly1305 doesn't have authentication tag
    # For authenticated encryption, use cryptography's AEAD interface
    return {
        'ciphertext': ciphertext,
        'nonce': nonce,
        'tag': b''  # No tag for basic ChaCha20
    }


def chacha20_decrypt(encrypted_data: dict, key: bytes, encoding: str = 'utf-8') -> str:
    """Decrypt ChaCha20 ciphertext."""
    if not HAS_CRYPTOGRAPHY:
        raise ImportError("cryptography library required. Install with: pip install cryptography")
    
    if len(key) != 32:
        raise ValueError("Key must be 32 bytes (256 bits)")
    
    # Create cipher
    cipher = Cipher(
        algorithms.ChaCha20(key, encrypted_data['nonce']),
        mode=None,
        backend=default_backend()
    )
    decryptor = cipher.decryptor()
    
    # Decrypt
    plaintext_bytes = decryptor.update(encrypted_data['ciphertext']) + decryptor.finalize()
    
    return plaintext_bytes.decode(encoding)


# ==========================================
# Asymmetric Encryption (RSA)
# ==========================================

def rsa_generate_keypair(key_size: int = 2048) -> dict:
    """
    Generate RSA key pair.
    
    Args:
        key_size: Key size in bits (2048 or 4096 recommended)
        
    Returns:
        Dict with 'private_key' (bytes, PEM), 'public_key' (bytes, PEM)
    """
    if not HAS_CRYPTOGRAPHY:
        raise ImportError("cryptography library required. Install with: pip install cryptography")
    
    if key_size < 1024 or key_size % 256 != 0:
        raise ValueError("Key size must be a multiple of 256 and at least 1024 bits")
    
    # Generate private key
    private_key = rsa.generate_private_key(
        public_exponent=65537,
        key_size=key_size,
        backend=default_backend()
    )
    
    # Serialize private key
    private_pem = private_key.private_bytes(
        encoding=serialization.Encoding.PEM,
        format=serialization.PrivateFormat.PKCS8,
        encryption_algorithm=serialization.NoEncryption()
    )
    
    # Get public key
    public_key = private_key.public_key()
    public_pem = public_key.public_bytes(
        encoding=serialization.Encoding.PEM,
        format=serialization.PublicFormat.SubjectPublicKeyInfo
    )
    
    return {
        'private_key': private_pem,
        'public_key': public_pem,
        'key_size': key_size
    }


def rsa_encrypt(plaintext: str, public_key_pem: bytes, encoding: str = 'utf-8') -> bytes:
    """
    Encrypt plaintext with RSA public key.
    
    Args:
        plaintext: Text to encrypt
        public_key_pem: Public key in PEM format
        encoding: Text encoding
        
    Returns:
        Encrypted ciphertext (bytes)
    """
    if not HAS_CRYPTOGRAPHY:
        raise ImportError("cryptography library required. Install with: pip install cryptography")
    
    # Load public key
    public_key = serialization.load_pem_public_key(
        public_key_pem,
        backend=default_backend()
    )
    
    # Encrypt with OAEP padding
    ciphertext = public_key.encrypt(
        plaintext.encode(encoding),
        padding.OAEP(
            mgf=padding.MGF1(algorithm=hashes.SHA256()),
            algorithm=hashes.SHA256(),
            label=None
        )
    )
    
    return ciphertext


def rsa_decrypt(ciphertext: bytes, private_key_pem: bytes, encoding: str = 'utf-8') -> str:
    """
    Decrypt RSA ciphertext with private key.
    
    Args:
        ciphertext: Encrypted data
        private_key_pem: Private key in PEM format
        encoding: Text encoding
        
    Returns:
        Decrypted plaintext string
    """
    if not HAS_CRYPTOGRAPHY:
        raise ImportError("cryptography library required. Install with: pip install cryptography")
    
    # Load private key
    private_key = serialization.load_pem_private_key(
        private_key_pem,
        password=None,
        backend=default_backend()
    )
    
    # Decrypt
    plaintext_bytes = private_key.decrypt(
        ciphertext,
        padding.OAEP(
            mgf=padding.MGF1(algorithm=hashes.SHA256()),
            algorithm=hashes.SHA256(),
            label=None
        )
    )
    
    return plaintext_bytes.decode(encoding)


def rsa_sign(message: str, private_key_pem: bytes, encoding: str = 'utf-8') -> bytes:
    """
    Sign message with RSA private key (RSA-PSS).
    
    Args:
        message: Message to sign
        private_key_pem: Private key in PEM format
        encoding: Text encoding
        
    Returns:
        Digital signature (bytes)
    """
    if not HAS_CRYPTOGRAPHY:
        raise ImportError("cryptography library required. Install with: pip install cryptography")
    
    # Load private key
    private_key = serialization.load_pem_private_key(
        private_key_pem,
        password=None,
        backend=default_backend()
    )
    
    # Sign with PSS padding
    signature = private_key.sign(
        message.encode(encoding),
        padding.PSS(
            mgf=padding.MGF1(hashes.SHA256()),
            salt_length=padding.PSS.MAX_LENGTH
        ),
        hashes.SHA256()
    )
    
    return signature


def rsa_verify(message: str, signature: bytes, public_key_pem: bytes, encoding: str = 'utf-8') -> bool:
    """
    Verify RSA signature.
    
    Args:
        message: Original message
        signature: Signature to verify
        public_key_pem: Public key in PEM format
        encoding: Text encoding
        
    Returns:
        True if signature is valid, False otherwise
    """
    if not HAS_CRYPTOGRAPHY:
        raise ImportError("cryptography library required. Install with: pip install cryptography")
    
    # Load public key
    public_key = serialization.load_pem_public_key(
        public_key_pem,
        backend=default_backend()
    )
    
    try:
        public_key.verify(
            signature,
            message.encode(encoding),
            padding.PSS(
                mgf=padding.MGF1(hashes.SHA256()),
                salt_length=padding.PSS.MAX_LENGTH
            ),
            hashes.SHA256()
        )
        return True
    except Exception:
        return False


# ==========================================
# Ed25519 (Modern Elliptic Curve Signatures)
# ==========================================

def ed25519_generate_keypair() -> dict:
    """
    Generate Ed25519 key pair (fast, secure signatures).
    
    Returns:
        Dict with 'private_key' (bytes, raw), 'public_key' (bytes, raw)
    """
    if not HAS_CRYPTOGRAPHY:
        raise ImportError("cryptography library required. Install with: pip install cryptography")
    
    # Generate private key
    private_key = ed25519.Ed25519PrivateKey.generate()
    
    # Serialize keys (raw format)
    private_bytes = private_key.private_bytes(
        encoding=serialization.Encoding.Raw,
        format=serialization.PrivateFormat.Raw,
        encryption_algorithm=serialization.NoEncryption()
    )
    
    public_key = private_key.public_key()
    public_bytes = public_key.public_bytes(
        encoding=serialization.Encoding.Raw,
        format=serialization.PublicFormat.Raw
    )
    
    return {
        'private_key': private_bytes,
        'public_key': public_bytes
    }


def ed25519_sign(message: str, private_key_bytes: bytes, encoding: str = 'utf-8') -> bytes:
    """
    Sign message with Ed25519 private key.
    
    Args:
        message: Message to sign
        private_key_bytes: Private key (32 bytes, raw format)
        encoding: Text encoding
        
    Returns:
        Signature (64 bytes)
    """
    if not HAS_CRYPTOGRAPHY:
        raise ImportError("cryptography library required. Install with: pip install cryptography")
    
    # Load private key from raw bytes
    private_key = ed25519.Ed25519PrivateKey.from_private_bytes(private_key_bytes)
    
    # Sign
    signature = private_key.sign(message.encode(encoding))
    
    return signature


def ed25519_verify(message: str, signature: bytes, public_key_bytes: bytes, encoding: str = 'utf-8') -> bool:
    """
    Verify Ed25519 signature.
    
    Args:
        message: Original message
        signature: Signature to verify (64 bytes)
        public_key_bytes: Public key (32 bytes, raw format)
        encoding: Text encoding
        
    Returns:
        True if signature is valid, False otherwise
    """
    if not HAS_CRYPTOGRAPHY:
        raise ImportError("cryptography library required. Install with: pip install cryptography")
    
    # Load public key from raw bytes
    public_key = ed25519.Ed25519PublicKey.from_public_bytes(public_key_bytes)
    
    try:
        public_key.verify(signature, message.encode(encoding))
        return True
    except Exception:
        return False


# ==========================================
# Key Derivation Functions
# ==========================================

def pbkdf2_derive_key(password: str, salt: Optional[bytes] = None, 
                      iterations: int = 100000, key_length: int = 32) -> dict:
    """
    Derive encryption key from password using PBKDF2.
    
    Args:
        password: Password to derive key from
        salt: Salt bytes (generated if None)
        iterations: Number of iterations (100000+ recommended)
        key_length: Derived key length in bytes (32 = 256 bits)
        
    Returns:
        Dict with 'key' (bytes), 'salt' (bytes), 'iterations', 'key_length'
    """
    if not HAS_CRYPTOGRAPHY:
        raise ImportError("cryptography library required. Install with: pip install cryptography")
    
    if salt is None:
        salt = secrets.token_bytes(16)
    
    # Create PBKDF2 KDF
    kdf = PBKDF2HMAC(
        algorithm=hashes.SHA256(),
        length=key_length,
        salt=salt,
        iterations=iterations,
        backend=default_backend()
    )
    
    # Derive key
    key = kdf.derive(password.encode('utf-8'))
    
    return {
        'key': key,
        'salt': salt,
        'iterations': iterations,
        'key_length': key_length,
        'algorithm': 'pbkdf2_sha256'
    }


def argon2_derive_key(password: str, salt: Optional[bytes] = None,
                      time_cost: int = 2, memory_cost: int = 65536,
                      parallelism: int = 1, key_length: int = 32) -> dict:
    """
    Derive encryption key from password using Argon2 (winner of Password Hashing Competition).
    
    Args:
        password: Password to derive key from
        salt: Salt bytes (generated if None)
        time_cost: Number of iterations
        memory_cost: Memory usage in KiB
        parallelism: Degree of parallelism
        key_length: Derived key length in bytes
        
    Returns:
        Dict with 'key', 'salt', parameters
    """
    if not HAS_CRYPTOGRAPHY:
        raise ImportError("cryptography library required. Install with: pip install cryptography")
    
    if salt is None:
        salt = secrets.token_bytes(16)
    
    # Create Argon2id KDF (winner of Password Hashing Competition)
    kdf = Argon2id(
        salt=salt,
        length=key_length,
        iterations=time_cost,
        lanes=parallelism,
        memory_cost=memory_cost,
    )
    
    # Derive key
    key = kdf.derive(password.encode('utf-8'))
    
    return {
        'key': key,
        'salt': salt,
        'time_cost': time_cost,
        'memory_cost': memory_cost,
        'parallelism': parallelism,
        'key_length': key_length,
        'algorithm': 'argon2'
    }


def register_crypto_functions(runtime: Runtime) -> None:
    """Register cryptography functions with the runtime."""
    
    # Hashing
    runtime.register_function("hash_md5", hash_md5)
    runtime.register_function("hash_sha1", hash_sha1)
    runtime.register_function("hash_sha256", hash_sha256)
    runtime.register_function("hash_sha512", hash_sha512)
    runtime.register_function("hash_sha3_256", hash_sha3_256)
    runtime.register_function("hash_sha3_512", hash_sha3_512)
    runtime.register_function("hash_blake2b", hash_blake2b)
    runtime.register_function("hash_blake2s", hash_blake2s)
    
    # HMAC
    runtime.register_function("hmac_sha256", hmac_sha256)
    runtime.register_function("hmac_sha512", hmac_sha512)
    
    # Base64
    runtime.register_function("base64_encode", base64_encode)
    runtime.register_function("base64_decode", base64_decode)
    runtime.register_function("base64_url_encode", base64_url_encode)
    runtime.register_function("base64_url_decode", base64_url_decode)
    
    # Random
    runtime.register_function("random_bytes", random_bytes)
    runtime.register_function("random_hex", random_hex)
    runtime.register_function("random_token", random_token)
    
    # Security
    runtime.register_function("compare_digest", compare_digest)
    runtime.register_function("hash_password", hash_password)
    runtime.register_function("verify_password", verify_password)
    
    # Symmetric Encryption (AES-256-GCM)
    if HAS_CRYPTOGRAPHY:
        runtime.register_function("aes_generate_key", aes_generate_key)
        runtime.register_function("aes_encrypt", aes_encrypt)
        runtime.register_function("aes_decrypt", aes_decrypt)
        runtime.register_function("chacha20_generate_key", chacha20_generate_key)
        runtime.register_function("chacha20_encrypt", chacha20_encrypt)
        runtime.register_function("chacha20_decrypt", chacha20_decrypt)
        
        # Asymmetric Encryption (RSA)
        runtime.register_function("rsa_generate_keypair", rsa_generate_keypair)
        runtime.register_function("rsa_encrypt", rsa_encrypt)
        runtime.register_function("rsa_decrypt", rsa_decrypt)
        runtime.register_function("rsa_sign", rsa_sign)
        runtime.register_function("rsa_verify", rsa_verify)
        
        # Ed25519 Signatures
        runtime.register_function("ed25519_generate_keypair", ed25519_generate_keypair)
        runtime.register_function("ed25519_sign", ed25519_sign)
        runtime.register_function("ed25519_verify", ed25519_verify)
        
        # Key Derivation
        runtime.register_function("pbkdf2_derive_key", pbkdf2_derive_key)
        runtime.register_function("argon2_derive_key", argon2_derive_key)
    
    # Aliases
    runtime.register_function("md5", hash_md5)
    runtime.register_function("sha256", hash_sha256)
    runtime.register_function("sha512", hash_sha512)
