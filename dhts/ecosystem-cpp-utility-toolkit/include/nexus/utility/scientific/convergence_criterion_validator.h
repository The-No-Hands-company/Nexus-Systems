#pragma once

#include <string>
#include <vector>
#include <cmath>
#include <atomic>
#include <mutex>

namespace nexus::utility::scientific {

/**
 * @brief Validate convergence of iterative numerical methods.
 */
class ConvergenceCriterionValidator {
public:
    enum class Criterion { AbsoluteResidual, RelativeResidual, StepSize };

    struct Result {
        bool converged = false;
        double value = 0.0;
        double tolerance = 0.0;
        std::size_t iterations = 0;
    };

    static ConvergenceCriterionValidator& instance() {
        static ConvergenceCriterionValidator inst;
        return inst;
    }

    void initialize(const std::string& config = "") {
        std::lock_guard<std::mutex> lk(mutex_);
        config_ = config;
        enabled_ = true;
    }
    void shutdown() { enabled_ = false; }
    bool isEnabled() const { return enabled_.load(); }

    void configure(Criterion criterion, double tolerance, std::size_t maxIterations) {
        std::lock_guard<std::mutex> lk(mutex_);
        criterion_ = criterion;
        tolerance_ = tolerance;
        maxIterations_ = maxIterations;
    }

    /// Record a residual/step value for the current iteration; returns convergence result.
    Result recordIteration(double currentResidual, double initialResidual = 1.0) {
        std::lock_guard<std::mutex> lk(mutex_);
        ++iterations_;
        double metric = currentResidual;
        if (criterion_ == Criterion::RelativeResidual && initialResidual != 0.0)
            metric = std::fabs(currentResidual / initialResidual);
        else
            metric = std::fabs(currentResidual);
        lastMetric_ = metric;
        Result r;
        r.value = metric;
        r.tolerance = tolerance_;
        r.iterations = iterations_;
        r.converged = metric <= tolerance_;
        return r;
    }

    bool diverged() const {
        std::lock_guard<std::mutex> lk(mutex_);
        return iterations_ >= maxIterations_ && lastMetric_ > tolerance_;
    }

    void reset() {
        std::lock_guard<std::mutex> lk(mutex_);
        iterations_ = 0;
        lastMetric_ = 0.0;
    }

private:
    ConvergenceCriterionValidator() = default;
    ~ConvergenceCriterionValidator() = default;
    ConvergenceCriterionValidator(const ConvergenceCriterionValidator&) = delete;
    ConvergenceCriterionValidator& operator=(const ConvergenceCriterionValidator&) = delete;

    mutable std::mutex mutex_;
    std::atomic<bool> enabled_{false};
    std::string config_;
    Criterion criterion_ = Criterion::RelativeResidual;
    double tolerance_ = 1e-6;
    std::size_t maxIterations_ = 1000;
    std::size_t iterations_ = 0;
    double lastMetric_ = 0.0;
};

} // namespace nexus::utility::scientific
