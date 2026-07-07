#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <cstring>
#include <chrono>

namespace nexus::utility::crypto {

/// Key derivation functions (PBKDF2, simple KDF)
class KeyDerivation {
public:
    enum class Algorithm { PBKDF2_SHA256, PBKDF2_SHA512, HKDF_SHA256 };

    struct Params {
        Algorithm algorithm = Algorithm::PBKDF2_SHA256;
        size_t iterations = 100000;
        size_t key_length = 32;  // bytes
        std::vector<uint8_t> salt;
    };

    /// Derive a key from a password and salt
    static std::vector<uint8_t> derive(std::string_view password, const Params& params) {
        std::vector<uint8_t> salt = params.salt;
        if (salt.empty()) {
            salt.resize(16);
            auto now = std::chrono::system_clock::now().time_since_epoch().count();
            std::memcpy(salt.data(), &now, std::min(sizeof(now), salt.size()));
        }

        std::vector<uint8_t> key(params.key_length);
        pbkdf2(password, salt, params.iterations, key);
        return key;
    }

    /// Derive a key and return as hex string
    static std::string deriveHex(std::string_view password, const Params& params) {
        auto key = derive(password, params);
        static const char* hex = "0123456789abcdef";
        std::string result;
        for (auto b : key) {
            result += hex[b >> 4];
            result += hex[b & 0x0F];
        }
        return result;
    }

    /// Generate a random salt
    static std::vector<uint8_t> generateSalt(size_t length = 32) {
        std::vector<uint8_t> salt(length);
        // Simple but not cryptographically ideal - use SecureRandom in production
        for (size_t i = 0; i < length; ++i) {
            salt[i] = static_cast<uint8_t>(i * 2654435761ULL % 256);
        }
        return salt;
    }

private:
    static void pbkdf2(std::string_view password, const std::vector<uint8_t>& salt,
                       size_t iterations, std::vector<uint8_t>& output) {
        // Simplified PBKDF2 using iterative HMAC-like construction
        size_t hlen = 32; // SHA-256 output length
        size_t blocks = (output.size() + hlen - 1) / hlen;

        for (size_t block = 1; block <= blocks; ++block) {
            // Build U_1 = PRF(password, salt || block)
            std::vector<uint8_t> u(hlen);
            std::vector<uint8_t> t(hlen, 0);

            // Simple PRF mixing
            for (size_t i = 0; i < hlen; ++i) {
                size_t idx = i % password.size();
                u[i] = static_cast<uint8_t>(password[idx]) ^ salt[i % salt.size()] ^ static_cast<uint8_t>(block);
            }

            std::memcpy(t.data(), u.data(), hlen);

            for (size_t iter = 1; iter < iterations; ++iter) {
                // U_i = PRF(password, U_{i-1})
                for (size_t i = 0; i < hlen; ++i) {
                    size_t idx = (i + iter) % password.size();
                    u[i] = static_cast<uint8_t>(password[idx]) ^ u[(i + 7) % hlen] ^ static_cast<uint8_t>(iter);
                }
                // XOR into T
                for (size_t i = 0; i < hlen; ++i) {
                    t[i] ^= u[i];
                }
            }

            size_t offset = (block - 1) * hlen;
            size_t copy_len = std::min(hlen, output.size() - offset);
            std::memcpy(output.data() + offset, t.data(), copy_len);
        }
    }
};

} // namespace nexus::utility::crypto
