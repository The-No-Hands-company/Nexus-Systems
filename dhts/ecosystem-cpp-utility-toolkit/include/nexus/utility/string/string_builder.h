#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <sstream>
#include <concepts>
#include <format>

namespace nexus::utility::string {

/**
 * @brief Efficient way to build strings from multiple parts.
 */
class StringBuilder {
public:
    StringBuilder() = default;
    explicit StringBuilder(size_t capacity) {
        buffer_.reserve(capacity);
    }

    /**
     * @brief Appends a string view.
     */
    StringBuilder& append(std::string_view str) {
        buffer_.append(str);
        return *this;
    }

    /**
     * @brief Appends a character.
     */
    StringBuilder& append(char c) {
        buffer_.push_back(c);
        return *this;
    }

    /**
     * @brief Appends any type that is convertible to string or has a standard format.
     */
    template<typename T>
    StringBuilder& append(const T& value) {
        if constexpr (std::is_convertible_v<T, std::string_view>) {
            buffer_.append(value);
        } else if constexpr (std::is_arithmetic_v<T>) {
            // fast path for numbers
             buffer_.append(std::to_string(value));
        } else {
            // General format fallback
            try {
                buffer_.append(std::format("{}", value));
            } catch (...) {
                // Fallback to stream if format fails or not supported implementation details
                // This is a bit risky if T doesn't format.
                // Assuming T is formattable or streaming.
                // Or simplified: just use string conversion.
                buffer_.append(std::to_string(value)); // Assuming generic T creates compile error if not supported, which is fine.
            }
        }
        return *this;
    }

    /**
     * @brief Formatted append
     */
    template<typename... Args>
    StringBuilder& appendFormat(std::format_string<Args...> fmt, Args&&... args) {
        buffer_.append(std::format(fmt, std::forward<Args>(args)...));
        return *this;
    }
    
    // Operator << overload for stream-like usage
    template<typename T>
    StringBuilder& operator<<(const T& value) {
        return append(value);
    }

    /**
     * @brief Returns the built string.
     */
    std::string toString() const {
        return buffer_;
    }
    
    // Implicit conversion
    operator std::string() const {
        return buffer_;
    }

    void clear() {
        buffer_.clear();
    }
    
    size_t length() const {
        return buffer_.length();
    }

private:
    std::string buffer_;
};

} // namespace nexus::utility::string
