#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <cstring>
#include <stdexcept>

namespace nexus::utility::crypto {

/// Constant-time comparison utilities to prevent timing side-channel attacks
class ConstantTimeCompare {
public:
    /// Compare two byte sequences in constant time
    static bool equal(const uint8_t* a, const uint8_t* b, size_t length) {
        uint8_t result = 0;
        for (size_t i = 0; i < length; ++i) {
            result |= a[i] ^ b[i];
        }
        return result == 0;
    }

    /// Compare two vectors in constant time
    static bool equal(const std::vector<uint8_t>& a, const std::vector<uint8_t>& b) {
        if (a.size() != b.size()) return false;
        return equal(a.data(), b.data(), a.size());
    }

    /// Compare two strings in constant time
    static bool equal(std::string_view a, std::string_view b) {
        if (a.size() != b.size()) return false;
        return equal(reinterpret_cast<const uint8_t*>(a.data()),
                     reinterpret_cast<const uint8_t*>(b.data()), a.size());
    }

    /// Constant-time select: returns a if condition is true, b otherwise
    /// without branching on the condition
    static uint8_t select(uint8_t a, uint8_t b, bool condition) {
        uint8_t mask = static_cast<uint8_t>(-static_cast<uint8_t>(condition));
        return (a & mask) | (b & ~mask);
    }

    /// Constant-time memory comparison result: 0 if equal, nonzero otherwise
    static int memcmp(const void* a, const void* b, size_t n) {
        const auto* pa = static_cast<const uint8_t*>(a);
        const auto* pb = static_cast<const uint8_t*>(b);
        uint8_t diff = 0;
        for (size_t i = 0; i < n; ++i) {
            diff |= pa[i] ^ pb[i];
        }
        return diff;
    }

    /// Zero a memory region (guaranteed not to be optimized away)
    static void secureZero(void* ptr, size_t len) {
        volatile uint8_t* p = static_cast<volatile uint8_t*>(ptr);
        while (len--) *p++ = 0;
    }

    /// Secure zero a vector
    static void secureZero(std::vector<uint8_t>& data) {
        secureZero(data.data(), data.size());
    }
};

} // namespace nexus::utility::crypto
