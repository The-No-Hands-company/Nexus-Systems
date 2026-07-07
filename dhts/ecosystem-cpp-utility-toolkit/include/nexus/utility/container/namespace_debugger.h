#pragma once

#include <string>
#include <map>
#include <set>
#include <mutex>

namespace nexus::utility::container {

class NamespaceDebugger {
public:
    static NamespaceDebugger& instance() {
        static NamespaceDebugger inst;
        return inst;
    }

    void initialize(const std::string& config = "") { std::lock_guard<std::mutex> lk(mutex_); enabled_ = true; config_ = config; }
    void shutdown() { std::lock_guard<std::mutex> lk(mutex_); enabled_ = false; }
    bool isEnabled() const { return enabled_; }
    void enable() { std::lock_guard<std::mutex> lk(mutex_); enabled_ = true; }
    void disable() { std::lock_guard<std::mutex> lk(mutex_); enabled_ = false; }
    void reset() { std::lock_guard<std::mutex> lk(mutex_); namespaces_.clear(); }

    void registerNamespace(const std::string& type, const std::string& id) {
        std::lock_guard<std::mutex> lk(mutex_);
        namespaces_[type] = id;
    }

    bool isIsolated(const std::string& type) const {
        std::lock_guard<std::mutex> lk(mutex_);
        return namespaces_.find(type) != namespaces_.end();
    }

    std::map<std::string, std::string> getActiveNamespaces() const {
        std::lock_guard<std::mutex> lk(mutex_);
        return namespaces_;
    }

private:
    NamespaceDebugger() = default;
    ~NamespaceDebugger() = default;
    NamespaceDebugger(const NamespaceDebugger&) = delete;
    NamespaceDebugger& operator=(const NamespaceDebugger&) = delete;
    NamespaceDebugger(NamespaceDebugger&&) = delete;
    NamespaceDebugger& operator=(NamespaceDebugger&&) = delete;

    mutable std::mutex mutex_;
    bool enabled_ = false;
    std::string config_;
    std::map<std::string, std::string> namespaces_;
};

} // namespace nexus::utility::container
