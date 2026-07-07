#pragma once

#include <string>
#include <vector>
#include <atomic>
#include <mutex>
#include <algorithm>

namespace nexus::utility::audio {

/**
 * @brief Track audio pipeline latency and buffer underruns.
 */
class AudioLatencyTracker {
public:
    static AudioLatencyTracker& instance() {
        static AudioLatencyTracker inst;
        return inst;
    }

    void initialize(const std::string& config = "") {
        std::lock_guard<std::mutex> lk(mutex_);
        config_ = config;
        enabled_ = true;
    }
    void shutdown() { enabled_ = false; }
    bool isEnabled() const { return enabled_.load(); }

    /// Configure device parameters to compute buffer latency.
    void configure(unsigned sampleRate, unsigned bufferFrames) {
        std::lock_guard<std::mutex> lk(mutex_);
        sampleRate_ = sampleRate;
        bufferFrames_ = bufferFrames;
    }

    /// Latency contributed by one buffer, in milliseconds.
    double bufferLatencyMs() const {
        std::lock_guard<std::mutex> lk(mutex_);
        return sampleRate_ ? 1000.0 * bufferFrames_ / sampleRate_ : 0.0;
    }

    /// Record a measured round-trip latency sample (ms).
    void recordLatency(double latencyMs) {
        std::lock_guard<std::mutex> lk(mutex_);
        samples_.push_back(latencyMs);
        sum_ += latencyMs;
        if (latencyMs > peak_) peak_ = latencyMs;
    }

    void recordUnderrun() { ++underruns_; }
    void recordOverrun() { ++overruns_; }

    double averageLatencyMs() const {
        std::lock_guard<std::mutex> lk(mutex_);
        return samples_.empty() ? 0.0 : sum_ / samples_.size();
    }
    double peakLatencyMs() const {
        std::lock_guard<std::mutex> lk(mutex_);
        return peak_;
    }
    std::size_t underruns() const { return underruns_.load(); }
    std::size_t overruns() const { return overruns_.load(); }

    void reset() {
        std::lock_guard<std::mutex> lk(mutex_);
        samples_.clear();
        sum_ = 0.0;
        peak_ = 0.0;
        underruns_ = 0;
        overruns_ = 0;
    }

private:
    AudioLatencyTracker() = default;
    ~AudioLatencyTracker() = default;
    AudioLatencyTracker(const AudioLatencyTracker&) = delete;
    AudioLatencyTracker& operator=(const AudioLatencyTracker&) = delete;

    mutable std::mutex mutex_;
    std::atomic<bool> enabled_{false};
    std::string config_;
    unsigned sampleRate_ = 0;
    unsigned bufferFrames_ = 0;
    std::vector<double> samples_;
    double sum_ = 0.0;
    double peak_ = 0.0;
    std::atomic<std::size_t> underruns_{0};
    std::atomic<std::size_t> overruns_{0};
};

} // namespace nexus::utility::audio
