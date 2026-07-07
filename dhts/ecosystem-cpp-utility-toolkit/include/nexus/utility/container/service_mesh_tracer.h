#pragma once

#include <string>
#include <vector>
#include <map>
#include <mutex>

namespace nexus::utility::container {

class ServiceMeshTracer {
public:
    struct Hop {
        std::string from;
        std::string service;
        double latency_ms;
    };

    static ServiceMeshTracer& instance() {
        static ServiceMeshTracer inst;
        return inst;
    }

    void initialize(const std::string& config = "") { std::lock_guard<std::mutex> lk(mutex_); enabled_ = true; config_ = config; }
    void shutdown() { std::lock_guard<std::mutex> lk(mutex_); enabled_ = false; }
    bool isEnabled() const { return enabled_; }
    void enable() { std::lock_guard<std::mutex> lk(mutex_); enabled_ = true; }
    void disable() { std::lock_guard<std::mutex> lk(mutex_); enabled_ = false; }
    void reset() { std::lock_guard<std::mutex> lk(mutex_); traces_.clear(); }

    void recordHop(const std::string& service, const std::string& from, double latency_ms) {
        std::lock_guard<std::mutex> lk(mutex_);
        if (!enabled_) return;
        traces_[service].push_back({from, service, latency_ms});
    }

    std::vector<Hop> getTrace(const std::string& service) const {
        std::lock_guard<std::mutex> lk(mutex_);
        auto it = traces_.find(service);
        return (it != traces_.end()) ? it->second : std::vector<Hop>{};
    }

    size_t getHopCount() const {
        std::lock_guard<std::mutex> lk(mutex_);
        size_t total = 0;
        for (const auto& [svc, hops] : traces_) total += hops.size();
        return total;
    }

    double getTotalLatency(const std::string& service) const {
        std::lock_guard<std::mutex> lk(mutex_);
        auto it = traces_.find(service);
        if (it == traces_.end()) return 0.0;
        double total = 0.0;
        for (const auto& hop : it->second) total += hop.latency_ms;
        return total;
    }

private:
    ServiceMeshTracer() = default;
    ~ServiceMeshTracer() = default;
    ServiceMeshTracer(const ServiceMeshTracer&) = delete;
    ServiceMeshTracer& operator=(const ServiceMeshTracer&) = delete;
    ServiceMeshTracer(ServiceMeshTracer&&) = delete;
    ServiceMeshTracer& operator=(ServiceMeshTracer&&) = delete;

    mutable std::mutex mutex_;
    bool enabled_ = false;
    std::string config_;
    std::map<std::string, std::vector<Hop>> traces_;
};

} // namespace nexus::utility::container
