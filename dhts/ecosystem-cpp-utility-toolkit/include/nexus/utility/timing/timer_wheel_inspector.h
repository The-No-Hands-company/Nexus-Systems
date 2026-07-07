#pragma once

#include <cstdint>
#include <vector>
#include <chrono>
#include <algorithm>
#include <mutex>

namespace nexus::utility::timing {

/// @brief Inspect a timer-wheel-like structure using a sorted timer list.
class TimerWheelInspector {
public:
    struct Timer {
        uint64_t id;
        std::chrono::milliseconds expiration;
    };

    static TimerWheelInspector& instance() {
        static TimerWheelInspector inst;
        return inst;
    }

    void initialize() {
        std::lock_guard<std::mutex> lock(mutex_);
        enabled_ = true;
        now_ = std::chrono::milliseconds{0};
        timers_.clear();
        expired_.clear();
    }

    void shutdown() {
        std::lock_guard<std::mutex> lock(mutex_);
        enabled_ = false;
        now_ = std::chrono::milliseconds{0};
        timers_.clear();
        expired_.clear();
    }

    bool isEnabled() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return enabled_;
    }

    /// Add a timer that expires `delay` after the current wheel time.
    void addTimer(uint64_t id, std::chrono::milliseconds delay) {
        std::lock_guard<std::mutex> lock(mutex_);
        timers_.push_back(Timer{id, now_ + delay});
        std::sort(timers_.begin(), timers_.end(),
                  [](const Timer& a, const Timer& b) { return a.expiration < b.expiration; });
    }

    void removeTimer(uint64_t id) {
        std::lock_guard<std::mutex> lock(mutex_);
        timers_.erase(std::remove_if(timers_.begin(), timers_.end(),
                      [id](const Timer& t) { return t.id == id; }), timers_.end());
    }

    /// Advance the wheel by one millisecond, expiring any due timers.
    void tick() {
        std::lock_guard<std::mutex> lock(mutex_);
        now_ += std::chrono::milliseconds{1};
        auto it = timers_.begin();
        while (it != timers_.end() && it->expiration <= now_) {
            expired_.push_back(it->id);
            it = timers_.erase(it);
        }
    }

    /// IDs of timers that have expired since initialization.
    std::vector<uint64_t> getExpiredTimers() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return expired_;
    }

    size_t getPendingCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return timers_.size();
    }

private:
    TimerWheelInspector() = default;
    ~TimerWheelInspector() = default;
    TimerWheelInspector(const TimerWheelInspector&) = delete;
    TimerWheelInspector& operator=(const TimerWheelInspector&) = delete;

    mutable std::mutex mutex_;
    bool enabled_ = false;
    std::chrono::milliseconds now_{0};
    std::vector<Timer> timers_;
    std::vector<uint64_t> expired_;
};

} // namespace nexus::utility::timing
