#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <cstdint>
#include <stdexcept>
#include <locale>
#include <codecvt> // Deprecated in C++17 but alternative (icu) is heavy. 
// We will implement simple UTF8 <-> UTF32 conversion manualy or using mbstowcs if possible,
// but mbstowcs depends on locale.
// Ideally usage of `char8_t` (C++20) might help.

namespace nexus::utility::string {

/**
 * @brief Basic Unicode utilities for UTF-8 and UTF-32.
 * Does not aim to replace ICU, but covers common needs.
 */
class UnicodeHelper {
public:
    /**
     * @brief Convert UTF-8 string to UTF-32 (std::u32string).
     */
    static std::u32string utf8ToUtf32(std::string_view utf8) {
        std::u32string utf32;
        size_t i = 0;
        size_t len = utf8.length();
        
        while (i < len) {
            unsigned char c = static_cast<unsigned char>(utf8[i]);
            char32_t codepoint = 0;
            size_t seq_len = 0;

            if (c < 0x80) {
                codepoint = c;
                seq_len = 1;
            } else if ((c & 0xE0) == 0xC0) {
                if (i + 1 >= len) throw std::runtime_error("Invalid UTF-8 sequence");
                codepoint = (c & 0x1F) << 6;
                codepoint |= (static_cast<unsigned char>(utf8[i + 1]) & 0x3F);
                seq_len = 2;
            } else if ((c & 0xF0) == 0xE0) {
                if (i + 2 >= len) throw std::runtime_error("Invalid UTF-8 sequence");
                codepoint = (c & 0x0F) << 12;
                codepoint |= (static_cast<unsigned char>(utf8[i + 1]) & 0x3F) << 6;
                codepoint |= (static_cast<unsigned char>(utf8[i + 2]) & 0x3F);
                seq_len = 3;
            } else if ((c & 0xF8) == 0xF0) {
                if (i + 3 >= len) throw std::runtime_error("Invalid UTF-8 sequence");
                codepoint = (c & 0x07) << 18;
                codepoint |= (static_cast<unsigned char>(utf8[i + 1]) & 0x3F) << 12;
                codepoint |= (static_cast<unsigned char>(utf8[i + 2]) & 0x3F) << 6;
                codepoint |= (static_cast<unsigned char>(utf8[i + 3]) & 0x3F);
                seq_len = 4;
            } else {
                throw std::runtime_error("Invalid UTF-8 sequence");
            }
            utf32.push_back(codepoint);
            i += seq_len;
        }
        return utf32;
    }

    /**
     * @brief Convert UTF-32 string to UTF-8.
     */
    static std::string utf32ToUtf8(std::u32string_view utf32) {
        std::string utf8;
        for (char32_t cp : utf32) {
            if (cp < 0x80) {
                utf8.push_back(static_cast<char>(cp));
            } else if (cp < 0x800) {
                utf8.push_back(static_cast<char>(0xC0 | (cp >> 6)));
                utf8.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
            } else if (cp < 0x10000) {
                utf8.push_back(static_cast<char>(0xE0 | (cp >> 12)));
                utf8.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
                utf8.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
            } else if (cp <= 0x10FFFF) {
                utf8.push_back(static_cast<char>(0xF0 | (cp >> 18)));
                utf8.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
                utf8.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
                utf8.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
            } else {
                 // Invalid codepoint, emit replacement? Or skip.
                 // Emitting REPLACEMENT_CHARACTER U+FFFD (EF BF BD)
                 utf8.push_back(static_cast<char>(0xEF));
                 utf8.push_back(static_cast<char>(0xBF));
                 utf8.push_back(static_cast<char>(0xBD));
            }
        }
        return utf8;
    }

    /**
     * @brief Check if string is valid UTF-8.
     */
    static bool isValidUtf8(std::string_view str) {
        try {
            utf8ToUtf32(str);
            return true;
        } catch (...) {
            return false;
        }
    }

    /**
     * @brief Count code points in UTF-8 string.
     */
    static size_t codePointCount(std::string_view str) {
        // Can be optimized to just scan start bytes (0xxxxxxx or 11xxxxxx)
        size_t count = 0;
        for (unsigned char c : str) {
            // Count bytes that are NOT continuation bytes (10xxxxxx)
            // Start bytes: 0xxxxxxx (ascii) or 11xxxxxx (multi-byte start)
            if ((c & 0xC0) != 0x80) {
                count++;
            }
        }
        return count;
    }
};

} // namespace nexus::utility::string
