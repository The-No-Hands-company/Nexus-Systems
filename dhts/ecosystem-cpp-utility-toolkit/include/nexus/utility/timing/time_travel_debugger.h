#pragma once

#include <string>
#include <vector>
#include <chrono>
#include <atomic>
#include <mutex>
#include <cstddef>

namespace nexus::utility::timing {

/// @brief Debug time-travel scenarios via a virtual clock offset.
class TimeTravelDebugger {
public:
    static TimeTravelDebugger& instance() {
        static TimeTravelDebugger inst;
        return inst;
    }

    void initialize() {
        std::lock_guard<std::mutex> lock(mutex_);
        enabled_ = true;
        offset_.store(0, std::memory_order_relaxed);
        reasons_.clear();
    }

    void shutdown() {
        std::lock_guard<std::mutex> lock(mutex_);
        enabled_ = false;
        offset_.store(0, std::memory_order_relaxed);
        reasons_.clear();
    }

    bool isEnabled() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return enabled_;
    }

    /// Set the virtual time offset (may be negative for the past).
    void setTimeOffset(std::chrono::milliseconds offset) {
        offset_.store(offset.count(), std::memory_order_relaxed);
    }

    /// Current wall clock adjusted by the virtual offset.
    std::chrono::system_clock::time_point getVirtualNow() const {
        auto off = std::chrono::milliseconds{offset_.load(std::memory_order_relaxed)};
        return std::chrono::system_clock::now() + off;
    }

    void recordTimeTravel(const std::string& reason) {
        std::lock_guard<std::mutex> lock(mutex_);
        reasons_.push_back(reason);
    }

    size_t getTravelCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return reasons_.size();
    }

    std::chrono::milliseconds getTimeOffset() const {
        return std::chrono::milliseconds{offset_.load(std::memory_order_relaxed)};
    }

private:
    TimeTravelDebugger() = default;
    ~TimeTravelDebugger() = default;
    TimeTravelDebugger(const TimeTravelDebugger&) = delete;
    TimeTravelDebugger& operator=(const TimeTravelDebugger&) = delete;

    mutable std::mutex mutex_;
    bool enabled_ = false;
    std::atomic<long long> offset_{0};
    std::vector<std::string> reasons_;
};

} // namespace nexus::utility::timing
