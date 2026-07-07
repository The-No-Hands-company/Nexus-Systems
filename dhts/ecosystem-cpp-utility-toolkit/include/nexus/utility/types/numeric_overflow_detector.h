#pragma once

#include <concepts>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <source_location>
#include <format>
#include <type_traits>

namespace nexus::utility::types {

namespace detail {

    template<typename T>
    concept Numeric = std::is_integral_v<T>; // Extend to floats if needed, but overflow is different there

} // namespace detail

/**
 * @brief Provides checked arithmetic operations that detect overflows.
 */
class NumericOverflowDetector {
public:
    // This class is now a collection of static methods or just a namespace holder.
    // Keeping the class name for compatibility with existing file structure naming.
    // But ideally should be a namespace. I will keep it as a static interface.

    /**
     * @brief Adds two numbers, returning std::nullopt on overflow.
     */
    template<detail::Numeric T>
    [[nodiscard]] static constexpr std::optional<T> add(T a, T b) noexcept {
#if defined(__GNUC__) || defined(__clang__)
        T result;
        if (__builtin_add_overflow(a, b, &result)) {
            return std::nullopt;
        }
        return result;
#else
        // Fallback for generic C++
        if (b > 0 && a > std::numeric_limits<T>::max() - b) return std::nullopt;
        if (b < 0 && a < std::numeric_limits<T>::min() - b) return std::nullopt;
        return a + b;
#endif
    }

    /**
     * @brief Subtracts two numbers, returning std::nullopt on overflow.
     */
    template<detail::Numeric T>
    [[nodiscard]] static constexpr std::optional<T> sub(T a, T b) noexcept {
#if defined(__GNUC__) || defined(__clang__)
        T result;
        if (__builtin_sub_overflow(a, b, &result)) {
            return std::nullopt;
        }
        return result;
#else
        if (b < 0 && a > std::numeric_limits<T>::max() + b) return std::nullopt;
        if (b > 0 && a < std::numeric_limits<T>::min() + b) return std::nullopt;
        return a - b;
#endif
    }

    /**
     * @brief Multiplies two numbers, returning std::nullopt on overflow.
     */
    template<detail::Numeric T>
    [[nodiscard]] static constexpr std::optional<T> mul(T a, T b) noexcept {
#if defined(__GNUC__) || defined(__clang__)
        T result;
        if (__builtin_mul_overflow(a, b, &result)) {
            return std::nullopt;
        }
        return result;
#else
       if (a == 0 || b == 0) return 0;
       if (a > 0 && b > 0 && a > std::numeric_limits<T>::max() / b) return std::nullopt;
       if (a > 0 && b < 0 && b < std::numeric_limits<T>::min() / a) return std::nullopt;
       if (a < 0 && b > 0 && a < std::numeric_limits<T>::min() / b) return std::nullopt;
       if (a < 0 && b < 0 && -a > std::numeric_limits<T>::max() / -b) return std::nullopt; // Caution with min() negation
       return a * b;
#endif
    }

    /**
     * @brief Divides two numbers, returning std::nullopt on overflow or divide-by-zero.
     */
    template<detail::Numeric T>
    [[nodiscard]] static constexpr std::optional<T> div(T a, T b) noexcept {
        if (b == 0) return std::nullopt;
        // The only overflow in division is INT_MIN / -1
        if constexpr (std::is_signed_v<T>) {
            if (a == std::numeric_limits<T>::min() && b == -1) {
                return std::nullopt;
            }
        }
        return a / b;
    }
};

/**
 * @brief An exception thrown when an arithmetic overflow occurs.
 */
class ArithmeticOverflowException : public std::runtime_error {
public:
    explicit ArithmeticOverflowException(const std::string& msg, 
                                        std::source_location loc = std::source_location::current())
        : std::runtime_error(msg) // + formatting with loc if desired
    {}
};

/**
 * @brief A wrapper around an integer that checks for overflow on all operations.
 */
template<detail::Numeric T>
class SafeInt {
public:
    constexpr SafeInt() : value_(0) {}
    constexpr SafeInt(T val) : value_(val) {}

    constexpr SafeInt& operator+=(const SafeInt& other) {
        auto res = NumericOverflowDetector::add(value_, other.value_);
        if (!res) throw ArithmeticOverflowException("Addition overflow");
        value_ = *res;
        return *this;
    }

    constexpr SafeInt& operator-=(const SafeInt& other) {
        auto res = NumericOverflowDetector::sub(value_, other.value_);
        if (!res) throw ArithmeticOverflowException("Subtraction overflow");
        value_ = *res;
        return *this;
    }

    constexpr SafeInt& operator*=(const SafeInt& other) {
        auto res = NumericOverflowDetector::mul(value_, other.value_);
        if (!res) throw ArithmeticOverflowException("Multiplication overflow");
        value_ = *res;
        return *this;
    }

    constexpr SafeInt& operator/=(const SafeInt& other) {
        auto res = NumericOverflowDetector::div(value_, other.value_);
        if (!res) throw ArithmeticOverflowException("Division error (overflow or zero)");
        value_ = *res;
        return *this;
    }

    constexpr T value() const { return value_; }

    // friends
    friend constexpr SafeInt operator+(SafeInt lhs, const SafeInt& rhs) {
        lhs += rhs;
        return lhs;
    }
    friend constexpr SafeInt operator-(SafeInt lhs, const SafeInt& rhs) {
        lhs -= rhs;
        return lhs;
    }
    friend constexpr SafeInt operator*(SafeInt lhs, const SafeInt& rhs) {
        lhs *= rhs;
        return lhs;
    }
    friend constexpr SafeInt operator/(SafeInt lhs, const SafeInt& rhs) {
        lhs /= rhs;
        return lhs;
    }

    auto operator<=>(const SafeInt&) const = default;

private:
    T value_;
};

} // namespace nexus::utility::types
