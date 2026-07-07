#pragma once

#include <string>
#include <vector>
#include <functional>
#include <source_location>
#include <cmath>
#include <mutex>

namespace nexus::utility::statistics {

class TrendAnalyzer {
public:
    static TrendAnalyzer& instance() {
        static TrendAnalyzer inst;
        return inst;
    }

    void initialize(const std::string& config = "") { config_ = config; enabled_ = true; }
    void shutdown() { enabled_ = false; }
    bool isEnabled() const { return enabled_; }
    void enable() { enabled_ = true; }
    void disable() { enabled_ = false; }
    std::string getStatus() const { return enabled_ ? "active" : "inactive"; }
    void reset() {
        std::lock_guard<std::mutex> lock(mutex_);
        values_.clear();
        count_ = 0;
        sum_x_ = 0.0;
        sum_y_ = 0.0;
        sum_xy_ = 0.0;
        sum_x2_ = 0.0;
        sum_y2_ = 0.0;
    }

    void addDataPoint(double value) {
        std::lock_guard<std::mutex> lock(mutex_);
        values_.push_back(value);
        double x = static_cast<double>(count_);
        ++count_;
        sum_x_ += x;
        sum_y_ += value;
        sum_xy_ += x * value;
        sum_x2_ += x * x;
        sum_y2_ += value * value;
    }

    double getSlope() const {
        std::lock_guard<std::mutex> lock(mutex_);
        if (count_ < 2) return 0.0;
        double n = static_cast<double>(count_);
        double denominator = n * sum_x2_ - sum_x_ * sum_x_;
        if (std::abs(denominator) < 1e-10) return 0.0;
        return (n * sum_xy_ - sum_x_ * sum_y_) / denominator;
    }

    double getIntercept() const {
        std::lock_guard<std::mutex> lock(mutex_);
        if (count_ < 2) return 0.0;
        double n = static_cast<double>(count_);
        double denominator = n * sum_x2_ - sum_x_ * sum_x_;
        if (std::abs(denominator) < 1e-10) return sum_y_ / n;
        double slope = (n * sum_xy_ - sum_x_ * sum_y_) / denominator;
        return (sum_y_ - slope * sum_x_) / n;
    }

    bool isTrendingUp() const {
        return getSlope() > 1e-6;
    }

    bool isTrendingDown() const {
        return getSlope() < -1e-6;
    }

    double getR2() const {
        std::lock_guard<std::mutex> lock(mutex_);
        if (count_ < 2) return 0.0;
        double n = static_cast<double>(count_);
        double denominator = n * sum_x2_ - sum_x_ * sum_x_;
        if (std::abs(denominator) < 1e-10) return 0.0;
        double slope = (n * sum_xy_ - sum_x_ * sum_y_) / denominator;
        double intercept = (sum_y_ - slope * sum_x_) / n;

        double mean_y = sum_y_ / n;
        double ss_tot = sum_y2_ - sum_y_ * sum_y_ / n;
        double ss_res = 0.0;
        for (size_t i = 0; i < values_.size(); ++i) {
            double pred = slope * static_cast<double>(i) + intercept;
            double diff = values_[i] - pred;
            ss_res += diff * diff;
        }

        if (ss_tot < 1e-10) return 1.0;
        return 1.0 - ss_res / ss_tot;
    }

    size_t getCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return count_;
    }

private:
    TrendAnalyzer() = default;
    ~TrendAnalyzer() = default;

    TrendAnalyzer(const TrendAnalyzer&) = delete;
    TrendAnalyzer& operator=(const TrendAnalyzer&) = delete;
    TrendAnalyzer(TrendAnalyzer&&) = delete;
    TrendAnalyzer& operator=(TrendAnalyzer&&) = delete;

    mutable std::mutex mutex_;
    bool enabled_ = false;
    std::string config_;
    std::vector<double> values_;
    size_t count_ = 0;
    double sum_x_ = 0.0;
    double sum_y_ = 0.0;
    double sum_xy_ = 0.0;
    double sum_x2_ = 0.0;
    double sum_y2_ = 0.0;
};

} // namespace nexus::utility::statistics
