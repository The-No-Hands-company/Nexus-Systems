#pragma once

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

namespace nexus::utility::hardware {

class PowerMonitor {
public:
    struct Reading {
        double watts;
        std::chrono::steady_clock::time_point timestamp;
    };

    static PowerMonitor& instance() {
        static PowerMonitor inst;
        return inst;
    }

    void initialize(const std::string& = "") { std::lock_guard<std::mutex> lock(mutex_); enabled_ = true; }
    void shutdown() { std::lock_guard<std::mutex> lock(mutex_); enabled_ = false; reset(); }
    bool isEnabled() const { return enabled_; }
    void enable() { std::lock_guard<std::mutex> lock(mutex_); enabled_ = true; }
    void disable() { std::lock_guard<std::mutex> lock(mutex_); enabled_ = false; }

    std::string getStatus() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return "PowerMonitor: " + std::to_string(readings_.size()) + " readings, enabled=" + (enabled_ ? "true" : "false");
    }
    void reset() {
        std::lock_guard<std::mutex> lock(mutex_);
        readings_.clear();
        peak_power_ = 0.0;
    }

    void recordReading(double watts) {
        std::lock_guard<std::mutex> lock(mutex_);
        readings_.push_back({watts, std::chrono::steady_clock::now()});
        if (watts > peak_power_) peak_power_ = watts;
        while (readings_.size() > 1024) readings_.erase(readings_.begin());
    }

    double getCurrentPower() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return readings_.empty() ? 0.0 : readings_.back().watts;
    }

    double getAveragePower() const {
        std::lock_guard<std::mutex> lock(mutex_);
        if (readings_.empty()) return 0.0;
        double sum = 0.0;
        for (const auto& r : readings_) sum += r.watts;
        return sum / readings_.size();
    }

    double getPeakPower() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return peak_power_;
    }

    double getTotalEnergy() const {
        std::lock_guard<std::mutex> lock(mutex_);
        if (readings_.size() < 2) return 0.0;
        double energy = 0.0;
        for (size_t i = 1; i < readings_.size(); ++i) {
            double dt = std::chrono::duration_cast<std::chrono::duration<double>>(
                readings_[i].timestamp - readings_[i - 1].timestamp).count();
            energy += readings_[i].watts * dt;
        }
        return energy;
    }

private:
    PowerMonitor() = default;
    ~PowerMonitor() = default;
    PowerMonitor(const PowerMonitor&) = delete;
    PowerMonitor& operator=(const PowerMonitor&) = delete;
    PowerMonitor(PowerMonitor&&) = delete;
    PowerMonitor& operator=(PowerMonitor&&) = delete;

    mutable std::mutex mutex_;
    bool enabled_ = false;
    std::vector<Reading> readings_;
    double peak_power_ = 0.0;
};

} // namespace nexus::utility::hardware
