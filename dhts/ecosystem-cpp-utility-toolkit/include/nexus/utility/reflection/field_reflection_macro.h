#pragma once

#include <string_view>
#include <tuple>
#include <utility>

namespace nexus::utility::reflection {

/**
 * @brief Macro-based field reflection for structs.
 */

// Macro to define a reflectable struct
#define NEXUS_REFLECTABLE_STRUCT(StructName, ...) \
    struct StructName { \
        __VA_ARGS__ \
        \
        static constexpr size_t field_count() { \
            return NEXUS_COUNT_FIELDS(__VA_ARGS__); \
        } \
        \
        auto asTuple() { \
            return std::tie(NEXUS_FIELD_NAMES(__VA_ARGS__)); \
        } \
        \
        auto asTuple() const { \
            return std::tie(NEXUS_FIELD_NAMES(__VA_ARGS__)); \
        } \
    };

// Helper macros (simplified implementation)
#define NEXUS_COUNT_FIELDS(...) \
    NEXUS_NARG(__VA_ARGS__)

#define NEXUS_FIELD_NAMES(...) \
    NEXUS_EXTRACT_NAMES(__VA_ARGS__)

// Count arguments (up to 10 for simplicity)
#define NEXUS_NARG(...) \
    NEXUS_NARG_(__VA_ARGS__, NEXUS_RSEQ_N())

#define NEXUS_NARG_(...) \
    NEXUS_ARG_N(__VA_ARGS__)

#define NEXUS_ARG_N(_1,_2,_3,_4,_5,_6,_7,_8,_9,_10,N,...) N

#define NEXUS_RSEQ_N() 10,9,8,7,6,5,4,3,2,1,0

// Extract field names from declarations
#define NEXUS_EXTRACT_NAMES(...) \
    NEXUS_EXTRACT_NAMES_IMPL(__VA_ARGS__)

#define NEXUS_EXTRACT_NAMES_IMPL(...) \
    NEXUS_FOR_EACH_FIELD(NEXUS_EXTRACT_NAME, __VA_ARGS__)

#define NEXUS_EXTRACT_NAME(type_and_name) \
    NEXUS_GET_NAME(type_and_name)

#define NEXUS_GET_NAME(type_and_name) \
    NEXUS_SECOND type_and_name

#define NEXUS_SECOND(a, b) b

// Simple field reflection helper
template<typename T>
struct FieldReflection {
    static constexpr size_t field_count = 0;
    
    template<size_t I>
    static constexpr std::string_view field_name() {
        static_assert(I < field_count, "Field index out of range");
        return "";
    }
};

// Simplified approach: use structured bindings count
template<typename T>
constexpr size_t count_fields() {
    if constexpr (std::is_aggregate_v<T>) {
        // This is a simplified version - full implementation would use
        // template metaprogramming to detect field count
        return 0;
    }
    return 0;
}

} // namespace nexus::utility::reflection

// Simpler macro for basic reflection
#define NEXUS_REFLECT_STRUCT(Name, ...) \
    struct Name { \
        __VA_ARGS__ \
    }; \
    \
    template<> \
    struct nexus::utility::reflection::FieldReflection<Name> { \
        static constexpr size_t field_count = NEXUS_COUNT_ARGS(__VA_ARGS__); \
    };

#define NEXUS_COUNT_ARGS(...) \
    (sizeof((int[]){0, ((void)(__VA_ARGS__), 0)...}) / sizeof(int) - 1)
