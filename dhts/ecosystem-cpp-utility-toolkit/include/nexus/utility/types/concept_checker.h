#pragma once

#include <concepts>
#include <string_view>
#include <type_traits>
#include <source_location>
#include <stdexcept>
#include <nexus/utility/types/type_validator.h>

namespace nexus::utility::types {

/**
 * @brief Utilities to validate C++20/23 concepts and constraints.
 */
class ConceptChecker {
public:

    
    // Generic check helper using a prediction functor
    template<typename T, typename Predicate>
    static constexpr bool check(Predicate p) {
        return p(std::type_identity<T>{});
    }

    /**
     * @brief Asserts that T satisfies the predicate, throwing a clean error if not.
     */
    template<typename T, typename Predicate>
    static constexpr void checkAndAssert(Predicate p, std::string_view conceptName, std::source_location loc = std::source_location::current()) {
         constexpr bool result = check<T>(p);
         if constexpr (!result) {
             if (!std::is_constant_evaluated()) {
                 throw std::runtime_error(std::string("Concept violation: Type ") + 
                                          TypeValidator::getTypeName<T>() + 
                                          " does not satisfy " + std::string(conceptName));
             }
         }
    }
};

} // namespace nexus::utility::types

// Macro to wrap the concept check
// Usage: NEXUS_CONCEPT_CHECK(std::integral, int);
#define NEXUS_CONCEPT_CHECK(Concept, Type) \
    nexus::utility::types::ConceptChecker::checkAndAssert<Type>( \
        []<typename T>(std::type_identity<T>) { return Concept<T>; }, \
        #Concept)
