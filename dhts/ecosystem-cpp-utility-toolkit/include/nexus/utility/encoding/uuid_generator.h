#pragma once

#include <string>
#include <sstream>
#include <iomanip>
#include <random>
#include <array>

namespace nexus::utility::encoding {

/// @brief UUID (Version 4) generator per RFC 4122 using cryptographic randomness
class UUIDGenerator {
public:
    /// @brief Generates a cryptographically random UUID v4.
    /// Format: xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx
    /// where x is any hex digit, y is one of 8, 9, A, or B.
    [[nodiscard]] static std::string generate() {
        std::array<uint8_t, 16> bytes{};
        fillRandom(bytes.data(), bytes.size());

        // Set version to 4 (UUIDv4)
        bytes[6] = (bytes[6] & 0x0F) | 0x40;
        // Set variant to RFC 4122
        bytes[8] = (bytes[8] & 0x3F) | 0x80;

        std::ostringstream oss;
        oss << std::hex << std::setfill('0');

        for (size_t i = 0; i < 16; ++i) {
            if (i == 4 || i == 6 || i == 8 || i == 10) {
                oss << '-';
            }
            oss << std::setw(2) << static_cast<int>(bytes[i]);
        }

        return oss.str();
    }

private:
    static void fillRandom(uint8_t* data, size_t size) {
        static thread_local std::random_device rd;
        static thread_local std::mt19937_64 gen(rd());
        std::uniform_int_distribution<int> dist(0, 255);

        for (size_t i = 0; i < size; ++i) {
            data[i] = static_cast<uint8_t>(dist(gen));
        }
    }
};

} // namespace nexus::utility::encoding
