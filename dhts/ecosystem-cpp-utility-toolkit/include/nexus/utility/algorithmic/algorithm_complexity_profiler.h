#pragma once

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

namespace nexus::utility::algorithmic {

class AlgorithmComplexityProfiler {
public:
    struct DataPoint {
        size_t input_size;
        double time_us;
    };

    static AlgorithmComplexityProfiler& instance() {
        static AlgorithmComplexityProfiler inst;
        return inst;
    }

    void initialize() { std::lock_guard<std::mutex> lock(mutex_); enabled_ = true; }
    void shutdown() { std::lock_guard<std::mutex> lock(mutex_); enabled_ = false; data_.clear(); }
    bool isEnabled() const { return enabled_; }

    void recordRun(size_t input_size, double time_us) {
        std::lock_guard<std::mutex> lock(mutex_);
        data_.push_back({input_size, time_us});
    }

    std::string estimateComplexity() const {
        std::lock_guard<std::mutex> lock(mutex_);
        if (data_.size() < 2) return "O(?)";

        double max_ratio = 0.0;
        for (size_t i = 1; i < data_.size(); ++i) {
            double size_ratio = static_cast<double>(data_[i].input_size) / data_[i - 1].input_size;
            double time_ratio = data_[i].time_us / data_[i - 1].time_us;
            if (size_ratio > 0.0 && time_ratio > 0.0) {
                double complexity_ratio = std::log(time_ratio) / std::log(size_ratio);
                if (std::abs(complexity_ratio) > max_ratio) max_ratio = std::abs(complexity_ratio);
            }
        }

        if (max_ratio < 0.3) return "O(1)";
        if (max_ratio < 0.6) return "O(log n)";
        if (max_ratio < 1.3) return "O(n)";
        if (max_ratio < 1.6) return "O(n log n)";
        return "O(n^2)";
    }

    std::vector<DataPoint> getDataPoints() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return data_;
    }

private:
    AlgorithmComplexityProfiler() = default;
    ~AlgorithmComplexityProfiler() = default;
    AlgorithmComplexityProfiler(const AlgorithmComplexityProfiler&) = delete;
    AlgorithmComplexityProfiler& operator=(const AlgorithmComplexityProfiler&) = delete;

    mutable std::mutex mutex_;
    bool enabled_ = false;
    std::vector<DataPoint> data_;
};

} // namespace nexus::utility::algorithmic
