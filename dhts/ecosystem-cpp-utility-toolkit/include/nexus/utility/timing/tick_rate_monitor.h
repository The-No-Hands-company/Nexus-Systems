#pragma once

#include <chrono>
#include <mutex>
#include <cstddef>

namespace nexus::utility::timing {

/// @brief Monitor tick rate (ticks per second) from recorded timestamps.
class TickRateMonitor {
public:
    static TickRateMonitor& instance() {
        static TickRateMonitor inst;
        return inst;
    }

    void initialize() {
        std::lock_guard<std::mutex> lock(mutex_);
        enabled_ = true;
        tick_count_ = 0;
        has_first_ = false;
    }

    void shutdown() {
        std::lock_guard<std::mutex> lock(mutex_);
        enabled_ = false;
        tick_count_ = 0;
        has_first_ = false;
    }

    bool isEnabled() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return enabled_;
    }

    void recordTick() {
        std::lock_guard<std::mutex> lock(mutex_);
        auto now = std::chrono::steady_clock::now();
        if (!has_first_) {
            first_ = now;
            has_first_ = true;
        }
        last_ = now;
        ++tick_count_;
    }

    /// Ticks per second over the recorded window (0 if insufficient data).
    double getTickRate() const {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!has_first_ || tick_count_ < 2) return 0.0;
        auto seconds = std::chrono::duration<double>(last_ - first_).count();
        if (seconds <= 0.0) return 0.0;
        return static_cast<double>(tick_count_ - 1) / seconds;
    }

    /// Average interval between ticks.
    std::chrono::microseconds getAverageTickInterval() const {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!has_first_ || tick_count_ < 2) return std::chrono::microseconds{0};
        auto total = std::chrono::duration_cast<std::chrono::microseconds>(last_ - first_).count();
        return std::chrono::microseconds{total / static_cast<long long>(tick_count_ - 1)};
    }

    size_t getTickCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return tick_count_;
    }

private:
    TickRateMonitor() = default;
    ~TickRateMonitor() = default;
    TickRateMonitor(const TickRateMonitor&) = delete;
    TickRateMonitor& operator=(const TickRateMonitor&) = delete;

    mutable std::mutex mutex_;
    bool enabled_ = false;
    size_t tick_count_ = 0;
    bool has_first_ = false;
    std::chrono::steady_clock::time_point first_{};
    std::chrono::steady_clock::time_point last_{};
};

} // namespace nexus::utility::timing
