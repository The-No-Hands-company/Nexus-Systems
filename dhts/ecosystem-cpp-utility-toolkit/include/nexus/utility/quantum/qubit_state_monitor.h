#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <cmath>
#include <atomic>
#include <mutex>

namespace nexus::utility::quantum {

/**
 * @brief Track qubit state measurements and statistics.
 */
class QubitStateMonitor {
public:
    struct QubitStats {
        std::size_t qubit = 0;
        std::size_t measurements = 0;
        std::size_t onesMeasured = 0;
        double probabilityOne() const {
            return measurements ? static_cast<double>(onesMeasured) / measurements : 0.0;
        }
    };

    static QubitStateMonitor& instance() {
        static QubitStateMonitor inst;
        return inst;
    }

    void initialize(const std::string& config = "") {
        std::lock_guard<std::mutex> lk(mutex_);
        config_ = config;
        enabled_ = true;
    }
    void shutdown() { enabled_ = false; }
    bool isEnabled() const { return enabled_.load(); }

    /// Record a computational-basis measurement (0 or 1) for a qubit.
    void recordMeasurement(std::size_t qubit, int result) {
        std::lock_guard<std::mutex> lk(mutex_);
        auto& s = stats_[qubit];
        s.qubit = qubit;
        ++s.measurements;
        if (result != 0) ++s.onesMeasured;
        ++total_;
    }

    QubitStats stats(std::size_t qubit) const {
        std::lock_guard<std::mutex> lk(mutex_);
        auto it = stats_.find(qubit);
        return it == stats_.end() ? QubitStats{qubit, 0, 0} : it->second;
    }

    /// Shannon entropy (bits) of a qubit's measured distribution.
    double entropy(std::size_t qubit) const {
        auto s = stats(qubit);
        double p = s.probabilityOne();
        if (p <= 0.0 || p >= 1.0) return 0.0;
        return -(p * std::log2(p) + (1 - p) * std::log2(1 - p));
    }

    /// A qubit measuring ~50/50 is likely in a superposition state.
    bool likelySuperposition(std::size_t qubit, double tolerance = 0.1) const {
        auto s = stats(qubit);
        return s.measurements >= 10 && std::fabs(s.probabilityOne() - 0.5) <= tolerance;
    }

    std::size_t totalMeasurements() const { return total_.load(); }

    std::vector<QubitStats> allStats() const {
        std::lock_guard<std::mutex> lk(mutex_);
        std::vector<QubitStats> out;
        for (const auto& [q, s] : stats_) out.push_back(s);
        return out;
    }

    void reset() {
        std::lock_guard<std::mutex> lk(mutex_);
        stats_.clear();
        total_ = 0;
    }

private:
    QubitStateMonitor() = default;
    ~QubitStateMonitor() = default;
    QubitStateMonitor(const QubitStateMonitor&) = delete;
    QubitStateMonitor& operator=(const QubitStateMonitor&) = delete;

    mutable std::mutex mutex_;
    std::atomic<bool> enabled_{false};
    std::string config_;
    std::unordered_map<std::size_t, QubitStats> stats_;
    std::atomic<std::size_t> total_{0};
};

} // namespace nexus::utility::quantum
