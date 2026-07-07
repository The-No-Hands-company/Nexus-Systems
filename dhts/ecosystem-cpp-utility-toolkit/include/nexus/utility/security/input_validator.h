#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <algorithm>
#include <cstring>
#include <type_traits>

namespace nexus::utility::security {

/// @brief Secure input validation and sanitization utilities
class InputValidator {
public:
    /// @brief Check if string contains only alphanumeric characters
    [[nodiscard]] static bool isAlphanumeric(std::string_view str) noexcept {
        return std::all_of(str.begin(), str.end(), [](char c) {
            return std::isalnum(static_cast<unsigned char>(c)) != 0;
        });
    }

    /// @brief Check if string contains only printable ASCII characters
    [[nodiscard]] static bool isPrintable(std::string_view str) noexcept {
        return std::all_of(str.begin(), str.end(), [](char c) {
            unsigned char uc = static_cast<unsigned char>(c);
            return uc >= 0x20 && uc <= 0x7E;
        });
    }

    /// @brief Check if string is empty or exceeds maximum length
    [[nodiscard]] static bool isValidLength(std::string_view str, size_t max_length) noexcept {
        return !str.empty() && str.size() <= max_length;
    }

    /// @brief Validate numeric range
    template <typename T>
    [[nodiscard]] static bool isInRange(T value, T min_val, T max_val) noexcept {
        static_assert(std::is_arithmetic_v<T>);
        return value >= min_val && value <= max_val;
    }

    /// @brief Check for null bytes in string (potential truncation attack)
    [[nodiscard]] static bool hasNullBytes(std::string_view str) noexcept {
        return str.find('\0') != std::string_view::npos;
    }

    /// @brief Sanitize string by removing null bytes
    [[nodiscard]] static std::string removeNullBytes(std::string_view str) {
        std::string result;
        result.reserve(str.size());
        for (char c : str) {
            if (c != '\0') {
                result.push_back(c);
            }
        }
        return result;
    }

    /// @brief Check for path traversal patterns
    [[nodiscard]] static bool hasPathTraversal(std::string_view str) noexcept {
        return str.find("..") != std::string_view::npos ||
               str.find("~") != std::string_view::npos ||
               str.find("$") != std::string_view::npos;
    }

    /// @brief Validate email format (basic check)
    [[nodiscard]] static bool isValidEmail(std::string_view email) noexcept {
        auto at_pos = email.find('@');
        if (at_pos == std::string_view::npos || at_pos == 0 || at_pos == email.size() - 1) {
            return false;
        }
        auto dot_pos = email.find('.', at_pos);
        return dot_pos != std::string_view::npos && dot_pos > at_pos + 1 && dot_pos < email.size() - 1;
    }

    /// @brief Validate URL format (basic check)
    [[nodiscard]] static bool isValidUrl(std::string_view url) noexcept {
        return url.starts_with("http://") || url.starts_with("https://") ||
               url.starts_with("ftp://") || url.starts_with("ws://") ||
               url.starts_with("wss://");
    }

    /// @brief Validate IP address format (basic IPv4 check)
    [[nodiscard]] static bool isValidIPv4(std::string_view ip) noexcept {
        int dots = 0;
        int digits = 0;
        int segment = 0;

        for (size_t i = 0; i < ip.size(); ++i) {
            char c = ip[i];
            if (c == '.') {
                if (digits == 0 || segment > 255) return false;
                ++dots;
                digits = 0;
                segment = 0;
            } else if (c >= '0' && c <= '9') {
                segment = segment * 10 + (c - '0');
                ++digits;
                if (segment > 255 || digits > 3) return false;
            } else {
                return false;
            }
        }

        return dots == 3 && digits > 0 && segment <= 255;
    }
};

/// @brief Secure memory wiping (prevents compiler optimization)
inline void secureZeroMemory(void* ptr, size_t size) noexcept {
    volatile auto* p = static_cast<volatile unsigned char*>(ptr);
    while (size--) {
        *p++ = 0;
    }
}

/// @brief Secure string wiping
inline void secureZeroString(std::string& str) noexcept {
    if (!str.empty()) {
        secureZeroMemory(str.data(), str.size());
        str.clear();
    }
}

/// @brief Constant-time comparison to prevent timing attacks
[[nodiscard]] inline bool constantTimeCompare(std::string_view a, std::string_view b) noexcept {
    if (a.size() != b.size()) {
        return false;
    }

    volatile unsigned char result = 0;
    for (size_t i = 0; i < a.size(); ++i) {
        result |= static_cast<unsigned char>(a[i] ^ b[i]);
    }
    return result == 0;
}

/// @brief Constant-time byte comparison
[[nodiscard]] inline bool constantTimeCompare(const void* a, const void* b, size_t size) noexcept {
    const auto* pa = static_cast<const volatile unsigned char*>(a);
    const auto* pb = static_cast<const volatile unsigned char*>(b);
    volatile unsigned char result = 0;

    for (size_t i = 0; i < size; ++i) {
        result |= pa[i] ^ pb[i];
    }
    return result == 0;
}

} // namespace nexus::utility::security
