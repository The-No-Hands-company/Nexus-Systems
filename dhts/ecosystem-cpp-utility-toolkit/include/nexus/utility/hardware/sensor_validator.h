#pragma once

#include <cmath>
#include <map>
#include <mutex>
#include <string>

namespace nexus::utility::hardware {

class SensorValidator {
public:
    struct SensorRange {
        double min;
        double max;
    };

    static SensorValidator& instance() {
        static SensorValidator inst;
        return inst;
    }

    void initialize(const std::string& = "") { std::lock_guard<std::mutex> lock(mutex_); enabled_ = true; }
    void shutdown() { std::lock_guard<std::mutex> lock(mutex_); enabled_ = false; reset(); }
    bool isEnabled() const { return enabled_; }
    void enable() { std::lock_guard<std::mutex> lock(mutex_); enabled_ = true; }
    void disable() { std::lock_guard<std::mutex> lock(mutex_); enabled_ = false; }

    std::string getStatus() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return "SensorValidator: " + std::to_string(ranges_.size()) + " sensors with ranges, " +
               std::to_string(deltas_.size()) + " sensors with deltas, enabled=" + (enabled_ ? "true" : "false");
    }
    void reset() {
        std::lock_guard<std::mutex> lock(mutex_);
        ranges_.clear();
        deltas_.clear();
    }

    void setExpectedRange(const std::string& sensor, double min, double max) {
        std::lock_guard<std::mutex> lock(mutex_);
        ranges_[sensor] = {min, max};
    }

    bool validate(const std::string& sensor, double value) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = ranges_.find(sensor);
        if (it == ranges_.end()) return true;
        return value >= it->second.min && value <= it->second.max;
    }

    void setMaxDelta(const std::string& sensor, double delta) {
        std::lock_guard<std::mutex> lock(mutex_);
        deltas_[sensor] = delta;
    }

    bool validateDelta(const std::string& sensor, double prev, double current) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = deltas_.find(sensor);
        if (it == deltas_.end()) return true;
        return std::abs(current - prev) <= it->second;
    }

private:
    SensorValidator() = default;
    ~SensorValidator() = default;
    SensorValidator(const SensorValidator&) = delete;
    SensorValidator& operator=(const SensorValidator&) = delete;
    SensorValidator(SensorValidator&&) = delete;
    SensorValidator& operator=(SensorValidator&&) = delete;

    mutable std::mutex mutex_;
    bool enabled_ = false;
    std::map<std::string, SensorRange> ranges_;
    std::map<std::string, double> deltas_;
};

} // namespace nexus::utility::hardware
