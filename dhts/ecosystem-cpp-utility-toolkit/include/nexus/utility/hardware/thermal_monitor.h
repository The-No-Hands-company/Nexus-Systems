#pragma once

#include <algorithm>
#include <cstdint>
#include <map>
#include <mutex>
#include <string>
#include <vector>

namespace nexus::utility::hardware {

class ThermalMonitor {
public:
    struct SensorData {
        std::vector<double> readings;
        double max_temp = -273.15;
        double sum = 0.0;
        uint64_t count = 0;
    };

    static ThermalMonitor& instance() {
        static ThermalMonitor inst;
        return inst;
    }

    void initialize(const std::string& = "") { std::lock_guard<std::mutex> lock(mutex_); enabled_ = true; }
    void shutdown() { std::lock_guard<std::mutex> lock(mutex_); enabled_ = false; reset(); }
    bool isEnabled() const { return enabled_; }
    void enable() { std::lock_guard<std::mutex> lock(mutex_); enabled_ = true; }
    void disable() { std::lock_guard<std::mutex> lock(mutex_); enabled_ = false; }

    std::string getStatus() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return "ThermalMonitor: " + std::to_string(sensors_.size()) + " sensors monitored, enabled=" + (enabled_ ? "true" : "false");
    }
    void reset() { std::lock_guard<std::mutex> lock(mutex_); sensors_.clear(); }

    void recordTemperature(const std::string& sensor, double celsius) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto& data = sensors_[sensor];
        data.readings.push_back(celsius);
        data.sum += celsius;
        ++data.count;
        if (celsius > data.max_temp) data.max_temp = celsius;
        while (data.readings.size() > 64) data.readings.erase(data.readings.begin());
    }

    double getCurrentTemp(const std::string& sensor) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = sensors_.find(sensor);
        if (it == sensors_.end() || it->second.readings.empty()) return 0.0;
        return it->second.readings.back();
    }

    double getMaxTemp(const std::string& sensor) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = sensors_.find(sensor);
        if (it == sensors_.end()) return 0.0;
        return it->second.max_temp;
    }

    double getAverageTemp(const std::string& sensor) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = sensors_.find(sensor);
        if (it == sensors_.end() || it->second.count == 0) return 0.0;
        return it->second.sum / it->second.count;
    }

    bool isThrottling(const std::string& sensor, double threshold) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = sensors_.find(sensor);
        if (it == sensors_.end() || it->second.count == 0) return false;
        return (it->second.sum / it->second.count) > threshold;
    }

private:
    ThermalMonitor() = default;
    ~ThermalMonitor() = default;
    ThermalMonitor(const ThermalMonitor&) = delete;
    ThermalMonitor& operator=(const ThermalMonitor&) = delete;
    ThermalMonitor(ThermalMonitor&&) = delete;
    ThermalMonitor& operator=(ThermalMonitor&&) = delete;

    mutable std::mutex mutex_;
    bool enabled_ = false;
    std::map<std::string, SensorData> sensors_;
};

} // namespace nexus::utility::hardware
