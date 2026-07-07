#pragma once

#include <cstddef>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <vector>

namespace nexus::utility::safety {

/// @brief Catalogs component failure modes and their effects/severity (FMEA-style).
class FailureModeDetector {
public:
    struct FailureMode {
        std::string component;
        std::string mode;
        std::string effect;
        int severity = 0;
    };

    static FailureModeDetector& instance() {
        static FailureModeDetector inst;
        return inst;
    }

    void initialize(const std::string& config = "") {
        std::lock_guard<std::mutex> lock(mutex_);
        enabled_ = true;
        config_ = config;
    }

    void shutdown() {
        std::lock_guard<std::mutex> lock(mutex_);
        enabled_ = false;
        modes_.clear();
        components_.clear();
    }

    bool isEnabled() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return enabled_;
    }

    void addComponent(const std::string& name) {
        std::lock_guard<std::mutex> lock(mutex_);
        components_.insert(name);
    }

    void addFailureMode(const std::string& component, const std::string& mode,
                        const std::string& effect, int severity) {
        std::lock_guard<std::mutex> lock(mutex_);
        components_.insert(component);
        modes_.push_back({component, mode, effect, severity});
    }

    std::vector<FailureMode> getFailureModes(const std::string& component) const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<FailureMode> out;
        for (const auto& m : modes_) {
            if (m.component == component) out.push_back(m);
        }
        return out;
    }

    /// @brief Map of severity level -> number of failure modes with that severity.
    std::map<int, size_t> getSeverityDistribution() const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::map<int, size_t> dist;
        for (const auto& m : modes_) {
            ++dist[m.severity];
        }
        return dist;
    }

    size_t getComponentCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return components_.size();
    }

    void reset() {
        std::lock_guard<std::mutex> lock(mutex_);
        modes_.clear();
        components_.clear();
    }

private:
    FailureModeDetector() = default;
    ~FailureModeDetector() = default;
    FailureModeDetector(const FailureModeDetector&) = delete;
    FailureModeDetector& operator=(const FailureModeDetector&) = delete;

    mutable std::mutex mutex_;
    bool enabled_ = false;
    std::string config_;
    std::vector<FailureMode> modes_;
    std::set<std::string> components_;
};

} // namespace nexus::utility::safety
