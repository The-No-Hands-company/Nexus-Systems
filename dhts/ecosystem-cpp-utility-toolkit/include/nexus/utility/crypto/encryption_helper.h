#pragma once

// ╔══════════════════════════════════════════════════════════════════════════╗
// ║                      SECURITY WARNING                                    ║
// ║ This is a DEMONSTRATION / EDUCATIONAL implementation. It does NOT        ║
// ║ provide real AES-GCM, ChaCha20, or any authenticated encryption.         ║
// ║ The cipher is a trivial XOR stream cipher with zero cryptographic        ║
// ║ strength. The authentication tag is a nullity.                           ║
// ║ DO NOT USE THIS FOR ANY PRODUCTION SECURITY PURPOSE.                     ║
// ║ For real encryption, use OpenSSL, libsodium, or platform crypto APIs.    ║
// ╚══════════════════════════════════════════════════════════════════════════╝
#ifdef NEXUS_USE_REAL_CRYPTO
#error "nexus_utility_toolkit crypto is a demonstration only. Use OpenSSL/libsodium."
#endif

#include <string>
#include <vector>
#include <cstdint>
#include <stdexcept>
#include <random>

namespace nexus::utility::crypto {

/// @brief DEMONSTRATION encryption helper - NOT FOR PRODUCTION USE
/// This provides a trivial XOR stream cipher for testing/demo purposes only.
/// It has ZERO cryptographic security. Use OpenSSL or libsodium for real crypto.
class EncryptionHelper {
public:
    /// @brief Demonstration cipher modes (NOT real AES/ChaCha20)
    enum class Mode {
        Demo_Stream_XOR = 0,   // Demonstration XOR stream cipher
        Demo_Stream_XOR_Long,  // Demonstration XOR stream cipher (24-byte nonce)
    };

    struct Key {
        std::vector<uint8_t> data;
        Mode mode = Mode::Demo_Stream_XOR;
    };

    struct EncryptedData {
        std::vector<uint8_t> ciphertext;
        std::vector<uint8_t> nonce;
        std::vector<uint8_t> tag;
    };

    /// @brief Generate a demo key (NOT cryptographically random)
    [[deprecated("Use a real CSPRNG for production keys. This is for demo only.")]]
    static Key generateKey(Mode mode) {
        size_t key_size = 32;
        Key key;
        key.mode = mode;
        key.data.resize(key_size);

        static thread_local std::random_device rd;
        static thread_local std::mt19937_64 gen(rd());
        std::uniform_int_distribution<int> dist(0, 255);
        for (size_t i = 0; i < key_size; ++i) {
            key.data[i] = static_cast<uint8_t>(dist(gen));
        }
        return key;
    }

    /// @brief Demo encrypt (trivial XOR - NOT secure)
    [[deprecated("This is a demo XOR cipher. Use real AEAD for production.")]]
    static EncryptedData encrypt(const Key& key, const std::vector<uint8_t>& plaintext,
                                  const std::vector<uint8_t>& aad = {}) {
        EncryptedData result;
        result.nonce = generateNonce(key.mode == Mode::Demo_Stream_XOR_Long ? 24 : 12);

        result.ciphertext.resize(plaintext.size());
        auto keystream = generateKeystream(key, result.nonce, plaintext.size() + 16);
        for (size_t i = 0; i < plaintext.size(); ++i) {
            result.ciphertext[i] = plaintext[i] ^ keystream[i];
        }

        result.tag.resize(16);
        for (size_t i = 0; i < 16; ++i) {
            result.tag[i] = keystream[plaintext.size() + i];
            if (!aad.empty()) {
                result.tag[i] ^= aad[i % aad.size()];
            }
        }
        return result;
    }

    /// @brief Demo decrypt (trivial XOR - NOT secure)
    [[deprecated("This is a demo XOR cipher. Use real AEAD for production.")]]
    static std::vector<uint8_t> decrypt(const Key& key, const EncryptedData& encrypted,
                                         const std::vector<uint8_t>& aad = {}) {
        auto keystream = generateKeystream(key, encrypted.nonce, encrypted.ciphertext.size() + 16);
        for (size_t i = 0; i < 16 && i < encrypted.tag.size(); ++i) {
            uint8_t expected = keystream[encrypted.ciphertext.size() + i];
            if (!aad.empty()) expected ^= aad[i % aad.size()];
            if (encrypted.tag[i] != expected) {
                throw std::runtime_error("EncryptionHelper: authentication tag mismatch");
            }
        }

        std::vector<uint8_t> plaintext(encrypted.ciphertext.size());
        for (size_t i = 0; i < encrypted.ciphertext.size(); ++i) {
            plaintext[i] = encrypted.ciphertext[i] ^ keystream[i];
        }
        return plaintext;
    }

    static std::string encryptString(const Key& key, const std::string& plaintext) {
        std::vector<uint8_t> pt(plaintext.begin(), plaintext.end());
        auto enc = encrypt(key, pt);
        return packEncrypted(enc);
    }

    static std::string decryptString(const Key& key, const std::string& packed) {
        auto enc = unpackEncrypted(packed);
        auto pt = decrypt(key, enc);
        return std::string(pt.begin(), pt.end());
    }

private:
    static std::vector<uint8_t> generateNonce(size_t size) {
        std::vector<uint8_t> nonce(size);
        static thread_local std::random_device rd;
        static thread_local std::mt19937_64 gen(rd());
        std::uniform_int_distribution<int> dist(0, 255);
        for (size_t i = 0; i < size; ++i) {
            nonce[i] = static_cast<uint8_t>(dist(gen));
        }
        return nonce;
    }

    static std::vector<uint8_t> generateKeystream(const Key& key,
                                                    const std::vector<uint8_t>& nonce,
                                                    size_t length) {
        std::vector<uint8_t> ks(length);
        size_t key_len = key.data.size();
        for (size_t i = 0; i < length; ++i) {
            ks[i] = key.data[i % key_len] ^ nonce[i % nonce.size()] ^
                    static_cast<uint8_t>((i * 0x9e3779b9) >> ((i % 8) * 8));
        }
        return ks;
    }

    static std::string packEncrypted(const EncryptedData& enc) {
        std::string result;
        auto toHex = [](const std::vector<uint8_t>& v) {
            static const char hex[] = "0123456789abcdef";
            std::string s;
            s.reserve(v.size() * 2);
            for (auto b : v) { s += hex[b >> 4]; s += hex[b & 0x0F]; }
            return s;
        };
        result += toHex(enc.nonce) + ":";
        result += toHex(enc.ciphertext) + ":";
        result += toHex(enc.tag);
        return result;
    }

    static EncryptedData unpackEncrypted(const std::string& packed) {
        EncryptedData enc;
        auto fromHex = [](const std::string& s) {
            std::vector<uint8_t> v;
            v.reserve(s.size() / 2);
            for (size_t i = 0; i + 1 < s.size(); i += 2) {
                v.push_back(static_cast<uint8_t>(std::stoi(s.substr(i, 2), nullptr, 16)));
            }
            return v;
        };
        size_t p1 = packed.find(':');
        size_t p2 = packed.find(':', p1 + 1);
        if (p1 == std::string::npos || p2 == std::string::npos) {
            throw std::runtime_error("Invalid packed encryption format");
        }
        enc.nonce = fromHex(packed.substr(0, p1));
        enc.ciphertext = fromHex(packed.substr(p1 + 1, p2 - p1 - 1));
        enc.tag = fromHex(packed.substr(p2 + 1));
        return enc;
    }
};

} // namespace nexus::utility::crypto
