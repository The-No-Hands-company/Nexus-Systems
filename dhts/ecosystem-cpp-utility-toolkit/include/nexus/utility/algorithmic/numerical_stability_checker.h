#pragma once

#include <cmath>
#include <limits>
#include <mutex>
#include <string>
#include <vector>

namespace nexus::utility::algorithmic {

class NumericalStabilityChecker {
public:
    static NumericalStabilityChecker& instance() {
        static NumericalStabilityChecker inst;
        return inst;
    }

    void initialize() { std::lock_guard<std::mutex> lock(mutex_); enabled_ = true; }
    void shutdown() { std::lock_guard<std::mutex> lock(mutex_); enabled_ = false; }
    bool isEnabled() const { return enabled_; }

    double checkCancellation(double a, double b) {
        double sum = a + b;
        if (std::abs(a) > 0.0 && std::abs(b) > 0.0 && std::abs(sum) < std::abs(a) * 1e-15) {
            return 0.0;
        }
        double cancellation = (std::abs(a) + std::abs(b) - std::abs(sum)) /
                              (std::abs(a) + std::abs(b) + 1e-300);
        return cancellation;
    }

    double checkConditionNumber(double value) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (value == 0.0) {
            condition_number_ = std::numeric_limits<double>::infinity();
        } else {
            condition_number_ = std::abs(1.0 / value);
        }
        return condition_number_;
    }

    bool isWellConditioned() {
        std::lock_guard<std::mutex> lock(mutex_);
        return condition_number_ < 1e6;
    }

private:
    NumericalStabilityChecker() = default;
    ~NumericalStabilityChecker() = default;
    NumericalStabilityChecker(const NumericalStabilityChecker&) = delete;
    NumericalStabilityChecker& operator=(const NumericalStabilityChecker&) = delete;

    mutable std::mutex mutex_;
    bool enabled_ = false;
    double condition_number_ = 0.0;
};

} // namespace nexus::utility::algorithmic
