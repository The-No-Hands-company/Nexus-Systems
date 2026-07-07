#pragma once

#include <cmath>
#include <mutex>
#include <string>

namespace nexus::utility::financial {

class TickPrecisionValidator {
public:
    static TickPrecisionValidator& instance() {
        static TickPrecisionValidator inst;
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
        if (!enabled_) return "TickPrecisionValidator disabled";
        return "TickPrecisionValidator active, tick_size=" + std::to_string(tick_size_);
    }

    void reset() {
        std::lock_guard<std::mutex> lock(mutex_);
        tick_size_ = 0.01;
    }

    void setTickSize(double tick) {
        std::lock_guard<std::mutex> lock(mutex_);
        tick_size_ = tick;
    }

    bool isValidTick(double price) const {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!enabled_) return true;
        if (tick_size_ <= 0.0) return true;
        double remainder = std::fmod(price, tick_size_);
        return std::abs(remainder) < epsilon_ || std::abs(remainder - tick_size_) < epsilon_;
    }

    double snapToTick(double price) const {
        std::lock_guard<std::mutex> lock(mutex_);
        if (tick_size_ <= 0.0) return price;
        return std::round(price / tick_size_) * tick_size_;
    }

private:
    TickPrecisionValidator() = default;
    ~TickPrecisionValidator() = default;

    TickPrecisionValidator(const TickPrecisionValidator&) = delete;
    TickPrecisionValidator& operator=(const TickPrecisionValidator&) = delete;
    TickPrecisionValidator(TickPrecisionValidator&&) = delete;
    TickPrecisionValidator& operator=(TickPrecisionValidator&&) = delete;

    mutable std::mutex mutex_;
    bool enabled_ = false;
    std::string config_;
    double tick_size_ = 0.01;
    double epsilon_ = 1e-9;
};

} // namespace nexus::utility::financial
