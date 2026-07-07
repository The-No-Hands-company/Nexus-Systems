#pragma once

#include <string>
#include <atomic>
#include <mutex>
#include <cstddef>

namespace nexus::utility::audio {

/**
 * @brief Detect audio buffer underruns (available samples below the demand).
 */
class AudioBufferUnderrunDetector {
public:
    static AudioBufferUnderrunDetector& instance() {
        static AudioBufferUnderrunDetector inst;
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

    /// Record the most recent buffer availability against the demand.
    void recordBufferStatus(std::size_t available_samples, std::size_t needed_samples) {
        std::lock_guard<std::mutex> lk(mutex_);
        available_ = available_samples;
        needed_ = needed_samples;
        ++checkCount_;
        if (available_samples < needed_samples) ++underrunCount_;
    }

    /// True if the last recorded status was an underrun (available < needed).
    bool checkUnderrun() const {
        std::lock_guard<std::mutex> lk(mutex_);
        return available_ < needed_;
    }

    std::size_t getUnderrunCount() const {
        std::lock_guard<std::mutex> lk(mutex_);
        return underrunCount_;
    }

    /// Fraction of recorded statuses that were underruns (0.0 - 1.0).
    double getUnderrunRate() const {
        std::lock_guard<std::mutex> lk(mutex_);
        return checkCount_ ? static_cast<double>(underrunCount_) / checkCount_ : 0.0;
    }

    std::string getStatus() const {
        std::lock_guard<std::mutex> lk(mutex_);
        return "AudioBufferUnderrunDetector{checks=" + std::to_string(checkCount_) +
               ", underruns=" + std::to_string(underrunCount_) + "}";
    }

    void reset() {
        std::lock_guard<std::mutex> lk(mutex_);
        available_ = 0;
        needed_ = 0;
        checkCount_ = 0;
        underrunCount_ = 0;
    }

private:
    AudioBufferUnderrunDetector() = default;
    ~AudioBufferUnderrunDetector() = default;
    AudioBufferUnderrunDetector(const AudioBufferUnderrunDetector&) = delete;
    AudioBufferUnderrunDetector& operator=(const AudioBufferUnderrunDetector&) = delete;

    mutable std::mutex mutex_;
    std::atomic<bool> enabled_{false};
    std::string config_;
    std::size_t available_ = 0;
    std::size_t needed_ = 0;
    std::size_t checkCount_ = 0;
    std::size_t underrunCount_ = 0;
};

} // namespace nexus::utility::audio
