#pragma once

#include <string>
#include <unordered_map>
#include <functional>
#include <mutex>

namespace nexus::utility::config {

class ConfigChangeNotifier {
public:
    using ChangeCallback = std::function<void(std::string, std::string)>;

    static ConfigChangeNotifier& instance() {
        static ConfigChangeNotifier inst;
        return inst;
    }

    void initialize() { std::lock_guard<std::mutex> lk(mutex_); enabled_ = true; }
    void shutdown() { std::lock_guard<std::mutex> lk(mutex_); enabled_ = false; }
    bool isEnabled() const { return enabled_; }

    void watch(const std::string& key, ChangeCallback callback) {
        std::lock_guard<std::mutex> lk(mutex_);
        watchers_[key] = std::move(callback);
    }

    void setValue(const std::string& key, const std::string& value) {
        std::lock_guard<std::mutex> lk(mutex_);
        if (!enabled_) return;
        std::string old = values_[key];
        values_[key] = value;
        auto it = watchers_.find(key);
        if (it != watchers_.end() && it->second) {
            it->second(key, value);
        }
    }

    std::string getValue(const std::string& key) const {
        std::lock_guard<std::mutex> lk(mutex_);
        auto it = values_.find(key);
        return (it != values_.end()) ? it->second : "";
    }

private:
    ConfigChangeNotifier() = default;
    ~ConfigChangeNotifier() = default;
    mutable std::mutex mutex_;
    bool enabled_ = false;
    std::unordered_map<std::string, std::string> values_;
    std::unordered_map<std::string, ChangeCallback> watchers_;
};

} // namespace nexus::utility::config
