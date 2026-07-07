#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <atomic>
#include <mutex>

namespace nexus::utility::lockfree {

/**
 * @brief Trace compare-and-swap (CAS) operations and success/failure rates.
 */
class CompareExchangeTracer {
public:
    static CompareExchangeTracer& instance() {
        static CompareExchangeTracer inst;
        return inst;
    }

    void initialize(const std::string& config = "") {
        std::lock_guard<std::mutex> lk(mutex_);
        config_ = config;
        enabled_ = true;
    }
    void shutdown() { enabled_ = false; }
    bool isEnabled() const { return enabled_.load(); }

    struct CasStats {
        std::string variable;
        std::size_t attempts = 0;
        std::size_t successes = 0;
        std::size_t failures = 0;
        double successRate() const { return attempts ? static_cast<double>(successes) / attempts : 0.0; }
    };

    void recordCas(const std::string& variable, bool success) {
        std::lock_guard<std::mutex> lk(mutex_);
        auto& s = stats_[variable];
        s.variable = variable;
        ++s.attempts;
        if (success) ++s.successes; else ++s.failures;
        ++totalAttempts_;
        if (success) ++totalSuccesses_;
    }

    CasStats stats(const std::string& variable) const {
        std::lock_guard<std::mutex> lk(mutex_);
        auto it = stats_.find(variable);
        return it == stats_.end() ? CasStats{variable, 0, 0, 0} : it->second;
    }

    double overallSuccessRate() const {
        std::size_t a = totalAttempts_.load(), s = totalSuccesses_.load();
        return a ? static_cast<double>(s) / a : 0.0;
    }

    /// High CAS contention is indicated by a low success rate.
    bool highContention(const std::string& variable, double threshold = 0.5) const {
        auto s = stats(variable);
        return s.attempts > 0 && s.successRate() < threshold;
    }

    std::vector<CasStats> allStats() const {
        std::lock_guard<std::mutex> lk(mutex_);
        std::vector<CasStats> out;
        for (const auto& [v, s] : stats_) out.push_back(s);
        return out;
    }

    void reset() {
        std::lock_guard<std::mutex> lk(mutex_);
        stats_.clear();
        totalAttempts_ = 0;
        totalSuccesses_ = 0;
    }

private:
    CompareExchangeTracer() = default;
    ~CompareExchangeTracer() = default;
    CompareExchangeTracer(const CompareExchangeTracer&) = delete;
    CompareExchangeTracer& operator=(const CompareExchangeTracer&) = delete;

    mutable std::mutex mutex_;
    std::atomic<bool> enabled_{false};
    std::string config_;
    std::unordered_map<std::string, CasStats> stats_;
    std::atomic<std::size_t> totalAttempts_{0};
    std::atomic<std::size_t> totalSuccesses_{0};
};

} // namespace nexus::utility::lockfree
