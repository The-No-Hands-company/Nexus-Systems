#pragma once

#include <chrono>
#include <string>
#include <functional>
#include <vector>
#include <mutex>

namespace nexus::utility::scheduling {

/// Deadline tracker for monitoring time-bounded operations
class DeadlineTracker {
public:
    struct Deadline {
        std::string name;
        std::chrono::steady_clock::time_point deadline;
        std::function<void()> on_expired;
        bool expired = false;
        bool notified = false;
    };

    /// Set a deadline for a named operation
    void set(const std::string& name, std::chrono::milliseconds timeout,
             std::function<void()> on_expired = nullptr) {
        std::lock_guard lock(mutex_);
        Deadline d;
        d.name = name;
        d.deadline = std::chrono::steady_clock::now() + timeout;
        d.on_expired = std::move(on_expired);
        deadlines_.push_back(std::move(d));
    }

    /// Check if a deadline has been met
    [[nodiscard]] bool check(const std::string& name) {
        std::function<void()> callback;
        bool expired = false;
        {
            std::lock_guard lock(mutex_);
            auto now = std::chrono::steady_clock::now();
            for (auto& d : deadlines_) {
                if (d.name == name && !d.notified) {
                    d.notified = true;
                    if (now > d.deadline) {
                        d.expired = true;
                        expired = true;
                        callback = std::move(d.on_expired);
                    } else {
                        return true;
                    }
                    break;
                }
            }
        }
        // Invoke callback outside the lock to prevent deadlocks
        if (callback) {
            callback();
        }
        return !expired;
    }

    /// Check all deadlines and trigger expired callbacks
    [[nodiscard]] size_t checkAll() {
        std::vector<std::function<void()>> callbacks;
        {
            std::lock_guard lock(mutex_);
            auto now = std::chrono::steady_clock::now();
            for (auto& d : deadlines_) {
                if (!d.notified && now > d.deadline) {
                    d.expired = true;
                    d.notified = true;
                    callbacks.push_back(std::move(d.on_expired));
                }
            }
        }
        // Invoke callbacks outside the lock to prevent deadlocks
        for (auto& cb : callbacks) {
            if (cb) cb();
        }
        return callbacks.size();
    }

    /// Get remaining time for a deadline
    [[nodiscard]] std::optional<std::chrono::milliseconds> remaining(
        const std::string& name) const {
        std::lock_guard lock(mutex_);
        auto now = std::chrono::steady_clock::now();
        for (const auto& d : deadlines_) {
            if (d.name == name && !d.notified) {
                auto remain = std::chrono::duration_cast<std::chrono::milliseconds>(
                    d.deadline - now);
                return std::max(std::chrono::milliseconds(0), remain);
            }
        }
        return std::nullopt;
    }

    /// Clear all deadlines
    void clear() {
        std::lock_guard lock(mutex_);
        deadlines_.clear();
    }

private:
    std::vector<Deadline> deadlines_;
    mutable std::mutex mutex_;
};

} // namespace nexus::utility::scheduling
