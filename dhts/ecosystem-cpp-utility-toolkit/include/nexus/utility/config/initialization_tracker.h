#pragma once

#include <string>
#include <vector>
#include <mutex>

namespace nexus::utility::config {

class InitializationTracker {
public:
    static InitializationTracker& instance() {
        static InitializationTracker inst;
        return inst;
    }

    void initialize() { std::lock_guard<std::mutex> lk(mutex_); enabled_ = true; }
    void shutdown() { std::lock_guard<std::mutex> lk(mutex_); enabled_ = false; }
    bool isEnabled() const { return enabled_; }

    void recordInit(const std::string& component, double ms) {
        std::lock_guard<std::mutex> lk(mutex_);
        if (!enabled_) return;
        order_.push_back({component, ms});
        total_time_ += ms;
        if (ms > slowest_time_) {
            slowest_time_ = ms;
            slowest_component_ = component;
        }
    }

    std::vector<std::string> getInitOrder() const {
        std::lock_guard<std::mutex> lk(mutex_);
        std::vector<std::string> names;
        names.reserve(order_.size());
        for (const auto& p : order_) names.push_back(p.first);
        return names;
    }

    double getTotalInitTime() const {
        std::lock_guard<std::mutex> lk(mutex_);
        return total_time_;
    }

    std::string getSlowestInit() const {
        std::lock_guard<std::mutex> lk(mutex_);
        return slowest_component_;
    }

private:
    InitializationTracker() = default;
    ~InitializationTracker() = default;
    mutable std::mutex mutex_;
    bool enabled_ = false;
    std::vector<std::pair<std::string, double>> order_;
    double total_time_ = 0.0;
    double slowest_time_ = 0.0;
    std::string slowest_component_;
};

} // namespace nexus::utility::config
