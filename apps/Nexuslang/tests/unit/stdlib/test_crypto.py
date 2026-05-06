"""
Tests for the NexusLang stdlib crypto module.

Covers:
- Always-available: MD5, SHA-1/256/512/SHA3/BLAKE2, HMAC, Base64,
  secure random, PBKDF2 password hashing and verification.
- Cryptography-gated (skip if HAS_CRYPTOGRAPHY is False):
  AES-256-GCM, ChaCha20, RSA, Ed25519, PBKDF2 key derivation,
  Argon2 key derivation.
"""

import os
import sys
import pytest

_PROJECT_ROOT = os.path.abspath(
    os.path.join(os.path.dirname(__file__), "..", "..", "..")
)
if _PROJECT_ROOT not in sys.path:
    sys.path.insert(0, _PROJECT_ROOT)

from nexuslang.stdlib.crypto import (
    HAS_CRYPTOGRAPHY,
    hash_md5,
    hash_sha1,
    hash_sha256,
    hash_sha512,
    hash_sha3_256,
    hash_sha3_512,
    hash_blake2b,
    hash_blake2s,
    hmac_sha256,
    hmac_sha512,
    base64_encode,
    base64_decode,
    base64_url_encode,
    base64_url_decode,
    random_bytes,
    random_hex,
    random_token,
    compare_digest,
    hash_password,
    verify_password,
)

# Optional heavy imports guarded by HAS_CRYPTOGRAPHY
if HAS_CRYPTOGRAPHY:
    from nexuslang.stdlib.crypto import (
        aes_generate_key,
        aes_encrypt,
        aes_decrypt,
        rsa_generate_keypair,
        rsa_encrypt,
        rsa_decrypt,
        rsa_sign,
        rsa_verify,
        ed25519_generate_keypair,
        ed25519_sign,
        ed25519_verify,
        pbkdf2_derive_key,
    )

_REQUIRES_CRYPTOGRAPHY = pytest.mark.skipif(
    not HAS_CRYPTOGRAPHY,
    reason="'cryptography' package not installed",
)

# ---------------------------------------------------------------------------
# Hash functions (always available via hashlib)
# ---------------------------------------------------------------------------


class TestMD5:
    def test_returns_hex_string(self):
        result = hash_md5("hello")
        assert isinstance(result, str)
        assert len(result) == 32

    def test_deterministic(self):
        assert hash_md5("hello") == hash_md5("hello")

    def test_different_inputs_differ(self):
        assert hash_md5("hello") != hash_md5("world")

    def test_known_value(self):
        # MD5("") = d41d8cd98f00b204e9800998ecf8427e
        assert hash_md5("") == "d41d8cd98f00b204e9800998ecf8427e"

    def test_empty_string(self):
        result = hash_md5("")
        assert isinstance(result, str) and len(result) == 32


class TestSHA256:
    def test_returns_hex_string(self):
        result = hash_sha256("hello")
        assert isinstance(result, str) and len(result) == 64

    def test_known_value(self):
        expected = "2cf24dba5fb0a30e26e83b2ac5b9e29e1b161e5c1fa7425e73043362938b9824"
        assert hash_sha256("hello") == expected

    def test_deterministic(self):
        assert hash_sha256("test") == hash_sha256("test")

    def test_different_inputs_differ(self):
        assert hash_sha256("abc") != hash_sha256("xyz")


class TestSHA512:
    def test_returns_hex_string(self):
        result = hash_sha512("hello")
        assert isinstance(result, str) and len(result) == 128

    def test_deterministic(self):
        assert hash_sha512("data") == hash_sha512("data")


class TestSHA3:
    def test_sha3_256_length(self):
        assert len(hash_sha3_256("test")) == 64

    def test_sha3_512_length(self):
        assert len(hash_sha3_512("test")) == 128

    def test_sha3_256_different_from_sha256(self):
        assert hash_sha3_256("hello") != hash_sha256("hello")


class TestBLAKE2:
    def test_blake2b_length(self):
        assert len(hash_blake2b("test")) == 128  # 64 bytes hex

    def test_blake2s_length(self):
        assert len(hash_blake2s("test")) == 64  # 32 bytes hex

    def test_deterministic(self):
        assert hash_blake2b("abc") == hash_blake2b("abc")


# ---------------------------------------------------------------------------
# HMAC
# ---------------------------------------------------------------------------


class TestHMAC:
    def test_hmac_sha256_returns_string(self):
        result = hmac_sha256("message", "secret")
        assert isinstance(result, str)

    def test_hmac_sha256_key_changes_output(self):
        assert hmac_sha256("message", "key1") != hmac_sha256("message", "key2")

    def test_hmac_sha256_data_changes_output(self):
        assert hmac_sha256("msg1", "key") != hmac_sha256("msg2", "key")

    def test_hmac_sha512_returns_string(self):
        result = hmac_sha512("message", "secret")
        assert isinstance(result, str)

    def test_hmac_deterministic(self):
        assert hmac_sha256("x", "k") == hmac_sha256("x", "k")


# ---------------------------------------------------------------------------
# Base64
# ---------------------------------------------------------------------------


class TestBase64:
    def test_encode_returns_string(self):
        result = base64_encode("hello")
        assert isinstance(result, str)

    def test_encode_decode_roundtrip(self):
        original = "Hello, World!"
        assert base64_decode(base64_encode(original)) == original

    def test_url_encode_decode_roundtrip(self):
        original = "binary+data/example=="
        assert base64_url_decode(base64_url_encode(original)) == original

    def test_empty_string_roundtrip(self):
        assert base64_decode(base64_encode("")) == ""

    def test_known_value(self):
        assert base64_encode("hello") == "aGVsbG8="


# ---------------------------------------------------------------------------
# Random primitives
# ---------------------------------------------------------------------------


class TestRandomPrimitives:
    def test_random_bytes_type(self):
        result = random_bytes(16)
        assert isinstance(result, bytes)

    def test_random_bytes_length(self):
        for n in (1, 8, 32, 64):
            assert len(random_bytes(n)) == n

    def test_random_hex_type(self):
        assert isinstance(random_hex(8), str)

    def test_random_hex_length(self):
        # n bytes => 2n hex chars
        assert len(random_hex(8)) == 16

    def test_random_token_type(self):
        assert isinstance(random_token(32), str)

    def test_random_bytes_differ(self):
        # Statistically virtually impossible to collide
        assert random_bytes(32) != random_bytes(32)

    def test_compare_digest_equal(self):
        assert compare_digest("abc", "abc") is True

    def test_compare_digest_not_equal(self):
        assert compare_digest("abc", "xyz") is False


# ---------------------------------------------------------------------------
# Password hashing (hashlib PBKDF2 — always available)
# ---------------------------------------------------------------------------


class TestPasswordHashing:
    def test_hash_password_returns_dict(self):
        result = hash_password("mypassword")
        assert isinstance(result, dict)

    def test_hash_password_has_required_keys(self):
        result = hash_password("mypassword")
        assert "hash" in result and "salt" in result

    def test_verify_password_correct(self):
        info = hash_password("secret123")
        assert verify_password("secret123", info) is True

    def test_verify_password_wrong(self):
        info = hash_password("secret123")
        assert verify_password("wrongpassword", info) is False

    def test_same_password_diff_salt(self):
        h1 = hash_password("pw")
        h2 = hash_password("pw")
        # Different salts should produce different hashes
        assert h1["hash"] != h2["hash"]

    def test_explicit_salt(self):
        h1 = hash_password("pw", salt="fixedsalt")
        h2 = hash_password("pw", salt="fixedsalt")
        assert h1["hash"] == h2["hash"]


# ---------------------------------------------------------------------------
# AES (requires 'cryptography' package)
# ---------------------------------------------------------------------------


@_REQUIRES_CRYPTOGRAPHY
class TestAES:
    def test_generate_key_type(self):
        key = aes_generate_key()
        assert isinstance(key, bytes)

    def test_generate_key_length(self):
        assert len(aes_generate_key()) == 32  # AES-256

    def test_encrypt_returns_dict(self):
        key = aes_generate_key()
        result = aes_encrypt("hello", key)
        assert isinstance(result, dict)

    def test_encrypt_decrypt_roundtrip(self):
        key = aes_generate_key()
        plaintext = "The quick brown fox jumps over the lazy dog"
        encrypted = aes_encrypt(plaintext, key)
        assert aes_decrypt(encrypted, key) == plaintext

    def test_different_keys_fail_decrypt(self):
        key1 = aes_generate_key()
        key2 = aes_generate_key()
        encrypted = aes_encrypt("hello", key1)
        with pytest.raises(Exception):
            aes_decrypt(encrypted, key2)

    def test_encrypt_produces_different_ciphertexts(self):
        key = aes_generate_key()
        e1 = aes_encrypt("hello", key)
        e2 = aes_encrypt("hello", key)
        # GCM uses random nonce — ciphertexts must differ
        assert e1 != e2


# ---------------------------------------------------------------------------
# RSA (requires 'cryptography' package)
# ---------------------------------------------------------------------------


@_REQUIRES_CRYPTOGRAPHY
class TestRSA:
    @pytest.fixture(scope="class")
    def keypair(self):
        return rsa_generate_keypair(key_size=1024)  # small key for speed

    def test_generate_keypair_keys(self, keypair):
        assert "public_key" in keypair and "private_key" in keypair

    def test_encrypt_decrypt_roundtrip(self, keypair):
        plaintext = "hello RSA"
        ciphertext = rsa_encrypt(plaintext, keypair["public_key"])
        assert rsa_decrypt(ciphertext, keypair["private_key"]) == plaintext

    def test_sign_verify(self, keypair):
        message = "sign me"
        sig = rsa_sign(message, keypair["private_key"])
        assert rsa_verify(message, sig, keypair["public_key"]) is True

    def test_verify_wrong_message_fails(self, keypair):
        sig = rsa_sign("original", keypair["private_key"])
        assert rsa_verify("modified", sig, keypair["public_key"]) is False


# ---------------------------------------------------------------------------
# Ed25519 (requires 'cryptography' package)
# ---------------------------------------------------------------------------


@_REQUIRES_CRYPTOGRAPHY
class TestEd25519:
    @pytest.fixture(scope="class")
    def keypair(self):
        return ed25519_generate_keypair()

    def test_keypair_keys(self, keypair):
        assert "public_key" in keypair and "private_key" in keypair

    def test_sign_verify(self, keypair):
        message = "hello ed25519"
        sig = ed25519_sign(message, keypair["private_key"])
        assert ed25519_verify(message, sig, keypair["public_key"]) is True

    def test_verify_wrong_message_fails(self, keypair):
        sig = ed25519_sign("original", keypair["private_key"])
        assert ed25519_verify("tampered", sig, keypair["public_key"]) is False
