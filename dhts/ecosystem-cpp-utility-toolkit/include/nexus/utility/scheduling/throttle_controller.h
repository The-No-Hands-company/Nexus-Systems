#pragma once

#include <chrono>
#include <deque>
#include <mutex>
#include <atomic>
#include <thread>

namespace nexus::utility::scheduling {

/// Request throttling with sliding window
class ThrottleController {
public:
    struct Config {
        size_t max_requests = 100;
        std::chrono::seconds window_size{1};
        bool block_on_limit = false;
    };

    ThrottleController() : config_{} {}
    explicit ThrottleController(Config config) : config_(config) {}

    /// Try to allow a request. Returns true if allowed.
    bool tryAcquire() {
        std::lock_guard lock(mutex_);
        auto now = std::chrono::steady_clock::now();
        cleanup(now);

        if (window_.size() < config_.max_requests) {
            window_.push_back(now);
            return true;
        }
        return false;
    }

    /// Wait until a slot is available
    bool waitAcquire() {
        if (config_.max_requests == 0) return false;
        while (!tryAcquire()) {
            std::this_thread::sleep_for(
                std::chrono::milliseconds(
                    std::max<int64_t>(1, config_.window_size.count() * 1000 /
                                            static_cast<int64_t>(config_.max_requests))));
        }
        return true;
    }

    /// Get current request count in window
    size_t currentCount() const {
        std::lock_guard lock(mutex_);
        cleanup(std::chrono::steady_clock::now());
        return window_.size();
    }

    /// Get remaining capacity
    size_t remaining() const {
        auto count = currentCount();
        return count < config_.max_requests ? config_.max_requests - count : 0;
    }

    /// Reset the throttle
    void reset() {
        std::lock_guard lock(mutex_);
        window_.clear();
    }

    const Config& config() const { return config_; }

private:
    Config config_;
    mutable std::deque<std::chrono::steady_clock::time_point> window_;
    mutable std::mutex mutex_;

    void cleanup(std::chrono::steady_clock::time_point now) const {
        auto cutoff = now - config_.window_size;
        while (!window_.empty() && window_.front() < cutoff) {
            window_.pop_front();
        }
    }
};

} // namespace nexus::utility::scheduling
