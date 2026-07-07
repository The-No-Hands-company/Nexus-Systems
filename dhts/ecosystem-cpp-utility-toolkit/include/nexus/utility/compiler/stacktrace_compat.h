#pragma once

#ifdef NEXUS_NO_STACKTRACE

#include <string>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <ostream>

namespace nexus::utility {

struct stacktrace_entry {
    [[nodiscard]] std::string description() const { return "<unavailable>"; }
    [[nodiscard]] std::string source_file() const { return ""; }
    [[nodiscard]] int source_line() const { return 0; }
    [[nodiscard]] uintptr_t native_handle() const noexcept { return 0; }
};

inline std::ostream& operator<<(std::ostream& os, const stacktrace_entry&) {
    return os << "  <stacktrace unavailable>";
}

class stacktrace {
public:
    using value_type = stacktrace_entry;

    [[nodiscard]] static stacktrace current(int = 0, size_t = 64) noexcept { return {}; }
    [[nodiscard]] static stacktrace current(const std::allocator<stacktrace_entry>&, int = 0, size_t = 64) noexcept { return {}; }
    [[nodiscard]] size_t size() const noexcept { return 0; }
    [[nodiscard]] bool empty() const noexcept { return true; }
    [[nodiscard]] const stacktrace_entry& operator[](size_t) const {
        static stacktrace_entry e;
        return e;
    }
    [[nodiscard]] const stacktrace_entry* begin() const noexcept { return nullptr; }
    [[nodiscard]] const stacktrace_entry* end() const noexcept { return nullptr; }
};

inline std::ostream& operator<<(std::ostream& os, const stacktrace&) {
    return os << "  <stacktrace unavailable>";
}

} // namespace nexus::utility

#else
#include <stacktrace>
namespace nexus::utility {
    using stacktrace = std::stacktrace;
    using stacktrace_entry = std::stacktrace_entry;
}
#endif
