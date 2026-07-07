#pragma once

#include <string>
#include <unordered_map>
#include <mutex>
#include <cstdint>

namespace nexus::utility::chaos {

class NetworkFaultInjector {
public:
    struct Rule {
        bool block = false;
        int64_t delay_ms = 0;
    };

    static NetworkFaultInjector& instance() {
        static NetworkFaultInjector inst;
        return inst;
    }

    void initialize(const std::string& config = "") {
        config_ = config;
        enabled_ = true;
    }

    void shutdown() {
        reset();
        enabled_ = false;
    }

    bool isEnabled() const { return enabled_; }
    void enable() { enabled_ = true; }
    void disable() { enabled_ = false; }

    std::string getStatus() const {
        std::lock_guard lock(mutex_);
        return std::to_string(rules_.size()) + " active rules";
    }

    void reset() {
        std::lock_guard lock(mutex_);
        rules_.clear();
    }

    void addBlockRule(const std::string& host, int port) {
        std::lock_guard lock(mutex_);
        auto key = makeKey(host, port);
        rules_[key] = {true, 0};
    }

    void addDelayRule(const std::string& host, int port, int64_t delay_ms) {
        std::lock_guard lock(mutex_);
        auto key = makeKey(host, port);
        rules_[key] = {false, delay_ms};
    }

    void removeRule(const std::string& host, int port) {
        std::lock_guard lock(mutex_);
        rules_.erase(makeKey(host, port));
    }

    bool shouldBlock(const std::string& host, int port) const {
        std::lock_guard lock(mutex_);
        auto it = rules_.find(makeKey(host, port));
        if (it == rules_.end()) return false;
        return it->second.block;
    }

    int64_t getDelay(const std::string& host, int port) const {
        std::lock_guard lock(mutex_);
        auto it = rules_.find(makeKey(host, port));
        if (it == rules_.end()) return 0;
        return it->second.delay_ms;
    }

private:
    NetworkFaultInjector() = default;
    ~NetworkFaultInjector() = default;

    NetworkFaultInjector(const NetworkFaultInjector&) = delete;
    NetworkFaultInjector& operator=(const NetworkFaultInjector&) = delete;
    NetworkFaultInjector(NetworkFaultInjector&&) = delete;
    NetworkFaultInjector& operator=(NetworkFaultInjector&&) = delete;

    static std::string makeKey(const std::string& host, int port) {
        return host + ":" + std::to_string(port);
    }

    bool enabled_ = false;
    std::string config_;
    mutable std::mutex mutex_;
    std::unordered_map<std::string, Rule> rules_;
};

} // namespace nexus::utility::chaos
