#pragma once

#include <cstddef>
#include <mutex>

namespace nexus::utility::string {

/// @brief Validate zero-terminated C strings within a bounded length.
class ZeroTerminatedValidator {
public:
    static ZeroTerminatedValidator& instance() {
        static ZeroTerminatedValidator inst;
        return inst;
    }

    void initialize() {
        std::lock_guard<std::mutex> lock(mutex_);
        enabled_ = true;
    }

    void shutdown() {
        std::lock_guard<std::mutex> lock(mutex_);
        enabled_ = false;
    }

    bool isEnabled() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return enabled_;
    }

    /// Returns true if a null terminator appears within [0, max_len).
    bool validate(const char* str, size_t max_len) const {
        if (str == nullptr) return false;
        for (size_t i = 0; i < max_len; ++i) {
            if (str[i] == '\0') return true;
        }
        return false;
    }

    /// Returns the index of the first '\0' in [0, max_len), or max_len if none.
    size_t findNullTerminator(const char* str, size_t max_len) const {
        if (str == nullptr) return max_len;
        for (size_t i = 0; i < max_len; ++i) {
            if (str[i] == '\0') return i;
        }
        return max_len;
    }

private:
    ZeroTerminatedValidator() = default;
    ~ZeroTerminatedValidator() = default;
    ZeroTerminatedValidator(const ZeroTerminatedValidator&) = delete;
    ZeroTerminatedValidator& operator=(const ZeroTerminatedValidator&) = delete;

    mutable std::mutex mutex_;
    bool enabled_ = false;
};

} // namespace nexus::utility::string
