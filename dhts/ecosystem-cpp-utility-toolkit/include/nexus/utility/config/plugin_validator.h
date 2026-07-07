#pragma once

#include <string>
#include <map>
#include <set>
#include <vector>
#include <mutex>

namespace nexus::utility::config {

class PluginValidator {
public:
    static PluginValidator& instance() {
        static PluginValidator inst;
        return inst;
    }

    void initialize() { std::lock_guard<std::mutex> lk(mutex_); enabled_ = true; }
    void shutdown() { std::lock_guard<std::mutex> lk(mutex_); enabled_ = false; }
    bool isEnabled() const { return enabled_; }

    void registerPlugin(const std::string& name, const std::string& version) {
        std::lock_guard<std::mutex> lk(mutex_);
        plugins_[name] = version;
        if (deps_.find(name) == deps_.end()) {
            deps_[name] = {};
        }
    }

    void setDependency(const std::string& plugin, const std::string& depends_on) {
        std::lock_guard<std::mutex> lk(mutex_);
        deps_[plugin].insert(depends_on);
    }

    bool validate() const {
        std::lock_guard<std::mutex> lk(mutex_);
        for (const auto& [plugin, dependencies] : deps_) {
            for (const auto& dep : dependencies) {
                if (plugins_.find(dep) == plugins_.end()) {
                    return false;
                }
            }
        }
        return true;
    }

private:
    PluginValidator() = default;
    ~PluginValidator() = default;
    mutable std::mutex mutex_;
    bool enabled_ = false;
    std::map<std::string, std::string> plugins_;
    std::map<std::string, std::set<std::string>> deps_;
};

} // namespace nexus::utility::config
