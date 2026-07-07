#pragma once

#include <cmath>
#include <limits>
#include <mutex>
#include <string>

namespace nexus::utility::financial {

class DecimalPrecisionValidator {
public:
    static DecimalPrecisionValidator& instance() {
        static DecimalPrecisionValidator inst;
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
        return enabled_ ? "DecimalPrecisionValidator active, precision=" + std::to_string(decimal_places_)
                        : "DecimalPrecisionValidator disabled";
    }

    void reset() {
        std::lock_guard<std::mutex> lock(mutex_);
        decimal_places_ = 2;
    }

    void setPrecision(int decimal_places) {
        std::lock_guard<std::mutex> lock(mutex_);
        decimal_places_ = decimal_places;
    }

    bool validate(double value) const {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!enabled_) return true;
        double factor = std::pow(10.0, decimal_places_);
        double scaled = value * factor;
        double rounded = std::round(scaled);
        return std::abs(scaled - rounded) < epsilon_;
    }

    double round(double value) const {
        std::lock_guard<std::mutex> lock(mutex_);
        double factor = std::pow(10.0, decimal_places_);
        return std::round(value * factor) / factor;
    }

private:
    DecimalPrecisionValidator() = default;
    ~DecimalPrecisionValidator() = default;

    DecimalPrecisionValidator(const DecimalPrecisionValidator&) = delete;
    DecimalPrecisionValidator& operator=(const DecimalPrecisionValidator&) = delete;
    DecimalPrecisionValidator(DecimalPrecisionValidator&&) = delete;
    DecimalPrecisionValidator& operator=(DecimalPrecisionValidator&&) = delete;

    mutable std::mutex mutex_;
    bool enabled_ = false;
    std::string config_;
    int decimal_places_ = 2;
    double epsilon_ = 1e-9;
};

} // namespace nexus::utility::financial
