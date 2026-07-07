#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <mutex>
#include <atomic>

namespace nexus::utility::codequality {

/**
 * @brief Track cyclomatic complexity scores for functions.
 */
class CyclomaticComplexityTracker {
public:
    struct FunctionComplexity {
        std::string function;
        int complexity = 1;
        std::string riskLevel() const {
            if (complexity <= 10) return "low";
            if (complexity <= 20) return "moderate";
            if (complexity <= 50) return "high";
            return "very-high";
        }
    };

    static CyclomaticComplexityTracker& instance() {
        static CyclomaticComplexityTracker inst;
        return inst;
    }

    void initialize(const std::string& config = "") {
        std::lock_guard<std::mutex> lk(mutex_);
        config_ = config;
        enabled_ = true;
    }
    void shutdown() { enabled_ = false; }
    bool isEnabled() const { return enabled_.load(); }

    void record(const std::string& function, int complexity) {
        std::lock_guard<std::mutex> lk(mutex_);
        scores_[function] = complexity;
    }

    int complexity(const std::string& function) const {
        std::lock_guard<std::mutex> lk(mutex_);
        auto it = scores_.find(function);
        return it == scores_.end() ? 0 : it->second;
    }

    double averageComplexity() const {
        std::lock_guard<std::mutex> lk(mutex_);
        if (scores_.empty()) return 0.0;
        int sum = 0;
        for (const auto& [f, c] : scores_) sum += c;
        return static_cast<double>(sum) / scores_.size();
    }

    std::vector<FunctionComplexity> aboveThreshold(int threshold) const {
        std::lock_guard<std::mutex> lk(mutex_);
        std::vector<FunctionComplexity> out;
        for (const auto& [f, c] : scores_)
            if (c > threshold) out.push_back({f, c});
        std::sort(out.begin(), out.end(), [](const auto& a, const auto& b) {
            return a.complexity > b.complexity;
        });
        return out;
    }

    void reset() {
        std::lock_guard<std::mutex> lk(mutex_);
        scores_.clear();
    }

private:
    CyclomaticComplexityTracker() = default;
    ~CyclomaticComplexityTracker() = default;
    CyclomaticComplexityTracker(const CyclomaticComplexityTracker&) = delete;
    CyclomaticComplexityTracker& operator=(const CyclomaticComplexityTracker&) = delete;

    mutable std::mutex mutex_;
    std::atomic<bool> enabled_{false};
    std::string config_;
    std::unordered_map<std::string, int> scores_;
};

} // namespace nexus::utility::codequality
