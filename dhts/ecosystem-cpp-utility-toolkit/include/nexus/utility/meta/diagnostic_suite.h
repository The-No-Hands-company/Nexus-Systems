#pragma once

#include <string>
#include <map>
#include <vector>
#include <functional>
#include <mutex>
#include <cstddef>

namespace nexus::utility::meta {

/**
 * @brief A suite of named diagnostics that can be individually enabled, disabled
 * and executed.
 *
 * Each diagnostic is registered with a predicate (function<bool()>) that returns
 * true when the diagnostic passes. Diagnostics can be toggled on/off and only
 * enabled diagnostics are run.
 *
 * Thread safety: all public methods are protected by a mutex.
 *
 * @category Meta
 * @version 1.0.0
 */
class DiagnosticSuite {
public:
    using Check = std::function<bool()>;

    /// @brief Get singleton instance
    static DiagnosticSuite& instance() {
        static DiagnosticSuite inst;
        return inst;
    }

    /// @brief Initialize the utility
    void initialize(const std::string& config = "") {
        std::lock_guard<std::mutex> lock(mutex_);
        config_ = config;
        enabled_ = true;
    }

    /// @brief Shutdown the utility and cleanup resources
    void shutdown() {
        std::lock_guard<std::mutex> lock(mutex_);
        enabled_ = false;
        diagnostics_.clear();
    }

    /// @brief Check if the whole suite is currently enabled
    bool isEnabled() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return enabled_;
    }

    /// @brief Enable the suite
    void enable() {
        std::lock_guard<std::mutex> lock(mutex_);
        enabled_ = true;
    }

    /// @brief Disable the suite
    void disable() {
        std::lock_guard<std::mutex> lock(mutex_);
        enabled_ = false;
    }

    // ── Domain methods ──────────────────────────────────────────────────────

    /// @brief Register a diagnostic with a check predicate. If no check is
    /// provided the diagnostic is treated as always passing.
    void registerDiagnostic(const std::string& name, Check check = {}) {
        std::lock_guard<std::mutex> lock(mutex_);
        diagnostics_[name] = Entry{true, std::move(check)};
    }

    /// @brief Enable a named diagnostic (registering it if unknown).
    void enableDiagnostic(const std::string& name) {
        std::lock_guard<std::mutex> lock(mutex_);
        diagnostics_[name].enabled = true;
    }

    /// @brief Disable a named diagnostic (registering it if unknown).
    void disableDiagnostic(const std::string& name) {
        std::lock_guard<std::mutex> lock(mutex_);
        diagnostics_[name].enabled = false;
    }

    /// @brief Check whether a named diagnostic is enabled.
    bool isEnabled(const std::string& name) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = diagnostics_.find(name);
        return it != diagnostics_.end() && it->second.enabled;
    }

    /// @brief Run a named diagnostic. Returns true if it passes. A disabled or
    /// unknown diagnostic returns false. A diagnostic without a check predicate
    /// passes by default.
    bool runDiagnostic(const std::string& name) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!enabled_) return false;
        auto it = diagnostics_.find(name);
        if (it == diagnostics_.end() || !it->second.enabled) return false;
        if (!it->second.check) return true;
        return it->second.check();
    }

    /// @brief Number of registered diagnostics.
    std::size_t getDiagnosticCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return diagnostics_.size();
    }

    /// @brief Get utility statistics/status
    std::string getStatus() const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::size_t on = 0;
        for (const auto& [name, e] : diagnostics_) {
            if (e.enabled) ++on;
        }
        return "DiagnosticSuite[registered=" + std::to_string(diagnostics_.size()) +
               ", enabled=" + std::to_string(on) + "]";
    }

    /// @brief Reset utility state
    void reset() {
        std::lock_guard<std::mutex> lock(mutex_);
        diagnostics_.clear();
    }

private:
    struct Entry {
        bool enabled = true;
        Check check;
    };

    DiagnosticSuite() = default;
    ~DiagnosticSuite() = default;

    DiagnosticSuite(const DiagnosticSuite&) = delete;
    DiagnosticSuite& operator=(const DiagnosticSuite&) = delete;
    DiagnosticSuite(DiagnosticSuite&&) = delete;
    DiagnosticSuite& operator=(DiagnosticSuite&&) = delete;

    mutable std::mutex mutex_;
    bool enabled_ = false;
    std::string config_;
    std::map<std::string, Entry> diagnostics_;
};

} // namespace nexus::utility::meta
