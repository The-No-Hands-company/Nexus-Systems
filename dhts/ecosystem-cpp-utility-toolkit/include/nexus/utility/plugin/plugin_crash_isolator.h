#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <atomic>
#include <mutex>

namespace nexus::utility::plugin {

/**
 * @brief Track per-plugin crash counts and isolate faulty plugins.
 */
class PluginCrashIsolator {
public:
    struct PluginHealth {
        std::string name;
        std::size_t crashes = 0;
        std::size_t invocations = 0;
        bool isolated = false;
        std::string lastError;
    };

    static PluginCrashIsolator& instance() {
        static PluginCrashIsolator inst;
        return inst;
    }

    void initialize(const std::string& config = "") {
        std::lock_guard<std::mutex> lk(mutex_);
        config_ = config;
        enabled_ = true;
    }
    void shutdown() { enabled_ = false; }
    bool isEnabled() const { return enabled_.load(); }

    void setCrashThreshold(std::size_t threshold) {
        std::lock_guard<std::mutex> lk(mutex_);
        threshold_ = threshold;
    }

    void recordInvocation(const std::string& plugin) {
        std::lock_guard<std::mutex> lk(mutex_);
        auto& h = plugins_[plugin];
        h.name = plugin;
        ++h.invocations;
    }

    /// Record a crash; isolates the plugin if it exceeds the threshold.
    bool recordCrash(const std::string& plugin, const std::string& error = "") {
        std::lock_guard<std::mutex> lk(mutex_);
        auto& h = plugins_[plugin];
        h.name = plugin;
        ++h.crashes;
        if (!error.empty()) h.lastError = error;
        if (h.crashes >= threshold_) h.isolated = true;
        return h.isolated;
    }

    bool isIsolated(const std::string& plugin) const {
        std::lock_guard<std::mutex> lk(mutex_);
        auto it = plugins_.find(plugin);
        return it != plugins_.end() && it->second.isolated;
    }

    void rehabilitate(const std::string& plugin) {
        std::lock_guard<std::mutex> lk(mutex_);
        auto it = plugins_.find(plugin);
        if (it != plugins_.end()) { it->second.isolated = false; it->second.crashes = 0; }
    }

    double crashRate(const std::string& plugin) const {
        std::lock_guard<std::mutex> lk(mutex_);
        auto it = plugins_.find(plugin);
        if (it == plugins_.end() || it->second.invocations == 0) return 0.0;
        return static_cast<double>(it->second.crashes) / it->second.invocations;
    }

    std::vector<PluginHealth> isolatedPlugins() const {
        std::lock_guard<std::mutex> lk(mutex_);
        std::vector<PluginHealth> out;
        for (const auto& [n, h] : plugins_) if (h.isolated) out.push_back(h);
        return out;
    }

    void reset() {
        std::lock_guard<std::mutex> lk(mutex_);
        plugins_.clear();
    }

private:
    PluginCrashIsolator() = default;
    ~PluginCrashIsolator() = default;
    PluginCrashIsolator(const PluginCrashIsolator&) = delete;
    PluginCrashIsolator& operator=(const PluginCrashIsolator&) = delete;

    mutable std::mutex mutex_;
    std::atomic<bool> enabled_{false};
    std::string config_;
    std::size_t threshold_ = 3;
    std::unordered_map<std::string, PluginHealth> plugins_;
};

} // namespace nexus::utility::plugin
