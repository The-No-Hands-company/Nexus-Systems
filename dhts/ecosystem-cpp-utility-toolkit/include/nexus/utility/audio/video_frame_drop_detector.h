#pragma once

#include <string>
#include <atomic>
#include <mutex>
#include <cstdint>
#include <cstddef>

namespace nexus::utility::audio {

/**
 * @brief Detect dropped video frames by tracking gaps in the frame sequence.
 */
class VideoFrameDropDetector {
public:
    static VideoFrameDropDetector& instance() {
        static VideoFrameDropDetector inst;
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

    /// Record a presented frame. Gaps versus the previous frame number are
    /// counted as dropped frames.
    void recordFrame(std::uint64_t frame_number, double presentation_time_ms) {
        std::lock_guard<std::mutex> lk(mutex_);
        lastPresentationMs_ = presentation_time_ms;
        if (haveLast_ && frame_number > lastFrame_ + 1) {
            std::uint64_t gap = frame_number - lastFrame_ - 1;
            droppedCount_ += gap;
            lastGap_ = gap;
        } else {
            lastGap_ = 0;
        }
        lastFrame_ = frame_number;
        haveLast_ = true;
        ++totalFrames_;
    }

    /// True if the most recently recorded frame followed a sequence gap.
    bool checkDropped() const {
        std::lock_guard<std::mutex> lk(mutex_);
        return lastGap_ > 0;
    }

    std::size_t getDroppedCount() const {
        std::lock_guard<std::mutex> lk(mutex_);
        return static_cast<std::size_t>(droppedCount_);
    }

    /// Ratio of dropped frames to expected frames (delivered + dropped).
    double getDropRate() const {
        std::lock_guard<std::mutex> lk(mutex_);
        std::uint64_t expected = totalFrames_ + droppedCount_;
        return expected ? static_cast<double>(droppedCount_) / expected : 0.0;
    }

    std::size_t getTotalFrames() const {
        std::lock_guard<std::mutex> lk(mutex_);
        return static_cast<std::size_t>(totalFrames_);
    }

    std::string getStatus() const {
        std::lock_guard<std::mutex> lk(mutex_);
        return "VideoFrameDropDetector{frames=" + std::to_string(totalFrames_) +
               ", dropped=" + std::to_string(droppedCount_) + "}";
    }

    void reset() {
        std::lock_guard<std::mutex> lk(mutex_);
        haveLast_ = false;
        lastFrame_ = 0;
        lastGap_ = 0;
        droppedCount_ = 0;
        totalFrames_ = 0;
        lastPresentationMs_ = 0.0;
    }

private:
    VideoFrameDropDetector() = default;
    ~VideoFrameDropDetector() = default;
    VideoFrameDropDetector(const VideoFrameDropDetector&) = delete;
    VideoFrameDropDetector& operator=(const VideoFrameDropDetector&) = delete;

    mutable std::mutex mutex_;
    std::atomic<bool> enabled_{false};
    std::string config_;
    bool haveLast_ = false;
    std::uint64_t lastFrame_ = 0;
    std::uint64_t lastGap_ = 0;
    std::uint64_t droppedCount_ = 0;
    std::uint64_t totalFrames_ = 0;
    double lastPresentationMs_ = 0.0;
};

} // namespace nexus::utility::audio
