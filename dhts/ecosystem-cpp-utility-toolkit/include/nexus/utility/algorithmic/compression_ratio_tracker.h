#pragma once

#include <cstdint>
#include <mutex>
#include <string>

namespace nexus::utility::algorithmic {

class CompressionRatioTracker {
public:
    static CompressionRatioTracker& instance() {
        static CompressionRatioTracker inst;
        return inst;
    }

    void initialize() { std::lock_guard<std::mutex> lock(mutex_); enabled_ = true; }
    void shutdown() { std::lock_guard<std::mutex> lock(mutex_); enabled_ = false; reset(); }
    bool isEnabled() const { return enabled_; }

    void recordCompression(size_t original, size_t compressed) {
        std::lock_guard<std::mutex> lock(mutex_);
        total_original_ += original;
        total_compressed_ += compressed;
        ++count_;
    }

    double getRatio() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return total_compressed_ > 0 ? static_cast<double>(total_original_) / total_compressed_ : 1.0;
    }

    double getAverageRatio() const {
        return getRatio();
    }

    size_t getSavedBytes() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return total_original_ > total_compressed_ ? total_original_ - total_compressed_ : 0;
    }

    uint64_t getCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return count_;
    }

private:
    void reset() {
        total_original_ = 0;
        total_compressed_ = 0;
        count_ = 0;
    }

    CompressionRatioTracker() = default;
    ~CompressionRatioTracker() = default;
    CompressionRatioTracker(const CompressionRatioTracker&) = delete;
    CompressionRatioTracker& operator=(const CompressionRatioTracker&) = delete;

    mutable std::mutex mutex_;
    bool enabled_ = false;
    size_t total_original_ = 0;
    size_t total_compressed_ = 0;
    uint64_t count_ = 0;
};

} // namespace nexus::utility::algorithmic
