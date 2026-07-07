#pragma once

#include <string>
#include <map>
#include <mutex>

namespace nexus::utility::config {

class FeatureFlagDebugger {
public:
    static FeatureFlagDebugger& instance() {
        static FeatureFlagDebugger inst;
        return inst;
    }

    void initialize() { std::lock_guard<std::mutex> lk(mutex_); enabled_ = true; }
    void shutdown() { std::lock_guard<std::mutex> lk(mutex_); enabled_ = false; }
    bool isEnabled() const { return enabled_; }

    void registerFlag(const std::string& name, bool default_value) {
        std::lock_guard<std::mutex> lk(mutex_);
        if (flags_.find(name) == flags_.end()) {
            flags_[name] = default_value;
        }
    }

    void setFlag(const std::string& name, bool value) {
        std::lock_guard<std::mutex> lk(mutex_);
        flags_[name] = value;
    }

    bool isEnabled(const std::string& name) const {
        std::lock_guard<std::mutex> lk(mutex_);
        auto it = flags_.find(name);
        return (it != flags_.end()) ? it->second : false;
    }

    size_t getFlagCount() const {
        std::lock_guard<std::mutex> lk(mutex_);
        return flags_.size();
    }

private:
    FeatureFlagDebugger() = default;
    ~FeatureFlagDebugger() = default;
    mutable std::mutex mutex_;
    bool enabled_ = false;
    std::map<std::string, bool> flags_;
};

} // namespace nexus::utility::config
