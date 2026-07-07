#pragma once

#include <string>
#include <map>
#include <mutex>
#include <cstddef>

namespace nexus::utility::meta {

/**
 * @brief Runtime feature flag registry.
 *
 * Flags are registered with a default boolean value and can be overridden at
 * runtime. isEnabled() queries the current value of a flag.
 *
 * Thread safety: all public methods are protected by a mutex.
 *
 * @category Meta
 * @version 1.0.0
 */
class FeatureFlags {
public:
    /// @brief Get singleton instance
    static FeatureFlags& instance() {
        static FeatureFlags inst;
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
        flags_.clear();
    }

    /// @brief Check if the flag subsystem is enabled
    bool isEnabled() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return enabled_;
    }

    /// @brief Enable the flag subsystem
    void enable() {
        std::lock_guard<std::mutex> lock(mutex_);
        enabled_ = true;
    }

    /// @brief Disable the flag subsystem
    void disable() {
        std::lock_guard<std::mutex> lock(mutex_);
        enabled_ = false;
    }

    // ── Domain methods ──────────────────────────────────────────────────────

    /// @brief Register a flag with a default value. Existing flags are not
    /// overwritten.
    void registerFlag(const std::string& name, bool default_value) {
        std::lock_guard<std::mutex> lock(mutex_);
        flags_.emplace(name, default_value);
    }

    /// @brief Set (or create) a flag's value.
    void setFlag(const std::string& name, bool value) {
        std::lock_guard<std::mutex> lock(mutex_);
        flags_[name] = value;
    }

    /// @brief Query whether a named flag is enabled. Unknown flags are false.
    bool isEnabled(const std::string& name) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = flags_.find(name);
        return it != flags_.end() && it->second;
    }

    /// @brief Whether a flag has been registered.
    bool hasFlag(const std::string& name) const {
        std::lock_guard<std::mutex> lock(mutex_);
        return flags_.find(name) != flags_.end();
    }

    /// @brief Number of registered flags.
    std::size_t getFlagCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return flags_.size();
    }

    /// @brief Get all flags and their current values.
    std::map<std::string, bool> getAll() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return flags_;
    }

    /// @brief Get utility statistics/status
    std::string getStatus() const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::size_t on = 0;
        for (const auto& [name, v] : flags_) {
            if (v) ++on;
        }
        return "FeatureFlags[registered=" + std::to_string(flags_.size()) +
               ", enabled=" + std::to_string(on) + "]";
    }

    /// @brief Reset utility state
    void reset() {
        std::lock_guard<std::mutex> lock(mutex_);
        flags_.clear();
    }

private:
    FeatureFlags() = default;
    ~FeatureFlags() = default;

    FeatureFlags(const FeatureFlags&) = delete;
    FeatureFlags& operator=(const FeatureFlags&) = delete;
    FeatureFlags(FeatureFlags&&) = delete;
    FeatureFlags& operator=(FeatureFlags&&) = delete;

    mutable std::mutex mutex_;
    bool enabled_ = false;
    std::string config_;
    std::map<std::string, bool> flags_;
};

} // namespace nexus::utility::meta
