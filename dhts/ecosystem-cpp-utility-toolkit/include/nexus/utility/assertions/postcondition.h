#pragma once

#include <source_location>

namespace nexus::utility::assertions {

/// @brief Function postcondition validator
class Postcondition {
public:
    static void check(bool condition, const char* message,
                     std::source_location location = std::source_location::current());
};

} // namespace nexus::utility::assertions

/// @brief Postcondition check macro
#ifdef NEXUS_ENABLE_CONTRACTS
    #define NEXUS_POSTCONDITION(cond, msg) \
        nexus::utility::assertions::Postcondition::check(cond, msg)
#else
    #define NEXUS_POSTCONDITION(cond, msg) ((void)0)
#endif
