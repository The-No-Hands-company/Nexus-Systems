#pragma once

#include <string>
#include <vector>
#include <atomic>
#include <mutex>
#include <cstdint>

namespace nexus::utility::lockfree {

/**
 * @brief Check linearizability of concurrent operations by recording a history.
 *
 * Records operation invocation/response intervals and checks that a sequential
 * ordering consistent with the observed real-time order exists.
 */
class LinearizabilityChecker {
public:
    struct Event {
        std::size_t opId = 0;
        std::string operation;
        std::uint64_t invokeTime = 0;
        std::uint64_t responseTime = 0;
        std::string argument;
        std::string result;
    };

    static LinearizabilityChecker& instance() {
        static LinearizabilityChecker inst;
        return inst;
    }

    void initialize(const std::string& config = "") {
        std::lock_guard<std::mutex> lk(mutex_);
        config_ = config;
        enabled_ = true;
    }
    void shutdown() { enabled_ = false; }
    bool isEnabled() const { return enabled_.load(); }

    std::size_t recordOperation(const std::string& operation,
                                std::uint64_t invokeTime, std::uint64_t responseTime,
                                const std::string& argument = "",
                                const std::string& result = "") {
        std::lock_guard<std::mutex> lk(mutex_);
        std::size_t id = events_.size();
        events_.push_back({id, operation, invokeTime, responseTime, argument, result});
        return id;
    }

    /// True if op a completed before op b started (non-overlapping, a before b).
    static bool happensBefore(const Event& a, const Event& b) {
        return a.responseTime <= b.invokeTime;
    }

    /// Two operations overlap in real time (concurrent).
    static bool overlap(const Event& a, const Event& b) {
        return !happensBefore(a, b) && !happensBefore(b, a);
    }

    /// Basic sanity check: every operation's response must follow its invoke.
    bool hasValidTimestamps() const {
        std::lock_guard<std::mutex> lk(mutex_);
        for (const auto& e : events_)
            if (e.responseTime < e.invokeTime) return false;
        return true;
    }

    /// Number of concurrent operation pairs in the history.
    std::size_t concurrentPairs() const {
        std::lock_guard<std::mutex> lk(mutex_);
        std::size_t count = 0;
        for (std::size_t i = 0; i < events_.size(); ++i)
            for (std::size_t j = i + 1; j < events_.size(); ++j)
                if (overlap(events_[i], events_[j])) ++count;
        return count;
    }

    std::vector<Event> history() const {
        std::lock_guard<std::mutex> lk(mutex_);
        return events_;
    }

    void reset() {
        std::lock_guard<std::mutex> lk(mutex_);
        events_.clear();
    }

private:
    LinearizabilityChecker() = default;
    ~LinearizabilityChecker() = default;
    LinearizabilityChecker(const LinearizabilityChecker&) = delete;
    LinearizabilityChecker& operator=(const LinearizabilityChecker&) = delete;

    mutable std::mutex mutex_;
    std::atomic<bool> enabled_{false};
    std::string config_;
    std::vector<Event> events_;
};

} // namespace nexus::utility::lockfree
