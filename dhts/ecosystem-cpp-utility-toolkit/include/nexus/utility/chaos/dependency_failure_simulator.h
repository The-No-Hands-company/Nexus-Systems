#pragma once

#include <string>
#include <unordered_map>
#include <random>
#include <mutex>

namespace nexus::utility::chaos {

class DependencyFailureSimulator {
public:
    static DependencyFailureSimulator& instance() {
        static DependencyFailureSimulator inst;
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
        return std::to_string(dependencies_.size()) + " dependencies";
    }

    void reset() {
        std::lock_guard lock(mutex_);
        dependencies_.clear();
    }

    void addDependency(const std::string& name, double failure_rate_percent) {
        std::lock_guard lock(mutex_);
        dependencies_[name] = failure_rate_percent;
    }

    void removeDependency(const std::string& name) {
        std::lock_guard lock(mutex_);
        dependencies_.erase(name);
    }

    bool isAvailable(const std::string& name) {
        if (!enabled_) return true;
        std::lock_guard lock(mutex_);
        auto it = dependencies_.find(name);
        if (it == dependencies_.end()) return true;
        std::uniform_real_distribution<double> dist(0.0, 100.0);
        return dist(rng_) >= it->second;
    }

private:
    DependencyFailureSimulator() : rng_(std::random_device{}()) {}
    ~DependencyFailureSimulator() = default;

    DependencyFailureSimulator(const DependencyFailureSimulator&) = delete;
    DependencyFailureSimulator& operator=(const DependencyFailureSimulator&) = delete;
    DependencyFailureSimulator(DependencyFailureSimulator&&) = delete;
    DependencyFailureSimulator& operator=(DependencyFailureSimulator&&) = delete;

    bool enabled_ = false;
    std::string config_;
    mutable std::mutex mutex_;
    std::unordered_map<std::string, double> dependencies_;
    std::mt19937 rng_;
};

} // namespace nexus::utility::chaos
