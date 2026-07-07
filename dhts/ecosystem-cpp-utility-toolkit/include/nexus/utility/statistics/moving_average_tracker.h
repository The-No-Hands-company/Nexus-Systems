#pragma once

#include <string>
#include <vector>
#include <functional>
#include <source_location>
#include <deque>
#include <numeric>
#include <mutex>

namespace nexus::utility::statistics {

class MovingAverageTracker {
public:
    static MovingAverageTracker& instance() {
        static MovingAverageTracker inst;
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
        window_ = 10;
        values_.clear();
        sum_ = 0.0;
        ema_value_ = 0.0;
        ema_initialized_ = false;
    }

    void addValue(double value) {
        std::lock_guard<std::mutex> lock(mutex_);
        values_.push_back(value);
        sum_ += value;
        if (values_.size() > window_) {
            sum_ -= values_.front();
            values_.pop_front();
        }
    }

    void setWindow(size_t window) {
        std::lock_guard<std::mutex> lock(mutex_);
        window_ = window;
        while (values_.size() > window_) {
            sum_ -= values_.front();
            values_.pop_front();
        }
    }

    double getSMA() const {
        std::lock_guard<std::mutex> lock(mutex_);
        if (values_.empty()) return 0.0;
        return sum_ / static_cast<double>(values_.size());
    }

    double getEMA(double alpha) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (values_.empty()) return 0.0;
        if (!ema_initialized_) {
            ema_value_ = sum_ / static_cast<double>(values_.size());
            ema_initialized_ = true;
            return ema_value_;
        }
        double latest = values_.back();
        ema_value_ = alpha * latest + (1.0 - alpha) * ema_value_;
        return ema_value_;
    }

    size_t getCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return values_.size();
    }

    size_t getWindow() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return window_;
    }

private:
    MovingAverageTracker() = default;
    ~MovingAverageTracker() = default;

    MovingAverageTracker(const MovingAverageTracker&) = delete;
    MovingAverageTracker& operator=(const MovingAverageTracker&) = delete;
    MovingAverageTracker(MovingAverageTracker&&) = delete;
    MovingAverageTracker& operator=(MovingAverageTracker&&) = delete;

    mutable std::mutex mutex_;
    bool enabled_ = false;
    std::string config_;
    size_t window_ = 10;
    std::deque<double> values_;
    double sum_ = 0.0;
    double ema_value_ = 0.0;
    bool ema_initialized_ = false;
};

} // namespace nexus::utility::statistics
