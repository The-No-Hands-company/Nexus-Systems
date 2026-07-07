#pragma once

#include <string>
#include <atomic>
#include <mutex>
#include <unordered_map>
#include <cstddef>

namespace nexus::utility::audio {

/**
 * @brief Profile real-time audio callback execution timing and deadline misses.
 */
class AudioCallbackProfiler {
public:
    static AudioCallbackProfiler& instance() {
        static AudioCallbackProfiler inst;
        return inst;
    }

    void initialize(const std::string& config = "") {
        std::lock_guard<std::mutex> lk(mutex_);
        config_ = config;
        enabled_ = true;
    }
    void shutdown() { enabled_ = false; }
    void enable() { enabled_ = true; }
    void disable() { enabled_ = false; }
    bool isEnabled() const { return enabled_.load(); }

    /// Record one callback invocation for @p name.
    void recordCallback(const std::string& name, double duration_us, bool missed_deadline) {
        std::lock_guard<std::mutex> lk(mutex_);
        auto& s = stats_[name];
        ++s.count;
        s.sum += duration_us;
        if (duration_us > s.max) s.max = duration_us;
        if (missed_deadline) ++s.missedDeadlines;
    }

    /// Mean callback duration (us) for @p name, or 0 if unseen.
    double getAvgDuration(const std::string& name) const {
        std::lock_guard<std::mutex> lk(mutex_);
        auto it = stats_.find(name);
        return (it != stats_.end() && it->second.count)
                   ? it->second.sum / it->second.count
                   : 0.0;
    }

    /// Worst-case callback duration (us) for @p name, or 0 if unseen.
    double getMaxDuration(const std::string& name) const {
        std::lock_guard<std::mutex> lk(mutex_);
        auto it = stats_.find(name);
        return it != stats_.end() ? it->second.max : 0.0;
    }

    std::size_t getMissedDeadlineCount(const std::string& name) const {
        std::lock_guard<std::mutex> lk(mutex_);
        auto it = stats_.find(name);
        return it != stats_.end() ? it->second.missedDeadlines : 0;
    }

    std::string getStatus() const {
        std::lock_guard<std::mutex> lk(mutex_);
        return "AudioCallbackProfiler{tracked=" + std::to_string(stats_.size()) + "}";
    }

    void reset() {
        std::lock_guard<std::mutex> lk(mutex_);
        stats_.clear();
    }

private:
    AudioCallbackProfiler() = default;
    ~AudioCallbackProfiler() = default;
    AudioCallbackProfiler(const AudioCallbackProfiler&) = delete;
    AudioCallbackProfiler& operator=(const AudioCallbackProfiler&) = delete;

    struct Stats {
        std::size_t count = 0;
        double sum = 0.0;
        double max = 0.0;
        std::size_t missedDeadlines = 0;
    };

    mutable std::mutex mutex_;
    std::atomic<bool> enabled_{false};
    std::string config_;
    std::unordered_map<std::string, Stats> stats_;
};

} // namespace nexus::utility::audio
