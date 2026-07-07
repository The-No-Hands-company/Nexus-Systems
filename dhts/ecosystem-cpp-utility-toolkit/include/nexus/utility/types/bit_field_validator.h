#pragma once

#include <concepts>
#include <type_traits>

namespace nexus::utility::types {

/**
 * @brief Utilities for safe bitwise operations and validations.
 */
class BitFieldValidator {
public:
    /**
     * @brief Checks if a set of bits (mask) only contains bits present in allowed_mask.
     */
    template<std::integral T>
    static constexpr bool validateMask(T mask, T allowed_mask) noexcept {
        return (mask & ~allowed_mask) == 0;
    }

    /**
     * @brief Type-safe check if a flag is set.
     */
    template<std::integral T>
    static constexpr bool hasFlag(T value, T flag) noexcept {
        return (value & flag) == flag;
    }
    
    /**
     * @brief Type-safe check if ANY flag from the set is set.
     */
    template<std::integral T>
    static constexpr bool hasAnyFlag(T value, T flags) noexcept {
        return (value & flags) != 0;
    }

    /**
     * @brief Checks if a type is safe to be trivially copied (POD-like for bit blasting).
     * Useful validation before doing memcpy or bit manipulation on structs.
     */
    template<typename T>
    static consteval bool isBitwiseSafe() {
        return std::is_trivially_copyable_v<T>;
    }
};

} // namespace nexus::utility::types
