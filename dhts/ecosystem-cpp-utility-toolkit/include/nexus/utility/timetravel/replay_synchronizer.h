#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <atomic>
#include <mutex>
#include <cstdint>

namespace nexus::utility::timetravel {

/**
 * @brief Synchronize multiple replay streams to a common logical clock.
 */
class ReplaySynchronizer {
public:
    struct Stream {
        std::string name;
        std::uint64_t cursor = 0;      // events consumed
        std::uint64_t clock = 0;       // current logical timestamp
    };

    static ReplaySynchronizer& instance() {
        static ReplaySynchronizer inst;
        return inst;
    }

    void initialize(const std::string& config = "") {
        std::lock_guard<std::mutex> lk(mutex_);
        config_ = config;
        enabled_ = true;
    }
    void shutdown() { enabled_ = false; }
    bool isEnabled() const { return enabled_.load(); }

    void registerStream(const std::string& name) {
        std::lock_guard<std::mutex> lk(mutex_);
        streams_[name] = {name, 0, 0};
    }

    void advance(const std::string& name, std::uint64_t toClock) {
        std::lock_guard<std::mutex> lk(mutex_);
        auto it = streams_.find(name);
        if (it != streams_.end()) {
            it->second.clock = toClock;
            ++it->second.cursor;
        }
    }

    /// The barrier clock is the minimum clock across all streams; a stream may
    /// only proceed past this to keep replay ordering consistent.
    std::uint64_t barrierClock() const {
        std::lock_guard<std::mutex> lk(mutex_);
        if (streams_.empty()) return 0;
        std::uint64_t minClock = UINT64_MAX;
        for (const auto& [n, s] : streams_) minClock = std::min(minClock, s.clock);
        return minClock;
    }

    /// A stream may advance if it is not ahead of the slowest stream by more
    /// than `slack` ticks (keeps streams roughly synchronized).
    bool canAdvance(const std::string& name, std::uint64_t slack = 0) const {
        std::lock_guard<std::mutex> lk(mutex_);
        auto it = streams_.find(name);
        if (it == streams_.end()) return false;
        std::uint64_t minClock = UINT64_MAX;
        for (const auto& [n, s] : streams_) minClock = std::min(minClock, s.clock);
        return it->second.clock <= minClock + slack;
    }

    bool allSynchronized() const {
        std::lock_guard<std::mutex> lk(mutex_);
        if (streams_.empty()) return true;
        std::uint64_t ref = streams_.begin()->second.clock;
        for (const auto& [n, s] : streams_) if (s.clock != ref) return false;
        return true;
    }

    Stream stream(const std::string& name) const {
        std::lock_guard<std::mutex> lk(mutex_);
        auto it = streams_.find(name);
        return it == streams_.end() ? Stream{name, 0, 0} : it->second;
    }

    void reset() {
        std::lock_guard<std::mutex> lk(mutex_);
        streams_.clear();
    }

private:
    ReplaySynchronizer() = default;
    ~ReplaySynchronizer() = default;
    ReplaySynchronizer(const ReplaySynchronizer&) = delete;
    ReplaySynchronizer& operator=(const ReplaySynchronizer&) = delete;

    mutable std::mutex mutex_;
    std::atomic<bool> enabled_{false};
    std::string config_;
    std::unordered_map<std::string, Stream> streams_;
};

} // namespace nexus::utility::timetravel
