#pragma once

#include <string>
#include <map>
#include <mutex>

namespace nexus::utility::config {

class DependencyInjectorDebug {
public:
    static DependencyInjectorDebug& instance() {
        static DependencyInjectorDebug inst;
        return inst;
    }

    void initialize() { std::lock_guard<std::mutex> lk(mutex_); enabled_ = true; }
    void shutdown() { std::lock_guard<std::mutex> lk(mutex_); enabled_ = false; }
    bool isEnabled() const { return enabled_; }

    void registerBinding(const std::string& interface, const std::string& implementation) {
        std::lock_guard<std::mutex> lk(mutex_);
        bindings_[interface] = implementation;
    }

    std::string resolve(const std::string& interface) const {
        std::lock_guard<std::mutex> lk(mutex_);
        auto it = bindings_.find(interface);
        return (it != bindings_.end()) ? it->second : "";
    }

    std::map<std::string, std::string> getBindings() const {
        std::lock_guard<std::mutex> lk(mutex_);
        return bindings_;
    }

private:
    DependencyInjectorDebug() = default;
    ~DependencyInjectorDebug() = default;
    mutable std::mutex mutex_;
    bool enabled_ = false;
    std::map<std::string, std::string> bindings_;
};

} // namespace nexus::utility::config
