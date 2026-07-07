#pragma once

#include <chrono>
#include <mutex>
#include <cstddef>

namespace nexus::utility::timing {

/// @brief Validate steady-clock behavior: monotonicity, drift, tick count.
class ClockValidator {
public:
    static ClockValidator& instance() {
        static ClockValidator inst;
        return inst;
    }

    void initialize() {
        std::lock_guard<std::mutex> lock(mutex_);
        enabled_ = true;
        tick_count_ = 0;
        violations_ = 0;
        has_last_ = false;
    }

    void shutdown() {
        std::lock_guard<std::mutex> lock(mutex_);
        enabled_ = false;
        tick_count_ = 0;
        violations_ = 0;
        has_last_ = false;
    }

    bool isEnabled() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return enabled_;
    }

    /// Record a clock reading. Detects backward jumps.
    void recordTick() {
        std::lock_guard<std::mutex> lock(mutex_);
        auto now = std::chrono::steady_clock::now();
        if (has_last_) {
            if (now < last_) ++violations_;
        } else {
            first_ = now;
            has_last_ = true;
        }
        last_ = now;
        ++tick_count_;
    }

    /// Returns true if the clock never went backward across recorded ticks.
    bool checkMonotonic() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return violations_ == 0;
    }

    size_t getTickCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return tick_count_;
    }

    /// Estimated drift in parts-per-million relative to expected tick interval.
    /// Returns 0 without a configured expected interval or insufficient data.
    double getDriftPPM() const {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!has_last_ || tick_count_ < 2 || expected_interval_.count() == 0) {
            return 0.0;
        }
        auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(last_ - first_).count();
        auto expected = expected_interval_.count() * static_cast<long long>(tick_count_ - 1);
        if (expected == 0) return 0.0;
        double drift = static_cast<double>(elapsed - expected) / static_cast<double>(expected);
        return drift * 1'000'000.0;
    }

    /// Configure the nominal interval between ticks for drift estimation.
    void setExpectedInterval(std::chrono::nanoseconds interval) {
        std::lock_guard<std::mutex> lock(mutex_);
        expected_interval_ = interval;
    }

private:
    ClockValidator() = default;
    ~ClockValidator() = default;
    ClockValidator(const ClockValidator&) = delete;
    ClockValidator& operator=(const ClockValidator&) = delete;

    mutable std::mutex mutex_;
    bool enabled_ = false;
    size_t tick_count_ = 0;
    size_t violations_ = 0;
    bool has_last_ = false;
    std::chrono::steady_clock::time_point first_{};
    std::chrono::steady_clock::time_point last_{};
    std::chrono::nanoseconds expected_interval_{0};
};

} // namespace nexus::utility::timing
