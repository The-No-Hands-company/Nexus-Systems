#pragma once

#include <string>
#include <map>
#include <vector>
#include <mutex>
#include <cstddef>
#include <cstdlib>
#include <cstring>

extern "C" char** environ;

namespace nexus::utility::meta {

/**
 * @brief Centralized debug configuration store.
 *
 * Holds string key/value configuration entries that can be set programmatically
 * or loaded from environment variables sharing a common prefix.
 *
 * Thread safety: all public methods are protected by a mutex.
 *
 * @category Meta
 * @version 1.0.0
 */
class DebugConfig {
public:
    /// @brief Get singleton instance
    static DebugConfig& instance() {
        static DebugConfig inst;
        return inst;
    }

    /// @brief Initialize the utility
    void initialize(const std::string& config = "") {
        std::lock_guard<std::mutex> lock(mutex_);
        init_config_ = config;
        enabled_ = true;
        config_.clear();
    }

    /// @brief Shutdown the utility and cleanup resources
    void shutdown() {
        std::lock_guard<std::mutex> lock(mutex_);
        enabled_ = false;
        config_.clear();
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

    /// @brief Set a configuration key to a value.
    void setConfig(const std::string& key, const std::string& value) {
        std::lock_guard<std::mutex> lock(mutex_);
        config_[key] = value;
    }

    /// @brief Get the value for a key, or an empty string if not present.
    std::string getConfig(const std::string& key) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = config_.find(key);
        return it == config_.end() ? std::string{} : it->second;
    }

    /// @brief Check whether a config key exists.
    bool hasConfig(const std::string& key) const {
        std::lock_guard<std::mutex> lock(mutex_);
        return config_.find(key) != config_.end();
    }

    /// @brief Load configuration from environment variables with the given
    /// prefix. The prefix is stripped from the resulting keys. For example,
    /// loadFromEnv("NEXUS_") turns NEXUS_LEVEL=debug into config["LEVEL"]=debug.
    /// @return Number of variables loaded.
    std::size_t loadFromEnv(const std::string& prefix) {
        std::lock_guard<std::mutex> lock(mutex_);
        std::size_t loaded = 0;
        if (environ == nullptr) return 0;
        for (char** env = environ; *env != nullptr; ++env) {
            std::string entry(*env);
            if (entry.compare(0, prefix.size(), prefix) != 0) continue;
            auto eq = entry.find('=');
            if (eq == std::string::npos) continue;
            std::string key = entry.substr(prefix.size(), eq - prefix.size());
            std::string value = entry.substr(eq + 1);
            config_[key] = value;
            ++loaded;
        }
        return loaded;
    }

    /// @brief Number of configuration entries.
    std::size_t getConfigCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return config_.size();
    }

    /// @brief Get all configuration key/value pairs.
    std::map<std::string, std::string> getAll() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return config_;
    }

    /// @brief Get utility statistics/status
    std::string getStatus() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return "DebugConfig[entries=" + std::to_string(config_.size()) +
               ", enabled=" + std::string(enabled_ ? "true" : "false") + "]";
    }

    /// @brief Reset utility state
    void reset() {
        std::lock_guard<std::mutex> lock(mutex_);
        config_.clear();
    }

private:
    DebugConfig() = default;
    ~DebugConfig() = default;

    DebugConfig(const DebugConfig&) = delete;
    DebugConfig& operator=(const DebugConfig&) = delete;
    DebugConfig(DebugConfig&&) = delete;
    DebugConfig& operator=(DebugConfig&&) = delete;

    mutable std::mutex mutex_;
    bool enabled_ = false;
    std::string init_config_;
    std::map<std::string, std::string> config_;
};

} // namespace nexus::utility::meta
