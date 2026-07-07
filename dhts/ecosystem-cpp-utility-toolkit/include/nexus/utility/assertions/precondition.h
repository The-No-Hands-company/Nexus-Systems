#pragma once

#include <source_location>

namespace nexus::utility::assertions {

/// @brief Function precondition validator (Design by Contract)
class Precondition {
public:
    static void check(bool condition, const char* message,
                     std::source_location location = std::source_location::current());
};


} // namespace nexus::utility::assertions

/// @brief Precondition check macro
#ifdef NEXUS_ENABLE_CONTRACTS
    #define NEXUS_PRECONDITION(cond, msg) \
        nexus::utility::assertions::Precondition::check(cond, msg)
#else
    #define NEXUS_PRECONDITION(cond, msg) ((void)0)
#endif
