#pragma once

#include <string>
#include <map>
#include <mutex>

namespace nexus::utility::cloud {

class CloudCostTracker {
public:
    static CloudCostTracker& instance() {
        static CloudCostTracker inst;
        return inst;
    }

    void initialize(const std::string& config = "") { std::lock_guard<std::mutex> lk(mutex_); enabled_ = true; config_ = config; }
    void shutdown() { std::lock_guard<std::mutex> lk(mutex_); enabled_ = false; }
    bool isEnabled() const { return enabled_; }
    void enable() { std::lock_guard<std::mutex> lk(mutex_); enabled_ = true; }
    void disable() { std::lock_guard<std::mutex> lk(mutex_); enabled_ = false; }
    void reset() { std::lock_guard<std::mutex> lk(mutex_); service_costs_.clear(); currency_costs_.clear(); }

    void recordCost(const std::string& service, double cost, const std::string& currency) {
        std::lock_guard<std::mutex> lk(mutex_);
        if (!enabled_) return;
        service_costs_[service] += cost;
        currency_costs_[currency] += cost;
    }

    double getTotalCost() const {
        std::lock_guard<std::mutex> lk(mutex_);
        double total = 0.0;
        for (const auto& [svc, c] : service_costs_) total += c;
        return total;
    }

    double getCostByService(const std::string& service) const {
        std::lock_guard<std::mutex> lk(mutex_);
        auto it = service_costs_.find(service);
        return (it != service_costs_.end()) ? it->second : 0.0;
    }

    double getCostByCurrency(const std::string& currency) const {
        std::lock_guard<std::mutex> lk(mutex_);
        auto it = currency_costs_.find(currency);
        return (it != currency_costs_.end()) ? it->second : 0.0;
    }

private:
    CloudCostTracker() = default;
    ~CloudCostTracker() = default;
    CloudCostTracker(const CloudCostTracker&) = delete;
    CloudCostTracker& operator=(const CloudCostTracker&) = delete;
    CloudCostTracker(CloudCostTracker&&) = delete;
    CloudCostTracker& operator=(CloudCostTracker&&) = delete;

    mutable std::mutex mutex_;
    bool enabled_ = false;
    std::string config_;
    std::map<std::string, double> service_costs_;
    std::map<std::string, double> currency_costs_;
};

} // namespace nexus::utility::cloud
