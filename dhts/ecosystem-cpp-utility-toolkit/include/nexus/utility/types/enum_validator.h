#pragma once

#include <type_traits>
#include <optional>
#include <string_view>
#include <concepts>
#include <algorithm>

namespace nexus::utility::types {

/**
 * @brief Traits class to define enum ranges.
 * Fully specialize this for your enum types.
 */
template<typename E>
struct EnumTraits {
    static constexpr bool is_specialized = false;
};

/**
 * @brief Validator for enum values.
 */
class EnumValidator {
public:
    /**
     * @brief Checks if the integer value is valid for the enum E.
     * Requires EnumTraits<E> to be specialized.
     */
    template<typename E>
    requires std::is_enum_v<E>
    static constexpr bool isValid(std::underlying_type_t<E> value) {
        if constexpr (EnumTraits<E>::is_specialized) {
            return EnumTraits<E>::isValid(value);
        } else {
            // Default: Cannot validate without traits, assume unsafe/false or compile error?
            // Let's force specialization for safety.
            static_assert(EnumTraits<E>::is_specialized, 
                "EnumTraits not specialized for this type. Use NEXUS_DECLARE_ENUM_RANGE.");
            return false;
        }
    }

    /**
     * @brief Safe cast from integer to Enum.
     */
    template<typename E>
    requires std::is_enum_v<E>
    static constexpr std::optional<E> toEnum(std::underlying_type_t<E> value) {
        if (isValid<E>(value)) {
            return static_cast<E>(value);
        }
        return std::nullopt;
    }
};

} // namespace nexus::utility::types

/**
 * @brief Macro to declare a contiguous enum range [Min, Max].
 * Must be used in global namespace or where EnumTraits is visible (generic).
 * Actually, must specialize nexus::utility::types::EnumTraits.
 */
#define NEXUS_DECLARE_ENUM_RANGE(EnumType, MinVal, MaxVal) \
template<> \
struct nexus::utility::types::EnumTraits<EnumType> { \
    static constexpr bool is_specialized = true; \
    static constexpr bool isValid(std::underlying_type_t<EnumType> val) { \
        return val >= static_cast<std::underlying_type_t<EnumType>>(MinVal) && \
               val <= static_cast<std::underlying_type_t<EnumType>>(MaxVal); \
    } \
};
