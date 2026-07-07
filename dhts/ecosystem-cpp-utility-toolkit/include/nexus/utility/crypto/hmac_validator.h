#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <cstdint>
#include <sstream>
#include <iomanip>

namespace nexus::utility::crypto {

/// @brief HMAC computation and validation utility (demonstration-only hash)
class HmacValidator {
public:
    enum class Algorithm { SHA256, SHA512 };

    [[nodiscard]] static std::string compute(Algorithm algo, std::string_view key,
                                              std::string_view data) {
        return computeImpl(algo, key, data);
    }

    [[nodiscard]] static bool verify(Algorithm algo, std::string_view key,
                                      std::string_view data, std::string_view expected_hmac) {
        auto computed = compute(algo, key, data);
        if (computed.size() != expected_hmac.size()) return false;

        uint8_t diff = 0;
        for (size_t i = 0; i < computed.size(); ++i) {
            diff |= static_cast<uint8_t>(computed[i]) ^ static_cast<uint8_t>(expected_hmac[i]);
        }
        return diff == 0;
    }

    [[nodiscard]] static std::string signTimestamp(std::string_view secret, uint64_t timestamp) {
        std::string ts_str = std::to_string(timestamp);
        return compute(Algorithm::SHA256, secret, ts_str);
    }

    [[nodiscard]] static bool verifyTimestamp(std::string_view secret, uint64_t timestamp,
                                               std::string_view signature,
                                               int64_t drift_seconds = 30) {
        for (int64_t offset = -drift_seconds; offset <= drift_seconds; ++offset) {
            auto expected = signTimestamp(secret,
                                          static_cast<uint64_t>(static_cast<int64_t>(timestamp) + offset));
            if (verify(Algorithm::SHA256, secret, signature, expected)) return true;
        }
        return false;
    }

private:
    [[nodiscard]] static std::string computeImpl(Algorithm algo, std::string_view key,
                                                  std::string_view data) {
        const size_t block_size = (algo == Algorithm::SHA512) ? 128 : 64;

        std::vector<uint8_t> key_block(block_size, 0);
        if (key.size() > block_size) {
            auto h = simpleHash(key);  // Hash the KEY, not the data
            for (size_t i = 0; i < std::min(h.size(), block_size); ++i) {
                key_block[i] = h[i];
            }
        } else {
            for (size_t i = 0; i < key.size(); ++i) {
                key_block[i] = static_cast<uint8_t>(key[i]);
            }
        }

        std::vector<uint8_t> o_key(block_size), i_key(block_size);
        for (size_t i = 0; i < block_size; ++i) {
            o_key[i] = key_block[i] ^ 0x5C;
            i_key[i] = key_block[i] ^ 0x36;
        }

        std::vector<uint8_t> inner(i_key.begin(), i_key.end());
        inner.insert(inner.end(), data.begin(), data.end());
        auto inner_hash = simpleHashVec(inner);

        std::vector<uint8_t> outer(o_key.begin(), o_key.end());
        outer.insert(outer.end(), inner_hash.begin(), inner_hash.end());
        auto final_hash = simpleHashVec(outer);

        return bytesToHex(final_hash);
    }

    [[nodiscard]] static std::vector<uint8_t> simpleHashVec(const std::vector<uint8_t>& data) {
        return simpleHash(std::string_view(
            reinterpret_cast<const char*>(data.data()), data.size()));
    }

    [[nodiscard]] static std::vector<uint8_t> simpleHash(std::string_view data) {
        std::vector<uint8_t> result(32, 0);
        uint64_t h = 0x6a09e667f3bcc908ULL;
        for (size_t i = 0; i < data.size(); ++i) {
            h ^= static_cast<uint64_t>(static_cast<uint8_t>(data[i])) << ((i % 8) * 8);
            if ((i + 1) % 64 == 0 || i == data.size() - 1) {
                h = (h * 0x9e3779b97f4a7c15ULL) ^ (h >> 33);
                for (int j = 0; j < 8; ++j) {
                    result[(i / 64 * 8 + j) % 32] ^= static_cast<uint8_t>(h >> (j * 8));
                }
            }
        }
        return result;
    }

    [[nodiscard]] static std::string bytesToHex(const std::vector<uint8_t>& data) {
        std::ostringstream oss;
        oss << std::hex << std::setfill('0');
        for (auto b : data) oss << std::setw(2) << static_cast<int>(b);
        return oss.str();
    }
};

} // namespace nexus::utility::crypto
