#pragma once

#include <string>
#include <vector>
#include <map>
#include <functional>
#include <mutex>
#include <cstddef>

namespace nexus::utility::meta {

/**
 * @brief Emergency diagnostics collector for crash/panic situations.
 *
 * Components register named "dumpers" (function<string()>) that produce a
 * snapshot of their state. When an emergency is triggered, dumpState() invokes
 * every registered dumper and returns a concatenated report. An emergency-mode
 * flag can be set to signal degraded operation.
 *
 * Thread safety: all public methods are protected by a mutex.
 *
 * @category Meta
 * @version 1.0.0
 */
class EmergencyDiagnostics {
public:
    using Dumper = std::function<std::string()>;

    /// @brief Get singleton instance
    static EmergencyDiagnostics& instance() {
        static EmergencyDiagnostics inst;
        return inst;
    }

    /// @brief Initialize the utility
    void initialize(const std::string& config = "") {
        std::lock_guard<std::mutex> lock(mutex_);
        config_ = config;
        enabled_ = true;
        emergency_mode_ = false;
    }

    /// @brief Shutdown the utility and cleanup resources
    void shutdown() {
        std::lock_guard<std::mutex> lock(mutex_);
        enabled_ = false;
        dumpers_.clear();
        order_.clear();
    }

    /// @brief Check if the utility is currently enabled
    bool isEnabled() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return enabled_;
    }

    /// @brief Enable the utility
    void enable() {
        std::lock_guard<std::mutex> lock(mutex_);
        enabled_ = true;
    }

    /// @brief Disable the utility
    void disable() {
        std::lock_guard<std::mutex> lock(mutex_);
        enabled_ = false;
    }

    // ── Domain methods ──────────────────────────────────────────────────────

    /// @brief Enter emergency mode.
    void setEmergencyMode() {
        std::lock_guard<std::mutex> lock(mutex_);
        emergency_mode_ = true;
    }

    /// @brief Leave emergency mode.
    void clearEmergencyMode() {
        std::lock_guard<std::mutex> lock(mutex_);
        emergency_mode_ = false;
    }

    /// @brief Whether emergency mode is currently active.
    bool isEmergencyMode() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return emergency_mode_;
    }

    /// @brief Register a named state dumper. Re-registering a name replaces its
    /// dumper but keeps its position.
    void registerDumper(const std::string& name, Dumper dumper) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (dumpers_.find(name) == dumpers_.end()) {
            order_.push_back(name);
        }
        dumpers_[name] = std::move(dumper);
    }

    /// @brief Invoke all registered dumpers and concatenate their output into a
    /// single report, each section prefixed with "=== <name> ===".
    std::string dumpState() {
        std::vector<std::string> names;
        std::vector<Dumper> fns;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            names = order_;
            fns.reserve(order_.size());
            for (const auto& name : order_) {
                fns.push_back(dumpers_[name]);
            }
        }
        std::string report;
        for (std::size_t i = 0; i < names.size(); ++i) {
            report += "=== " + names[i] + " ===\n";
            if (fns[i]) report += fns[i]();
            report += "\n";
        }
        return report;
    }

    /// @brief Number of registered dumpers.
    std::size_t getDumperCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return dumpers_.size();
    }

    /// @brief Get utility statistics/status
    std::string getStatus() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return "EmergencyDiagnostics[dumpers=" + std::to_string(dumpers_.size()) +
               ", emergency=" + std::string(emergency_mode_ ? "true" : "false") + "]";
    }

    /// @brief Reset utility state
    void reset() {
        std::lock_guard<std::mutex> lock(mutex_);
        dumpers_.clear();
        order_.clear();
        emergency_mode_ = false;
    }

private:
    EmergencyDiagnostics() = default;
    ~EmergencyDiagnostics() = default;

    EmergencyDiagnostics(const EmergencyDiagnostics&) = delete;
    EmergencyDiagnostics& operator=(const EmergencyDiagnostics&) = delete;
    EmergencyDiagnostics(EmergencyDiagnostics&&) = delete;
    EmergencyDiagnostics& operator=(EmergencyDiagnostics&&) = delete;

    mutable std::mutex mutex_;
    bool enabled_ = false;
    bool emergency_mode_ = false;
    std::string config_;
    std::map<std::string, Dumper> dumpers_;
    std::vector<std::string> order_;
};

} // namespace nexus::utility::meta
