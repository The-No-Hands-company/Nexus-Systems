#pragma once

#include <string>
#include <vector>
#include <numeric>
#include <algorithm>
#include <cmath>
#include <atomic>
#include <mutex>

namespace nexus::utility::ml {

/**
 * @brief Validate tensor dimensions and value ranges.
 */
class TensorValidator {
public:
    struct Validation {
        std::string tensor;
        bool shapeOk = true;
        bool rangeOk = true;
        double minValue = 0.0;
        double maxValue = 0.0;
    };

    static TensorValidator& instance() {
        static TensorValidator inst;
        return inst;
    }

    void initialize(const std::string& config = "") {
        std::lock_guard<std::mutex> lk(mutex_);
        config_ = config;
        enabled_ = true;
    }
    void shutdown() { enabled_ = false; }
    bool isEnabled() const { return enabled_.load(); }

    /// Element count implied by a shape.
    static std::size_t elementCount(const std::vector<std::size_t>& shape) {
        return std::accumulate(shape.begin(), shape.end(), std::size_t{1},
                               std::multiplies<std::size_t>());
    }

    /// Validate that data size matches shape and values are within [lo, hi].
    Validation validate(const std::string& tensor, const std::vector<double>& data,
                        const std::vector<std::size_t>& shape,
                        double lo = -1e30, double hi = 1e30) {
        Validation v;
        v.tensor = tensor;
        v.shapeOk = (elementCount(shape) == data.size());
        if (!data.empty()) {
            v.minValue = v.maxValue = data.front();
            for (double x : data) {
                if (x < v.minValue) v.minValue = x;
                if (x > v.maxValue) v.maxValue = x;
            }
            v.rangeOk = (v.minValue >= lo && v.maxValue <= hi);
        }
        std::lock_guard<std::mutex> lk(mutex_);
        ++validations_;
        if (!v.shapeOk || !v.rangeOk) ++failures_;
        return v;
    }

    /// Verify two shapes are broadcast-compatible (numpy rules).
    static bool broadcastable(const std::vector<std::size_t>& a,
                              const std::vector<std::size_t>& b) {
        std::size_t n = std::max(a.size(), b.size());
        for (std::size_t i = 0; i < n; ++i) {
            std::size_t da = i < a.size() ? a[a.size() - 1 - i] : 1;
            std::size_t db = i < b.size() ? b[b.size() - 1 - i] : 1;
            if (da != db && da != 1 && db != 1) return false;
        }
        return true;
    }

    std::size_t validations() const { return validations_.load(); }
    std::size_t failures() const { return failures_.load(); }

    void reset() { validations_ = 0; failures_ = 0; }

private:
    TensorValidator() = default;
    ~TensorValidator() = default;
    TensorValidator(const TensorValidator&) = delete;
    TensorValidator& operator=(const TensorValidator&) = delete;

    mutable std::mutex mutex_;
    std::atomic<bool> enabled_{false};
    std::string config_;
    std::atomic<std::size_t> validations_{0};
    std::atomic<std::size_t> failures_{0};
};

} // namespace nexus::utility::ml
