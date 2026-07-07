#pragma once

#include <string>
#include <vector>
#include <cmath>
#include <limits>
#include <atomic>
#include <mutex>

namespace nexus::utility::scientific {

/**
 * @brief Estimate matrix condition number from row/column norms.
 *
 * Provides a cheap condition-number estimate (ratio of max to min absolute
 * row-sum norms) to flag ill-conditioned systems before solving.
 */
class MatrixConditionChecker {
public:
    struct ConditionEstimate {
        double maxNorm = 0.0;
        double minNorm = 0.0;
        double estimate = 0.0;   // maxNorm / minNorm
        bool illConditioned = false;
    };

    static MatrixConditionChecker& instance() {
        static MatrixConditionChecker inst;
        return inst;
    }

    void initialize(const std::string& config = "") {
        std::lock_guard<std::mutex> lk(mutex_);
        config_ = config;
        enabled_ = true;
    }
    void shutdown() { enabled_ = false; }
    bool isEnabled() const { return enabled_.load(); }

    void setThreshold(double threshold) {
        std::lock_guard<std::mutex> lk(mutex_);
        threshold_ = threshold;
    }

    /// Estimate the condition number from a dense row-major matrix.
    ConditionEstimate estimate(const std::vector<std::vector<double>>& matrix) const {
        ConditionEstimate e;
        double maxNorm = 0.0;
        double minNorm = std::numeric_limits<double>::max();
        for (const auto& row : matrix) {
            double sum = 0.0;
            for (double v : row) sum += std::fabs(v);
            if (sum > maxNorm) maxNorm = sum;
            if (sum < minNorm) minNorm = sum;
        }
        if (matrix.empty()) minNorm = 0.0;
        e.maxNorm = maxNorm;
        e.minNorm = minNorm;
        e.estimate = (minNorm > 0.0) ? maxNorm / minNorm
                                     : std::numeric_limits<double>::infinity();
        double th;
        { std::lock_guard<std::mutex> lk(mutex_); th = threshold_; }
        e.illConditioned = e.estimate > th;
        return e;
    }

    void reset() {}

private:
    MatrixConditionChecker() = default;
    ~MatrixConditionChecker() = default;
    MatrixConditionChecker(const MatrixConditionChecker&) = delete;
    MatrixConditionChecker& operator=(const MatrixConditionChecker&) = delete;

    mutable std::mutex mutex_;
    std::atomic<bool> enabled_{false};
    std::string config_;
    double threshold_ = 1e12;
};

} // namespace nexus::utility::scientific
