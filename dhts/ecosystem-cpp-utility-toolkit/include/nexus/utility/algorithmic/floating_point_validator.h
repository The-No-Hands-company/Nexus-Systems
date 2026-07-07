#pragma once

#include <algorithm>
#include <cmath>
#include <cfloat>
#include <cstdint>
#include <cstring>
#include <limits>
#include <mutex>
#include <string>
#include <vector>

namespace nexus::utility::algorithmic {

class FloatingPointValidator {
public:
    static FloatingPointValidator& instance() {
        static FloatingPointValidator inst;
        return inst;
    }

    void initialize() { std::lock_guard<std::mutex> lock(mutex_); enabled_ = true; }
    void shutdown() { std::lock_guard<std::mutex> lock(mutex_); enabled_ = false; }
    bool isEnabled() const { return enabled_; }

    bool isNearlyEqual(double a, double b, double epsilon) {
        return std::abs(a - b) < epsilon;
    }

    bool hasNaN(const std::vector<double>& data) {
        for (double v : data) {
            if (std::isnan(v)) return true;
        }
        return false;
    }

    bool hasInf(const std::vector<double>& data) {
        for (double v : data) {
            if (std::isinf(v)) return true;
        }
        return false;
    }

    uint64_t getUlpDifference(double a, double b) {
        if (a == b) return 0;
        if (std::isnan(a) || std::isnan(b)) return UINT64_MAX;
        if (std::isinf(a) || std::isinf(b)) return UINT64_MAX;

        int64_t ia = 0;
        int64_t ib = 0;
        std::memcpy(&ia, &a, sizeof(ia));
        std::memcpy(&ib, &b, sizeof(ib));

        if ((ia < 0) != (ib < 0)) return UINT64_MAX;
        return static_cast<uint64_t>(std::abs(ia - ib));
    }

private:
    FloatingPointValidator() = default;
    ~FloatingPointValidator() = default;
    FloatingPointValidator(const FloatingPointValidator&) = delete;
    FloatingPointValidator& operator=(const FloatingPointValidator&) = delete;

    mutable std::mutex mutex_;
    bool enabled_ = false;
};

} // namespace nexus::utility::algorithmic
