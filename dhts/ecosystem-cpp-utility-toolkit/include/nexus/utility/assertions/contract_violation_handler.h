#pragma once

#include <functional>
#include <source_location>
#ifdef NEXUS_NO_STACKTRACE
#include "nexus/utility/compiler/stacktrace_compat.h"
#else
#include <stacktrace>
#endif
#include <stdexcept>

namespace nexus::utility::assertions {

enum class ViolationPolicy {
    Abort,
    Throw,
    LogAndContinue
};

/// @brief Exception thrown on contract violation if Policy::Throw is selected
class ContractViolationException : public std::logic_error {
public:
    using std::logic_error::logic_error;
};

/// @brief Central handler for contract violations
class ContractViolationHandler {
public:
    /// @brief Set the global violation policy
    static void setPolicy(ViolationPolicy policy) noexcept;

    /// @brief Get the current policy
    static ViolationPolicy getPolicy() noexcept;

    /// @brief Set a custom callback for violations
    using ViolationCallback = std::function<void(const char* type, const char* expr, const char* msg, 
                                               const std::source_location& loc, const nexus::utility::stacktrace& trace)>;
    static void setCallback(ViolationCallback callback);

    /// @brief Handle a violation according to current policy
    static void handleViolation(const char* type, const char* expr, const char* msg, 
                               std::source_location loc = std::source_location::current(),
                               nexus::utility::stacktrace trace = nexus::utility::stacktrace::current());

private:
    // Internal state
    static ViolationPolicy policy_;
    static ViolationCallback callback_;
};

} // namespace nexus::utility::assertions
