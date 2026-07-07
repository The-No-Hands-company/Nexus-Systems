#pragma once

#include <chrono>
#include <mutex>
#include <cstddef>

namespace nexus::utility::timing {

/// @brief Verify a clock's monotonicity by tracking successive readings.
class MonotonicClockChecker {
public:
    static MonotonicClockChecker& instance() {
        static MonotonicClockChecker inst;
        return inst;
    }

    void initialize() {
        std::lock_guard<std::mutex> lock(mutex_);
        enabled_ = true;
        violations_ = 0;
        has_last_ = false;
    }

    void shutdown() {
        std::lock_guard<std::mutex> lock(mutex_);
        enabled_ = false;
        violations_ = 0;
        has_last_ = false;
    }

    bool isEnabled() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return enabled_;
    }

    /// Record a time point; counts a violation if it precedes the previous one.
    void recordTime(std::chrono::steady_clock::time_point t) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (has_last_ && t < last_) ++violations_;
        last_ = t;
        has_last_ = true;
    }

    /// Returns false if time ever went backward.
    bool isMonotonic() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return violations_ == 0;
    }

    size_t getViolationCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return violations_;
    }

private:
    MonotonicClockChecker() = default;
    ~MonotonicClockChecker() = default;
    MonotonicClockChecker(const MonotonicClockChecker&) = delete;
    MonotonicClockChecker& operator=(const MonotonicClockChecker&) = delete;

    mutable std::mutex mutex_;
    bool enabled_ = false;
    size_t violations_ = 0;
    bool has_last_ = false;
    std::chrono::steady_clock::time_point last_{};
};

} // namespace nexus::utility::timing
