#pragma once

#include <string>
#include <string_view>
#include <optional>
#include <array>
#include <algorithm>

namespace nexus::utility::reflection {

/**
 * @brief Enum reflection using macros.
 */

// Macro to define a reflectable enum
#define NEXUS_ENUM(EnumName, ...) \
    enum class EnumName { __VA_ARGS__ }; \
    \
    namespace detail { \
        constexpr std::string_view EnumName##_names = #__VA_ARGS__; \
        \
        template<> \
        struct EnumReflector<EnumName> { \
            static constexpr std::string_view names = EnumName##_names; \
            \
            static std::string toString(EnumName value) { \
                switch (value) { \
                    NEXUS_ENUM_CASES(EnumName, __VA_ARGS__) \
                    default: return "Unknown"; \
                } \
            } \
            \
            static std::optional<EnumName> fromString(std::string_view str) { \
                NEXUS_ENUM_FROM_STRING(EnumName, __VA_ARGS__) \
                return std::nullopt; \
            } \
        }; \
    }

// Helper macros
#define NEXUS_ENUM_CASE(EnumName, value) \
    case EnumName::value: return #value;

#define NEXUS_ENUM_FROM_STRING_CASE(EnumName, value) \
    if (str == #value) return EnumName::value;

// Simple enum reflector (manual specialization)
template<typename E>
struct EnumReflector {
    static std::string toString(E value) {
        return std::to_string(static_cast<int>(value));
    }
    
    static std::optional<E> fromString(std::string_view str) {
        return std::nullopt;
    }
};

// Simplified enum reflection without complex macros
template<typename E>
class SimpleEnumReflector {
public:
    struct EnumValue {
        E value;
        std::string_view name;
    };

    template<size_t N>
    static constexpr auto makeReflector(const std::array<EnumValue, N>& values) {
        return [values](E val) -> std::string_view {
            for (const auto& ev : values) {
                if (ev.value == val) return ev.name;
            }
            return "Unknown";
        };
    }

    template<size_t N>
    static constexpr auto makeParser(const std::array<EnumValue, N>& values) {
        return [values](std::string_view name) -> std::optional<E> {
            for (const auto& ev : values) {
                if (ev.name == name) return ev.value;
            }
            return std::nullopt;
        };
    }
};

} // namespace nexus::utility::reflection

// Usage example macro
#define NEXUS_SIMPLE_ENUM(EnumName, ...) \
    enum class EnumName { __VA_ARGS__ }; \
    \
    inline std::string toString(EnumName value) { \
        using E = EnumName; \
        switch (value) { \
            NEXUS_ENUM_TO_STRING_CASES(__VA_ARGS__) \
            default: return "Unknown"; \
        } \
    } \
    \
    inline std::optional<EnumName> fromString(std::string_view str) { \
        using E = EnumName; \
        NEXUS_ENUM_FROM_STRING_CASES(__VA_ARGS__) \
        return std::nullopt; \
    }

// Helper to generate case statements
#define NEXUS_ENUM_TO_STRING_CASES(...) \
    NEXUS_ENUM_TO_STRING_IMPL(__VA_ARGS__)

#define NEXUS_ENUM_TO_STRING_IMPL(...) \
    FOR_EACH(NEXUS_ENUM_TO_STRING_CASE, __VA_ARGS__)

#define NEXUS_ENUM_TO_STRING_CASE(x) \
    case E::x: return #x;

#define NEXUS_ENUM_FROM_STRING_CASES(...) \
    FOR_EACH(NEXUS_ENUM_FROM_STRING_CASE, __VA_ARGS__)

#define NEXUS_ENUM_FROM_STRING_CASE(x) \
    if (str == #x) return E::x;

// Simple FOR_EACH implementation (limited to 10 args for simplicity)
#define FOR_EACH(macro, ...) \
    FOR_EACH_IMPL(macro, __VA_ARGS__)

#define FOR_EACH_IMPL(m, a1, ...) \
    m(a1) FOR_EACH_1(m, __VA_ARGS__)
#define FOR_EACH_1(m, a1, ...) \
    m(a1) FOR_EACH_2(m, __VA_ARGS__)
#define FOR_EACH_2(m, a1, ...) \
    m(a1) FOR_EACH_3(m, __VA_ARGS__)
#define FOR_EACH_3(m, a1, ...) \
    m(a1) FOR_EACH_4(m, __VA_ARGS__)
#define FOR_EACH_4(m, a1, ...) \
    m(a1) FOR_EACH_5(m, __VA_ARGS__)
#define FOR_EACH_5(m, a1, ...) \
    m(a1)
