#pragma once

#include <string>
#include <cstddef>
#include <mutex>

namespace nexus::utility::string {

/// @brief Debug Small String Optimization (SSO) by inspecting capacity.
///
/// Most libstdc++/libc++ implementations keep strings up to ~15 characters
/// inline. We heuristically treat a string as SSO-active when its capacity
/// is within the typical inline-buffer threshold.
class SmallStringOptimizerDebug {
public:
    static constexpr size_t kTypicalSsoCapacity = 15;

    static SmallStringOptimizerDebug& instance() {
        static SmallStringOptimizerDebug inst;
        return inst;
    }

    void initialize() {
        std::lock_guard<std::mutex> lock(mutex_);
        enabled_ = true;
        sso_count_ = 0;
        heap_count_ = 0;
    }

    void shutdown() {
        std::lock_guard<std::mutex> lock(mutex_);
        enabled_ = false;
        sso_count_ = 0;
        heap_count_ = 0;
    }

    bool isEnabled() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return enabled_;
    }

    /// Guess whether SSO is active for the string and update statistics.
    bool checkSSO(const std::string& str) {
        std::lock_guard<std::mutex> lock(mutex_);
        bool sso = str.capacity() <= kTypicalSsoCapacity;
        if (sso) ++sso_count_;
        else ++heap_count_;
        return sso;
    }

    size_t getSSOCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return sso_count_;
    }

    size_t getHeapCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return heap_count_;
    }

private:
    SmallStringOptimizerDebug() = default;
    ~SmallStringOptimizerDebug() = default;
    SmallStringOptimizerDebug(const SmallStringOptimizerDebug&) = delete;
    SmallStringOptimizerDebug& operator=(const SmallStringOptimizerDebug&) = delete;

    mutable std::mutex mutex_;
    bool enabled_ = false;
    size_t sso_count_ = 0;
    size_t heap_count_ = 0;
};

} // namespace nexus::utility::string
