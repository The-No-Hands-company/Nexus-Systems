#pragma once

#include <cstdint>
#include <mutex>
#include <string>

namespace nexus::utility::hardware {

class BatteryDiagnostics {
public:
    static BatteryDiagnostics& instance() {
        static BatteryDiagnostics inst;
        return inst;
    }

    void initialize(const std::string& = "") { std::lock_guard<std::mutex> lock(mutex_); enabled_ = true; }
    void shutdown() { std::lock_guard<std::mutex> lock(mutex_); enabled_ = false; reset(); }
    bool isEnabled() const { return enabled_; }
    void enable() { std::lock_guard<std::mutex> lock(mutex_); enabled_ = true; }
    void disable() { std::lock_guard<std::mutex> lock(mutex_); enabled_ = false; }

    std::string getStatus() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return "BatteryDiagnostics: " + std::to_string(reading_count_) + " readings, enabled=" + (enabled_ ? "true" : "false");
    }
    void reset() {
        std::lock_guard<std::mutex> lock(mutex_);
        voltage_sum_ = current_sum_ = temp_sum_ = 0.0;
        reading_count_ = 0;
    }

    void recordReading(double voltage, double current, double temp) {
        std::lock_guard<std::mutex> lock(mutex_);
        voltage_sum_ += voltage;
        current_sum_ += current;
        temp_sum_ += temp;
        ++reading_count_;
    }

    double getVoltage() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return reading_count_ > 0 ? voltage_sum_ / reading_count_ : 0.0;
    }

    double getCurrent() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return reading_count_ > 0 ? current_sum_ / reading_count_ : 0.0;
    }

    double getTemperature() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return reading_count_ > 0 ? temp_sum_ / reading_count_ : 0.0;
    }

    double getPower() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return reading_count_ > 0 ? (voltage_sum_ / reading_count_) * (current_sum_ / reading_count_) : 0.0;
    }

    double getHealth() const {
        std::lock_guard<std::mutex> lock(mutex_);
        if (reading_count_ == 0) return 1.0;
        double avg_v = voltage_sum_ / reading_count_;
        double avg_t = temp_sum_ / reading_count_;
        double voltage_health = std::max(0.0, 1.0 - std::abs(avg_v - 3.7) / 3.7);
        double temp_health = std::max(0.0, 1.0 - std::max(0.0, avg_t - 25.0) / 60.0);
        return (voltage_health + temp_health) / 2.0;
    }

private:
    BatteryDiagnostics() = default;
    ~BatteryDiagnostics() = default;
    BatteryDiagnostics(const BatteryDiagnostics&) = delete;
    BatteryDiagnostics& operator=(const BatteryDiagnostics&) = delete;
    BatteryDiagnostics(BatteryDiagnostics&&) = delete;
    BatteryDiagnostics& operator=(BatteryDiagnostics&&) = delete;

    mutable std::mutex mutex_;
    bool enabled_ = false;
    double voltage_sum_ = 0.0;
    double current_sum_ = 0.0;
    double temp_sum_ = 0.0;
    uint64_t reading_count_ = 0;
};

} // namespace nexus::utility::hardware
