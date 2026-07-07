#pragma once

#include <source_location>
#ifdef NEXUS_NO_STACKTRACE
#include "nexus/utility/compiler/stacktrace_compat.h"
#else
#include <stacktrace>
#endif
#include <iostream>
#include <sstream>
#include <functional>
#include <string>

#include <nexus/utility/assertions/contract_violation_handler.h>

namespace nexus::utility::assertions {

/// @brief Callback type for assertion failures
using AssertionHandler = std::function<void(const char* expr, const char* msg, const std::source_location& loc, const nexus::utility::stacktrace& trace)>;

/// @brief Enhanced assertion with stack trace, context, and custom handlers
class EnhancedAssert {
public:
    /// @brief Trigger an assertion failure
    /// Delegates to ContractViolationHandler with type "ASSERTION FAILED"
    static void assertFailed(const char* expression,
                            const char* message,
                            std::source_location location = std::source_location::current());

    /// @brief Set a custom assertion handler
    /// NOTE: This internally sets the ContractViolationHandler callback.
    static void setHandler(AssertionHandler handler);

    /// @brief Reset to default handler
    static void resetHandler();
};

} // namespace nexus::utility::assertions

/// @brief Enhanced assertion macro with stack trace
/// Define NEXUS_ENABLE_ASSERTS or NEXUS_DEBUG to enable
#if defined(NEXUS_ENABLE_ASSERTS) || defined(NEXUS_DEBUG) || !defined(NDEBUG)
    #define NEXUS_ASSERT(expr, msg) \
        do { \
            if (false == (static_cast<bool>(expr))) { \
                nexus::utility::assertions::EnhancedAssert::assertFailed(#expr, msg); \
            } \
        } while(0)
#else
    #define NEXUS_ASSERT(expr, msg) ((void)0)
#endif

/// @brief Always-on assertion (even in release builds)
#define NEXUS_VERIFY(expr, msg) \
    do { \
        if (false == (static_cast<bool>(expr))) { \
            nexus::utility::assertions::EnhancedAssert::assertFailed(#expr, msg); \
        } \
    } while(0)
