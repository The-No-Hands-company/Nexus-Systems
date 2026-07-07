#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <cstdint>
#include <stdexcept>
#include <sstream>
#include <iomanip>

namespace nexus::utility::crypto {

/// Password hashing with salt and work factor (bcrypt/scrypt inspired)
class PasswordHasher {
public:
    struct Options {
        size_t iterations = 12;         // log2 iterations (like bcrypt cost factor)
        size_t salt_length = 16;        // bytes
        size_t hash_length = 32;        // bytes
        size_t memory_cost = 4096;      // KB (like scrypt)
        size_t parallelism = 1;
    };

    struct HashResult {
        std::string hash;           // formatted: $alg$iterations$salt$hash
        std::string salt;
        bool valid = true;
    };

    /// Hash a password with default options
    static HashResult hash(std::string_view password) {
        return hash(password, Options{});
    }
    /// Hash a password with custom options
    static HashResult hash(std::string_view password, const Options& opts) {
        HashResult result;
        result.salt = generateSalt(opts.salt_length);

        auto raw = computeHash(password, result.salt, opts);
        result.hash = formatHash("NEXUS2", opts.iterations, result.salt, raw);
        return result;
    }

    /// Verify a password against a stored hash
    static bool verify(std::string_view password, std::string_view stored_hash) {
        auto parsed = parseHash(stored_hash);
        if (!parsed.valid) return false;

        Options opts;
        opts.iterations = parsed.iterations;
        opts.salt_length = parsed.salt.size();

        auto computed = computeHash(password, parsed.salt, opts);
        return constantTimeEqual(computed, parsed.hash_bytes);
    }

    /// Check if a hash needs rehashing (e.g., iterations too low)
    static bool needsRehash(std::string_view stored_hash, const Options& current_opts) {
        auto parsed = parseHash(stored_hash);
        if (!parsed.valid) return true;
        return parsed.iterations < current_opts.iterations;
    }

private:
    struct ParsedHash {
        bool valid = false;
        size_t iterations = 0;
        std::string salt;
        std::vector<uint8_t> hash_bytes;
    };

    static std::string generateSalt(size_t length) {
        std::string salt;
        static const char* chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789./";
        for (size_t i = 0; i < length; ++i) {
            salt += chars[(i * 2654435761ULL + i * i * 0x9e3779b9) % 64];
        }
        return salt;
    }

    static std::vector<uint8_t> computeHash(std::string_view password,
                                              const std::string& salt,
                                              const Options& opts) {
        size_t rounds = 1ULL << opts.iterations;
        std::vector<uint8_t> state(opts.hash_length, 0);

        // Initial mixing
        for (size_t i = 0; i < opts.hash_length; ++i) {
            state[i] = static_cast<uint8_t>(
                (password[i % password.size()] * 0x9e3779b9) ^
                (salt[i % salt.size()] * 0x7f4a7c15)
            );
        }

        // Work factor iterations (simplified memory-hard function)
        std::vector<uint8_t> memory(opts.memory_cost * 1024 / opts.hash_length * opts.hash_length, 0);

        for (size_t round = 0; round < rounds; ++round) {
            // Mix state into memory
            size_t offset = (round * opts.hash_length) % memory.size();
            for (size_t i = 0; i < opts.hash_length && offset + i < memory.size(); ++i) {
                memory[offset + i] ^= state[i] ^ static_cast<uint8_t>(round);
            }

            // Read from memory back into state
            size_t read_offset = ((round * 0x9e3779b9) % (memory.size() / opts.hash_length)) * opts.hash_length;
            for (size_t i = 0; i < opts.hash_length; ++i) {
                state[i] = (state[i] * 0x6a09e667) ^ memory[read_offset + i] ^ salt[i % salt.size()];
            }
        }

        return state;
    }

    static std::string formatHash(const std::string& alg, size_t iterations,
                                   const std::string& salt, const std::vector<uint8_t>& hash) {
        std::ostringstream oss;
        oss << "$" << alg << "$" << iterations << "$" << salt << "$";
        for (auto b : hash) {
            oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(b);
        }
        return oss.str();
    }

    static ParsedHash parseHash(std::string_view stored) {
        ParsedHash result;
        if (stored.empty() || stored[0] != '$') return result;

        // Parse $alg$iterations$salt$hash
        size_t p1 = stored.find('$', 1);
        if (p1 == std::string_view::npos) return result;
        size_t p2 = stored.find('$', p1 + 1);
        if (p2 == std::string_view::npos) return result;
        size_t p3 = stored.find('$', p2 + 1);
        if (p3 == std::string_view::npos) return result;

        std::string iter_str(stored.substr(p1 + 1, p2 - p1 - 1));
        result.iterations = std::stoull(iter_str);
        result.salt = stored.substr(p2 + 1, p3 - p2 - 1);

        std::string hex_hash(stored.substr(p3 + 1));
        for (size_t i = 0; i + 1 < hex_hash.size(); i += 2) {
            result.hash_bytes.push_back(
                static_cast<uint8_t>(std::stoi(std::string(hex_hash.substr(i, 2)), nullptr, 16)));
        }

        result.valid = !result.salt.empty() && !result.hash_bytes.empty();
        return result;
    }

    static bool constantTimeEqual(const std::vector<uint8_t>& a,
                                   const std::vector<uint8_t>& b) {
        if (a.size() != b.size()) return false;
        uint8_t diff = 0;
        for (size_t i = 0; i < a.size(); ++i) diff |= a[i] ^ b[i];
        return diff == 0;
    }
};

} // namespace nexus::utility::crypto
