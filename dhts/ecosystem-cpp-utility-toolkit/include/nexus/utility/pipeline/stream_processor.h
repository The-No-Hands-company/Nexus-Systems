#pragma once

#include <chrono>
#include <cstddef>
#include <functional>
#include <mutex>
#include <string>

namespace nexus::utility::pipeline {

/// @brief Processes stream elements one at a time, tracking throughput and rate.
class StreamProcessor {
public:
    using Processor = std::function<std::string(std::string)>;

    static StreamProcessor& instance() {
        static StreamProcessor inst;
        return inst;
    }

    void initialize(const std::string& config = "") {
        std::lock_guard<std::mutex> lock(mutex_);
        enabled_ = true;
        config_ = config;
    }

    void shutdown() {
        std::lock_guard<std::mutex> lock(mutex_);
        enabled_ = false;
        processed_count_ = 0;
        started_ = false;
        rate_limit_ = 0;
    }

    bool isEnabled() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return enabled_;
    }

    /// @brief Apply a processor to a stream element and record it.
    std::string processElement(const std::string& element, Processor processor) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (!started_) {
                start_time_ = std::chrono::steady_clock::now();
                started_ = true;
            }
        }
        std::string result = processor ? processor(element) : element;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            ++processed_count_;
            last_time_ = std::chrono::steady_clock::now();
        }
        return result;
    }

    size_t getProcessedCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return processed_count_;
    }

    /// @brief Elements processed per second since the first element (0.0 if not measurable).
    double getProcessingRate() const {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!started_ || processed_count_ == 0) return 0.0;
        double seconds = std::chrono::duration<double>(last_time_ - start_time_).count();
        if (seconds <= 0.0) return static_cast<double>(processed_count_);
        return static_cast<double>(processed_count_) / seconds;
    }

    void setRateLimit(size_t per_second) {
        std::lock_guard<std::mutex> lock(mutex_);
        rate_limit_ = per_second;
    }

    size_t getRateLimit() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return rate_limit_;
    }

    void reset() {
        std::lock_guard<std::mutex> lock(mutex_);
        processed_count_ = 0;
        started_ = false;
        rate_limit_ = 0;
    }

private:
    StreamProcessor() = default;
    ~StreamProcessor() = default;
    StreamProcessor(const StreamProcessor&) = delete;
    StreamProcessor& operator=(const StreamProcessor&) = delete;

    mutable std::mutex mutex_;
    bool enabled_ = false;
    std::string config_;
    size_t processed_count_ = 0;
    size_t rate_limit_ = 0;
    bool started_ = false;
    std::chrono::steady_clock::time_point start_time_{};
    std::chrono::steady_clock::time_point last_time_{};
};

} // namespace nexus::utility::pipeline
