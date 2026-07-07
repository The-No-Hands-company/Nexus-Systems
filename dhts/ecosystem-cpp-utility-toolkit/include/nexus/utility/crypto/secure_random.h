#pragma once

// ╔══════════════════════════════════════════════════════════════════════════╗
// ║                      SECURITY WARNING                                    ║
// ║ This RNG is NOT cryptographically secure. It uses MT19937 (Mersenne      ║
// ║ Twister), which is a general-purpose PRNG predictable after 624 outputs. ║
// ║ Key generation, tokens, and nonces produced by this class are NOT safe   ║
// ║ for cryptographic use.                                                   ║
// ║ For real CSPRNG: use getrandom(), /dev/urandom, or platform crypto APIs. ║
// ╚══════════════════════════════════════════════════════════════════════════╝

#include <random>
#include <vector>
#include <cstdint>
#include <string>
#include <algorithm>
#include <climits>
#include <array>

namespace nexus::utility::crypto {

/// @brief NON-cryptographic random number generator.
/// Uses MT19937_64 seeded from std::random_device. NOT suitable for key material.
class SecureRandom {
public:
    SecureRandom() : rng_(makeSeeder()) {}

    /// Fill buffer with random bytes (NOT cryptographically secure)
    void fill(std::vector<uint8_t>& buffer) {
        std::uniform_int_distribution<uint32_t> dist(0, 255);
        for (auto& b : buffer) {
            b = static_cast<uint8_t>(dist(rng_));
        }
    }

    [[nodiscard]] std::vector<uint8_t> bytes(size_t count) {
        std::vector<uint8_t> result(count);
        fill(result);
        return result;
    }

    template <typename T>
        requires std::is_integral_v<T>
    [[nodiscard]] T intInRange(T min, T max) {
        std::uniform_int_distribution<T> dist(min, max);
        return dist(rng_);
    }

    [[nodiscard]] uint64_t uint64() {
        std::uniform_int_distribution<uint64_t> dist;
        return dist(rng_);
    }

    [[nodiscard]] uint32_t uint32() {
        std::uniform_int_distribution<uint32_t> dist;
        return dist(rng_);
    }

    [[nodiscard]] double double01() {
        std::uniform_real_distribution<double> dist(0.0, 1.0);
        return dist(rng_);
    }

    [[nodiscard]] std::string hexString(size_t byte_count) {
        auto b = bytes(byte_count);
        static const char hex[] = "0123456789abcdef";
        std::string result;
        result.reserve(byte_count * 2);
        for (auto byte : b) {
            result += hex[byte >> 4];
            result += hex[byte & 0x0F];
        }
        return result;
    }

    [[nodiscard]] std::string token(size_t length) {
        static const char chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
        std::string result;
        result.reserve(length);
        std::uniform_int_distribution<size_t> dist(0, 61);
        for (size_t i = 0; i < length; ++i) {
            result += chars[dist(rng_)];
        }
        return result;
    }

    template <typename RandomIt>
    void shuffle(RandomIt first, RandomIt last) {
        auto n = std::distance(first, last);
        for (decltype(n) i = n - 1; i > 0; --i) {
            std::uniform_int_distribution<decltype(i)> dist(0, i);
            std::swap(first[i], first[dist(rng_)]);
        }
    }

private:
    [[nodiscard]] static std::mt19937_64 makeSeeder() noexcept {
        std::array<std::seed_seq::result_type, 8> seeds{};
        std::random_device rd;
        for (auto& s : seeds) {
            s = static_cast<std::seed_seq::result_type>(rd());
        }
        std::seed_seq seq(seeds.begin(), seeds.end());
        return std::mt19937_64(seq);
    }

    std::mt19937_64 rng_;
};

} // namespace nexus::utility::crypto
