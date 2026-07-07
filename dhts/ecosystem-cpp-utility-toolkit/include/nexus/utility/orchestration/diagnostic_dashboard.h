#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <atomic>
#include <mutex>

namespace nexus::utility::orchestration {

/**
 * @brief Register named metrics and track their current values for a dashboard.
 */
class DiagnosticDashboard {
public:
    struct Metric {
        std::string name;
        std::string unit;
        double value = 0.0;
        double min = 0.0;
        double max = 0.0;
        std::size_t updates = 0;
    };

    static DiagnosticDashboard& instance() {
        static DiagnosticDashboard inst;
        return inst;
    }

    void initialize(const std::string& config = "") {
        std::lock_guard<std::mutex> lk(mutex_);
        config_ = config;
        enabled_ = true;
    }
    void shutdown() { enabled_ = false; }
    bool isEnabled() const { return enabled_.load(); }

    void registerMetric(const std::string& name, const std::string& unit = "") {
        std::lock_guard<std::mutex> lk(mutex_);
        auto& m = metrics_[name];
        m.name = name;
        m.unit = unit;
    }

    void update(const std::string& name, double value) {
        std::lock_guard<std::mutex> lk(mutex_);
        auto& m = metrics_[name];
        if (m.updates == 0) {
            m.name = name;
            m.min = m.max = value;
        } else {
            if (value < m.min) m.min = value;
            if (value > m.max) m.max = value;
        }
        m.value = value;
        ++m.updates;
    }

    Metric get(const std::string& name) const {
        std::lock_guard<std::mutex> lk(mutex_);
        auto it = metrics_.find(name);
        return it == metrics_.end() ? Metric{name, "", 0, 0, 0, 0} : it->second;
    }

    std::vector<Metric> all() const {
        std::lock_guard<std::mutex> lk(mutex_);
        std::vector<Metric> out;
        for (const auto& [k, m] : metrics_) out.push_back(m);
        return out;
    }

    void reset() {
        std::lock_guard<std::mutex> lk(mutex_);
        metrics_.clear();
    }

private:
    DiagnosticDashboard() = default;
    ~DiagnosticDashboard() = default;
    DiagnosticDashboard(const DiagnosticDashboard&) = delete;
    DiagnosticDashboard& operator=(const DiagnosticDashboard&) = delete;

    mutable std::mutex mutex_;
    std::atomic<bool> enabled_{false};
    std::string config_;
    std::unordered_map<std::string, Metric> metrics_;
};

} // namespace nexus::utility::orchestration
