#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <cstdint>
#include <cstddef>

namespace nexus::utility::string {

/// @brief UTF-8 string validation with position reporting for invalid sequences
class Utf8Validator {
public:
    struct ValidationResult {
        bool valid = true;
        size_t code_points = 0;
        std::vector<size_t> invalid_offsets;  // Byte offsets of invalid sequences
        std::string error_message;
    };

    [[nodiscard]] static ValidationResult validate(std::string_view str) noexcept {
        ValidationResult result;

        for (size_t i = 0; i < str.size();) {
            unsigned char c = static_cast<unsigned char>(str[i]);
            size_t bytes_needed = 0;
            char32_t codepoint = 0;

            if (c <= 0x7F) {
                bytes_needed = 1;
                codepoint = c;
            } else if ((c & 0xE0) == 0xC0) {
                bytes_needed = 2;
                codepoint = c & 0x1F;
            } else if ((c & 0xF0) == 0xE0) {
                bytes_needed = 3;
                codepoint = c & 0x0F;
            } else if ((c & 0xF8) == 0xF0) {
                bytes_needed = 4;
                codepoint = c & 0x07;
            } else {
                result.valid = false;
                result.invalid_offsets.push_back(i);
                result.error_message = "Invalid UTF-8 leading byte";
                ++i;
                continue;
            }

            if (i + bytes_needed > str.size()) {
                result.valid = false;
                result.invalid_offsets.push_back(i);
                result.error_message = "Incomplete UTF-8 sequence";
                break;
            }

            // Validate continuation bytes
            for (size_t j = 1; j < bytes_needed; ++j) {
                unsigned char cont = static_cast<unsigned char>(str[i + j]);
                if ((cont & 0xC0) != 0x80) {
                    result.valid = false;
                    result.invalid_offsets.push_back(i);
                    result.error_message = "Invalid UTF-8 continuation byte";
                    i += j;  // Skip the bad sequence
                    goto next_sequence;
                }
                codepoint = (codepoint << 6) | (cont & 0x3F);
            }

            // Validate codepoint range
            if (codepoint > 0x10FFFF ||
                (codepoint >= 0xD800 && codepoint <= 0xDFFF) ||
                (bytes_needed == 2 && codepoint < 0x80) ||
                (bytes_needed == 3 && codepoint < 0x800) ||
                (bytes_needed == 4 && codepoint < 0x10000)) {
                result.valid = false;
                result.invalid_offsets.push_back(i);
                result.error_message = "Overlong encoding or invalid codepoint";
            }

            ++result.code_points;
            i += bytes_needed;
            continue;

        next_sequence:;
        }

        return result;
    }

    [[nodiscard]] static bool isValid(std::string_view str) noexcept {
        return validate(str).valid;
    }

    [[nodiscard]] static size_t codePointCount(std::string_view str) noexcept {
        return validate(str).code_points;
    }
};

} // namespace nexus::utility::string
