#pragma once

#include <concepts>
#include <stdexcept>
#include <string>
#include <format>
#include <source_location>
#include <nexus/utility/types/type_validator.h>

namespace nexus::utility::types {

/**
 * @brief Utilities for validating that values fall within specific ranges.
 */
class RangeValidator {
public:
    /**
     * @brief Checks if a value is within [min, max] inclusive.
     */
    template<typename T>
    requires std::totally_ordered<T>
    static constexpr bool checkRange(const T& value, const T& min, const T& max) noexcept {
        return value >= min && value <= max;
    }

    /**
     * @brief Checks if a value is within [min, max], throwing if not.
     */
    template<typename T>
    requires std::totally_ordered<T>
    static void validateRange(const T& value, const T& min, const T& max, 
                            std::string_view name = "Value",
                            std::source_location loc = std::source_location::current()) {
        if (!checkRange(value, min, max)) {
            // Using std::to_string logic or format if available.
            // Simplified error message construction.
            throw std::out_of_range(std::string(name) + " out of range in " + 
                                  std::string(loc.function_name()));
        }
    }

    /**
     * @brief Clamps a value to [min, max], returning the clamped value.
     *        If throws_on_clamp is true, throws instead of clamping.
     */
    template<bool ThrowsOnClamp = false, typename T>
    requires std::totally_ordered<T>
    static constexpr T clamp(const T& value, const T& min, const T& max, 
                           std::string_view name = "Value",
                           std::source_location loc = std::source_location::current()) {
        if (value < min) {
            if constexpr (ThrowsOnClamp) {
                validateRange(value, min, max, name, loc);
            }
            return min;
        }
        if (value > max) {
            if constexpr (ThrowsOnClamp) {
                validateRange(value, min, max, name, loc);
            }
            return max;
        }
        return value;
    }

    // Specific validators
    
    template<typename T>
    requires std::floating_point<T>
    static void validateProbability(T value, std::string_view name = "Probability", 
                                  std::source_location loc = std::source_location::current()) {
        validateRange(value, static_cast<T>(0.0), static_cast<T>(1.0), name, loc);
    }
};

} // namespace nexus::utility::types
