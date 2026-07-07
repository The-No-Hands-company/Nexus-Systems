#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <stdexcept>

namespace nexus::utility::encoding {

/**
 * @brief Base64 encoding and decoding utilities.
 */
class Base64Encoder {
public:
    /**
     * @brief Encodes binary data to Base64 string.
     */
    [[nodiscard]] static std::string encode(const std::vector<uint8_t>& data) {
        return encode(data.data(), data.size());
    }

    [[nodiscard]] static std::string encode(const uint8_t* data, size_t len) {
        static const char* base64_chars = 
            "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
            "abcdefghijklmnopqrstuvwxyz"
            "0123456789+/";

        std::string result;
        result.reserve(((len + 2) / 3) * 4);

        for (size_t i = 0; i < len; i += 3) {
            uint32_t triple = (static_cast<uint32_t>(data[i]) << 16);
            
            if (i + 1 < len) triple |= (static_cast<uint32_t>(data[i + 1]) << 8);
            if (i + 2 < len) triple |= static_cast<uint32_t>(data[i + 2]);

            result += base64_chars[(triple >> 18) & 0x3F];
            result += base64_chars[(triple >> 12) & 0x3F];
            result += (i + 1 < len) ? base64_chars[(triple >> 6) & 0x3F] : '=';
            result += (i + 2 < len) ? base64_chars[triple & 0x3F] : '=';
        }

        return result;
    }

    [[nodiscard]] static std::string encode(const std::string& str) {
        return encode(reinterpret_cast<const uint8_t*>(str.data()), str.size());
    }

    /**
     * @brief Decodes Base64 string to binary data.
     */
    [[nodiscard]] static std::vector<uint8_t> decode(const std::string& encoded) {
        static const int decode_table[256] = {
            -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
            -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
            -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,62,-1,-1,-1,63,
            52,53,54,55,56,57,58,59,60,61,-1,-1,-1,-1,-1,-1,
            -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,
            15,16,17,18,19,20,21,22,23,24,25,-1,-1,-1,-1,-1,
            -1,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,
            41,42,43,44,45,46,47,48,49,50,51,-1,-1,-1,-1,-1,
            -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
            -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
            -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
            -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
            -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
            -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
            -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
            -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1
        };

        std::vector<uint8_t> result;
        result.reserve((encoded.size() / 4) * 3);

        uint32_t buffer = 0;
        int bits = 0;

        for (char c : encoded) {
            if (c == '=') break;
            
            int val = decode_table[static_cast<unsigned char>(c)];
            if (val == -1) {
                throw std::invalid_argument("Invalid Base64 character");
            }

            buffer = (buffer << 6) | val;
            bits += 6;

            if (bits >= 8) {
                bits -= 8;
                result.push_back(static_cast<uint8_t>((buffer >> bits) & 0xFF));
            }
        }

        return result;
    }
};

} // namespace nexus::utility::encoding
