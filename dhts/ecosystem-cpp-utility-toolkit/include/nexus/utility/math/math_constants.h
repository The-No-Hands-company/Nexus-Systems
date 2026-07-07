#pragma once

#include <numbers>
#include <concepts>
#include <cmath>

namespace nexus::utility::math {

/**
 * @brief Common mathematical constants and helpers.
 * Leverages C++20 standard constants where possible.
 */
class MathConstants {
public:
    // Constants
    template<typename T>
    static constexpr T pi_v = std::numbers::pi_v<T>;

    template<typename T>
    static constexpr T e_v = std::numbers::e_v<T>;

    template<typename T>
    static constexpr T sqrt2_v = std::numbers::sqrt2_v<T>;

    // Shortcuts for double
    static constexpr double PI = pi_v<double>;
    static constexpr double E = e_v<double>;
    static constexpr double SQRT2 = sqrt2_v<double>;

    // Helpers
    template<std::floating_point T>
    static constexpr T toDegrees(T radians) {
        return radians * (static_cast<T>(180) / pi_v<T>);
    }

    template<std::floating_point T>
    static constexpr T toRadians(T degrees) {
        return degrees * (pi_v<T> / static_cast<T>(180));
    }
};

} // namespace nexus::utility::math
