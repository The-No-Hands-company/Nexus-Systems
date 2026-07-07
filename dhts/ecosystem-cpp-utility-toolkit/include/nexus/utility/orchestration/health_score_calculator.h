#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <atomic>
#include <mutex>

namespace nexus::utility::orchestration {

/**
 * @brief Compute weighted health scores from component statuses.
 */
class HealthScoreCalculator {
public:
    enum class Status { Healthy, Degraded, Unhealthy };

    struct Component {
        std::string name;
        Status status = Status::Healthy;
        double weight = 1.0;
    };

    static HealthScoreCalculator& instance() {
        static HealthScoreCalculator inst;
        return inst;
    }

    void initialize(const std::string& config = "") {
        std::lock_guard<std::mutex> lk(mutex_);
        config_ = config;
        enabled_ = true;
    }
    void shutdown() { enabled_ = false; }
    bool isEnabled() const { return enabled_.load(); }

    void setComponent(const std::string& name, Status status, double weight = 1.0) {
        std::lock_guard<std::mutex> lk(mutex_);
        components_[name] = {name, status, weight};
    }

    static double statusScore(Status s) {
        switch (s) {
            case Status::Healthy:   return 1.0;
            case Status::Degraded:  return 0.5;
            case Status::Unhealthy: return 0.0;
        }
        return 0.0;
    }

    /// Weighted health score in the range [0, 100].
    double score() const {
        std::lock_guard<std::mutex> lk(mutex_);
        double weightSum = 0.0, weighted = 0.0;
        for (const auto& [name, c] : components_) {
            weightSum += c.weight;
            weighted += c.weight * statusScore(c.status);
        }
        return weightSum == 0.0 ? 100.0 : 100.0 * weighted / weightSum;
    }

    Status overallStatus() const {
        double s = score();
        if (s >= 90.0) return Status::Healthy;
        if (s >= 50.0) return Status::Degraded;
        return Status::Unhealthy;
    }

    void reset() {
        std::lock_guard<std::mutex> lk(mutex_);
        components_.clear();
    }

private:
    HealthScoreCalculator() = default;
    ~HealthScoreCalculator() = default;
    HealthScoreCalculator(const HealthScoreCalculator&) = delete;
    HealthScoreCalculator& operator=(const HealthScoreCalculator&) = delete;

    mutable std::mutex mutex_;
    std::atomic<bool> enabled_{false};
    std::string config_;
    std::unordered_map<std::string, Component> components_;
};

} // namespace nexus::utility::orchestration
