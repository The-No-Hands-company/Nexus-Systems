#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <atomic>
#include <mutex>
#include <chrono>
#include <cstdint>

namespace nexus::utility::protocol {

/**
 * @brief Track protocol timeouts and their outcomes.
 */
class TimeoutDebugger {
public:
    struct PendingOp {
        std::string name;
        std::uint64_t startUs = 0;
        std::uint64_t timeoutUs = 0;
    };

    struct TimeoutEvent {
        std::string name;
        std::uint64_t elapsedUs = 0;
        std::uint64_t timeoutUs = 0;
        bool timedOut = false;
    };

    static TimeoutDebugger& instance() {
        static TimeoutDebugger inst;
        return inst;
    }

    void initialize(const std::string& config = "") {
        std::lock_guard<std::mutex> lk(mutex_);
        config_ = config;
        enabled_ = true;
    }
    void shutdown() { enabled_ = false; }
    bool isEnabled() const { return enabled_.load(); }

    void startOperation(const std::string& name, std::uint64_t timeoutUs) {
        std::lock_guard<std::mutex> lk(mutex_);
        pending_[name] = {name, nowUs(), timeoutUs};
    }

    /// Complete an operation, returning whether it exceeded its timeout.
    TimeoutEvent completeOperation(const std::string& name) {
        std::lock_guard<std::mutex> lk(mutex_);
        TimeoutEvent ev;
        ev.name = name;
        auto it = pending_.find(name);
        if (it != pending_.end()) {
            ev.elapsedUs = nowUs() - it->second.startUs;
            ev.timeoutUs = it->second.timeoutUs;
            ev.timedOut = ev.elapsedUs > it->second.timeoutUs;
            pending_.erase(it);
        }
        events_.push_back(ev);
        if (ev.timedOut) ++timeouts_;
        return ev;
    }

    /// Check for currently pending operations that have already exceeded timeout.
    std::vector<std::string> expiredOperations() const {
        std::lock_guard<std::mutex> lk(mutex_);
        std::vector<std::string> out;
        std::uint64_t now = nowUs();
        for (const auto& [n, op] : pending_)
            if (now - op.startUs > op.timeoutUs) out.push_back(n);
        return out;
    }

    std::size_t timeouts() const { return timeouts_.load(); }

    std::vector<TimeoutEvent> events() const {
        std::lock_guard<std::mutex> lk(mutex_);
        return events_;
    }

    void reset() {
        std::lock_guard<std::mutex> lk(mutex_);
        pending_.clear();
        events_.clear();
        timeouts_ = 0;
    }

private:
    TimeoutDebugger() = default;
    ~TimeoutDebugger() = default;
    TimeoutDebugger(const TimeoutDebugger&) = delete;
    TimeoutDebugger& operator=(const TimeoutDebugger&) = delete;

    static std::uint64_t nowUs() {
        return static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count());
    }

    mutable std::mutex mutex_;
    std::atomic<bool> enabled_{false};
    std::string config_;
    std::unordered_map<std::string, PendingOp> pending_;
    std::vector<TimeoutEvent> events_;
    std::atomic<std::size_t> timeouts_{0};
};

} // namespace nexus::utility::protocol
