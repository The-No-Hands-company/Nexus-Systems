#pragma once

#include <source_location>

namespace nexus::utility::assertions {

/// @brief Class invariant validator
class InvariantChecker {
public:
    static void check(bool condition, const char* message,
                     std::source_location location = std::source_location::current());
};

} // namespace nexus::utility::assertions

/// @brief Invariant check macro
#ifdef NEXUS_ENABLE_CONTRACTS
    #define NEXUS_CHECK_INVARIANT(cond, msg) \
        nexus::utility::assertions::InvariantChecker::check(cond, msg)
#else
    #define NEXUS_CHECK_INVARIANT(cond, msg) ((void)0)
#endif
