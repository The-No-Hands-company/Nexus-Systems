#pragma once

#include <expected>
#ifdef NEXUS_NO_STACKTRACE
#include "nexus/utility/compiler/stacktrace_compat.h"
#else
#include <stacktrace>
#endif
#include <string>
#include <iostream>
#include <sstream>

namespace nexus::utility::error {

template <typename E>
struct UnexpectedDebug {
    E error;
    nexus::utility::stacktrace trace;

    explicit UnexpectedDebug(E e, nexus::utility::stacktrace t = nexus::utility::stacktrace::current())
        : error(std::move(e)), trace(std::move(t)) {}

    UnexpectedDebug(E e)
        : error(std::move(e)), trace(nexus::utility::stacktrace::current()) {}

    [[nodiscard]] const E& value() const noexcept { return error; }
    [[nodiscard]] E& value() noexcept { return error; }

    [[nodiscard]] const char* what() const noexcept {
        if constexpr (std::is_same_v<E, std::string>) {
            return error.c_str();
        } else {
            return "unknown error";
        }
    }
};

template <typename E>
std::ostream& operator<<(std::ostream& os, const UnexpectedDebug<E>& err) {
    os << err.error;
    return os;
}

template <typename E>
[[nodiscard]] auto make_unexpected(E&& e) {
    return std::unexpected(UnexpectedDebug<E>(std::forward<E>(e),
                                              nexus::utility::stacktrace::current()));
}

template <typename T, typename E>
using ExpectedDebug = std::expected<T, UnexpectedDebug<E>>;

/// @brief Print error from ExpectedDebug to output stream
template <typename T, typename E>
void printError(const ExpectedDebug<T, E>& result, std::ostream& os = std::cerr) {
    if (!result.has_value()) {
        const auto& err = result.error();
        os << "Error: " << err.error << "\n";
        os << "Stack trace:\n" << err.trace << "\n";
    }
}

} // namespace nexus::utility::error
