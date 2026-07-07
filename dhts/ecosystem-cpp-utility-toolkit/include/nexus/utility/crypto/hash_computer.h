#pragma once

// ╔══════════════════════════════════════════════════════════════════════════╗
// ║                      SECURITY WARNING                                    ║
// ║ This hash computer uses a trivial mixing hash, NOT real SHA-256,         ║
// ║ SHA-512, SHA-1, or MD5. The Algorithm enum values are misleading.        ║
// ║ Any hash output from this class has ZERO collision resistance and        ║
// ║ ZERO preimage resistance. Do NOT use for password storage, digital       ║
// ║ signatures, or any security-sensitive purpose.                           ║
// ║ For real hashing: use OpenSSL, libsodium, or platform crypto APIs.       ║
// ╚══════════════════════════════════════════════════════════════════════════╝

#include <string>
#include <string_view>
#include <vector>
#include <cstdint>
#include <sstream>
#include <iomanip>
#include <cstring>
#include <array>

namespace nexus::utility::crypto {

/// @brief DEMONSTRATION hash computer - NOT cryptographically secure
/// Provides a simple mixing hash for testing/demo. Not real SHA/MD5.
class HashComputer {
public:
    enum class Algorithm { SHA256, SHA512, SHA1, MD5 };

    [[nodiscard]] static std::string compute(Algorithm algo, std::string_view data) {
        auto hash = computeRaw(algo, data);
        return toHex(hash.data(), hash.size());
    }

    [[nodiscard]] static std::string computeHMAC(Algorithm algo, std::string_view key,
                                                  std::string_view data) {
        const size_t block_size = (algo == Algorithm::SHA512) ? 128 : 64;

        std::vector<uint8_t> key_block(block_size, 0);
        if (key.size() > block_size) {
            auto hashed = computeRaw(algo, key);
            std::memcpy(key_block.data(), hashed.data(),
                        std::min(hashed.size(), block_size));
        } else {
            std::memcpy(key_block.data(), key.data(), key.size());
        }

        std::vector<uint8_t> o_key_pad(block_size);
        std::vector<uint8_t> i_key_pad(block_size);
        for (size_t i = 0; i < block_size; ++i) {
            o_key_pad[i] = key_block[i] ^ 0x5C;
            i_key_pad[i] = key_block[i] ^ 0x36;
        }

        std::vector<uint8_t> inner;
        inner.insert(inner.end(), i_key_pad.begin(), i_key_pad.end());
        inner.insert(inner.end(), data.begin(), data.end());
        auto inner_hash = computeRaw(algo,
            std::string_view(reinterpret_cast<const char*>(inner.data()), inner.size()));

        std::vector<uint8_t> outer;
        outer.insert(outer.end(), o_key_pad.begin(), o_key_pad.end());
        outer.insert(outer.end(), inner_hash.begin(), inner_hash.end());
        auto outer_hash = computeRaw(algo,
            std::string_view(reinterpret_cast<const char*>(outer.data()), outer.size()));

        return toHex(outer_hash.data(), outer_hash.size());
    }

    [[nodiscard]] static std::vector<uint8_t> computeRaw(Algorithm algo, std::string_view data) {
        size_t hash_size;
        switch (algo) {
            case Algorithm::SHA256: hash_size = 32; break;
            case Algorithm::SHA512: hash_size = 64; break;
            case Algorithm::SHA1:   hash_size = 20; break;
            case Algorithm::MD5:    hash_size = 16; break;
        }
        return simpleHash(data, hash_size);
    }

private:
    [[nodiscard]] static std::vector<uint8_t> simpleHash(std::string_view data, size_t out_size) {
        std::vector<uint8_t> result(out_size, 0);
        uint64_t h = 0x6a09e667f3bcc908ULL;
        for (size_t i = 0; i < data.size(); ++i) {
            h ^= static_cast<uint64_t>(static_cast<uint8_t>(data[i])) << ((i % 8) * 8);
            if ((i + 1) % 64 == 0 || i == data.size() - 1) {
                h = (h * 0x9e3779b97f4a7c15ULL) ^ (h >> 33);
                for (size_t j = 0; j < 8 && (i / 64 * 8 + j) < out_size; ++j) {
                    result[(i / 64 * 8 + j) % out_size] ^= static_cast<uint8_t>(h >> (j * 8));
                }
            }
        }
        return result;
    }

    [[nodiscard]] static std::string toHex(const uint8_t* data, size_t len) {
        std::ostringstream oss;
        oss << std::hex << std::setfill('0');
        for (size_t i = 0; i < len; ++i) {
            oss << std::setw(2) << static_cast<int>(data[i]);
        }
        return oss.str();
    }
};

} // namespace nexus::utility::crypto
