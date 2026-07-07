#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <array>
#include <sstream>
#include <iomanip>
#include <cstring>

namespace nexus::utility::math {

/**
 * @brief Self-contained implementation of SHA-256 for basic hashing needs.
 * Does not depend on OpenSSL or other external libs.
 */
class CryptoHashWrapper {
public:
    static std::string sha256(const std::string& input) {
        return sha256(input.data(), input.size());
    }

    static std::string sha256(const void* data, size_t len) {
        std::vector<uint8_t> hash = calculateSHA256(static_cast<const uint8_t*>(data), len);
        
        std::ostringstream oss;
        for (uint8_t b : hash) {
            oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(b);
        }
        return oss.str();
    }

private:
    // SHA-256 Implementation details
    
    static constexpr std::array<uint32_t, 64> K = {
        0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
        0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
        0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
        0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
        0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
        0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
        0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
        0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
    };

    static uint32_t rightRotate(uint32_t value, uint32_t bits) {
        return (value >> bits) | (value << (32 - bits));
    }

    static std::vector<uint8_t> calculateSHA256(const uint8_t* data, size_t len) {
        // Initial hash values
        uint32_t h[8] = {
            0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
            0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19
        };

        // Padding
        uint64_t totalBits = len * 8;
        std::vector<uint8_t> message(data, data + len);
        message.push_back(0x80); // Append 1 bit

        while ((message.size() * 8 + 64) % 512 != 0) {
            message.push_back(0x00);
        }

        // Append 64-bit length (big-endian)
        for (int i = 7; i >= 0; --i) {
            message.push_back((totalBits >> (i * 8)) & 0xFF);
        }

        // Process chunks (512 bits / 64 bytes)
        for (size_t i = 0; i < message.size(); i += 64) {
            uint32_t w[64];
            
            // First 16 words
            for (int j = 0; j < 16; ++j) {
                w[j] = (static_cast<uint32_t>(message[i + j * 4]) << 24) |
                       (static_cast<uint32_t>(message[i + j * 4 + 1]) << 16) |
                       (static_cast<uint32_t>(message[i + j * 4 + 2]) << 8) |
                       (static_cast<uint32_t>(message[i + j * 4 + 3]));
            }

            // Extend to 64 words
            for (int j = 16; j < 64; ++j) {
                uint32_t s0 = rightRotate(w[j - 15], 7) ^ rightRotate(w[j - 15], 18) ^ (w[j - 15] >> 3);
                uint32_t s1 = rightRotate(w[j - 2], 17) ^ rightRotate(w[j - 2], 19) ^ (w[j - 2] >> 10);
                w[j] = w[j - 16] + s0 + w[j - 7] + s1;
            }

            // Compression
            uint32_t a = h[0], b = h[1], c = h[2], d = h[3];
            uint32_t e = h[4], f = h[5], g = h[6], h_val = h[7]; // Renamed h to h_val to avoid conflict

            for (int j = 0; j < 64; ++j) {
                uint32_t S1 = rightRotate(e, 6) ^ rightRotate(e, 11) ^ rightRotate(e, 25);
                uint32_t ch = (e & f) ^ ((~e) & g);
                uint32_t temp1 = h_val + S1 + ch + K[j] + w[j];
                uint32_t S0 = rightRotate(a, 2) ^ rightRotate(a, 13) ^ rightRotate(a, 22);
                uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
                uint32_t temp2 = S0 + maj;

                h_val = g;
                g = f;
                f = e;
                e = d + temp1;
                d = c;
                c = b;
                b = a;
                a = temp1 + temp2;
            }

            h[0] += a;
            h[1] += b;
            h[2] += c;
            h[3] += d;
            h[4] += e;
            h[5] += f;
            h[6] += g;
            h[7] += h_val;
        }

        // Produce final hash
        std::vector<uint8_t> digest;
        digest.reserve(32);
        for (int i = 0; i < 8; ++i) {
            digest.push_back((h[i] >> 24) & 0xFF);
            digest.push_back((h[i] >> 16) & 0xFF);
            digest.push_back((h[i] >> 8) & 0xFF);
            digest.push_back(h[i] & 0xFF);
        }
        return digest;
    }
};

} // namespace nexus::utility::math
