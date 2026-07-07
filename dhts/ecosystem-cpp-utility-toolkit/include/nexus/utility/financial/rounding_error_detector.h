#pragma once

#include <algorithm>
#include <cmath>
#include <mutex>
#include <string>

namespace nexus::utility::financial {

class RoundingErrorDetector {
public:
    static RoundingErrorDetector& instance() {
        static RoundingErrorDetector inst;
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
    }

    bool isEnabled() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return enabled_;
    }

    void enable() {
        std::lock_guard<std::mutex> lock(mutex_);
        enabled_ = true;
    }

    void disable() {
        std::lock_guard<std::mutex> lock(mutex_);
        enabled_ = false;
    }

    std::string getStatus() const {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!enabled_) return "RoundingErrorDetector disabled";
        return "RoundingErrorDetector active, ops=" + std::to_string(operation_count_)
             + ", accumulated=" + std::to_string(accumulated_error_)
             + ", max=" + std::to_string(max_error_);
    }

    void reset() {
        std::lock_guard<std::mutex> lock(mutex_);
        accumulated_error_ = 0.0;
        max_error_ = 0.0;
        operation_count_ = 0;
    }

    void recordOperation(double expected, double actual) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!enabled_) return;
        double error = std::abs(expected - actual);
        accumulated_error_ += error;
        max_error_ = std::max(max_error_, error);
        ++operation_count_;
    }

    double getAccumulatedError() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return accumulated_error_;
    }

    double getMaxError() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return max_error_;
    }

    uint64_t getOperationCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return operation_count_;
    }

private:
    RoundingErrorDetector() = default;
    ~RoundingErrorDetector() = default;

    RoundingErrorDetector(const RoundingErrorDetector&) = delete;
    RoundingErrorDetector& operator=(const RoundingErrorDetector&) = delete;
    RoundingErrorDetector(RoundingErrorDetector&&) = delete;
    RoundingErrorDetector& operator=(RoundingErrorDetector&&) = delete;

    mutable std::mutex mutex_;
    bool enabled_ = false;
    std::string config_;
    double accumulated_error_ = 0.0;
    double max_error_ = 0.0;
    uint64_t operation_count_ = 0;
};

} // namespace nexus::utility::financial
