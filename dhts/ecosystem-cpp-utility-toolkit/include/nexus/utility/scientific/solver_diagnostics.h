#pragma once

#include <string>
#include <vector>
#include <cmath>
#include <atomic>
#include <mutex>

namespace nexus::utility::scientific {

/**
 * @brief Track iterations and residuals of an iterative solver.
 */
class SolverDiagnostics {
public:
    struct IterationRecord {
        std::size_t iteration = 0;
        double residual = 0.0;
    };

    struct Summary {
        std::size_t iterations = 0;
        double initialResidual = 0.0;
        double finalResidual = 0.0;
        double reductionFactor = 0.0;   // final / initial
        bool converged = false;
    };

    static SolverDiagnostics& instance() {
        static SolverDiagnostics inst;
        return inst;
    }

    void initialize(const std::string& config = "") {
        std::lock_guard<std::mutex> lk(mutex_);
        config_ = config;
        enabled_ = true;
    }
    void shutdown() { enabled_ = false; }
    bool isEnabled() const { return enabled_.load(); }

    void setTolerance(double tol) {
        std::lock_guard<std::mutex> lk(mutex_);
        tolerance_ = tol;
    }

    void recordIteration(double residual) {
        std::lock_guard<std::mutex> lk(mutex_);
        history_.push_back({history_.size() + 1, residual});
    }

    Summary summary() const {
        std::lock_guard<std::mutex> lk(mutex_);
        Summary s;
        s.iterations = history_.size();
        if (!history_.empty()) {
            s.initialResidual = history_.front().residual;
            s.finalResidual = history_.back().residual;
            s.reductionFactor = s.initialResidual != 0.0
                ? s.finalResidual / s.initialResidual : 0.0;
            s.converged = s.finalResidual <= tolerance_;
        }
        return s;
    }

    /// Estimated average convergence rate per iteration.
    double convergenceRate() const {
        std::lock_guard<std::mutex> lk(mutex_);
        if (history_.size() < 2) return 0.0;
        double r0 = history_.front().residual;
        double rn = history_.back().residual;
        if (r0 <= 0.0 || rn <= 0.0) return 0.0;
        return std::pow(rn / r0, 1.0 / (history_.size() - 1));
    }

    std::vector<IterationRecord> history() const {
        std::lock_guard<std::mutex> lk(mutex_);
        return history_;
    }

    void reset() {
        std::lock_guard<std::mutex> lk(mutex_);
        history_.clear();
    }

private:
    SolverDiagnostics() = default;
    ~SolverDiagnostics() = default;
    SolverDiagnostics(const SolverDiagnostics&) = delete;
    SolverDiagnostics& operator=(const SolverDiagnostics&) = delete;

    mutable std::mutex mutex_;
    std::atomic<bool> enabled_{false};
    std::string config_;
    double tolerance_ = 1e-6;
    std::vector<IterationRecord> history_;
};

} // namespace nexus::utility::scientific
