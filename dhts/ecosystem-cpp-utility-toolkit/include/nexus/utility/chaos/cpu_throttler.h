#pragma once

#include <string>
#include <thread>
#include <atomic>
#include <chrono>

namespace nexus::utility::chaos {

class CpuThrottler {
public:
    static CpuThrottler& instance() {
        static CpuThrottler inst;
        return inst;
    }

    void initialize(const std::string& config = "") {
        config_ = config;
        enabled_ = true;
    }

    void shutdown() {
        stopThrottling();
        enabled_ = false;
    }

    bool isEnabled() const { return enabled_; }
    void enable() { enabled_ = true; }
    void disable() { enabled_ = false; }

    std::string getStatus() const {
        if (running_) {
            return "throttling at " + std::to_string(utilization_.load()) + "%";
        }
        return "idle";
    }

    void reset() {
        stopThrottling();
        utilization_ = 0.0;
    }

    void startThrottling(double utilization_percent) {
        stopThrottling();
        if (utilization_percent <= 0.0) return;
        if (utilization_percent > 100.0) utilization_percent = 100.0;
        utilization_ = utilization_percent;
        running_ = true;
        worker_ = std::thread([this]() {
            while (running_) {
                double util = utilization_.load();
                auto work_us = std::chrono::microseconds(static_cast<long long>(util * 100));
                auto sleep_us = std::chrono::microseconds(static_cast<long long>((100.0 - util) * 100));
                auto start = std::chrono::high_resolution_clock::now();
                while (std::chrono::high_resolution_clock::now() - start < work_us) {}
                if (sleep_us.count() > 0) {
                    std::this_thread::sleep_for(sleep_us);
                }
            }
        });
    }

    void stopThrottling() {
        running_ = false;
        if (worker_.joinable()) {
            worker_.join();
        }
    }

    double getUtilization() const { return utilization_.load(); }

private:
    CpuThrottler() = default;
    ~CpuThrottler() { shutdown(); }

    CpuThrottler(const CpuThrottler&) = delete;
    CpuThrottler& operator=(const CpuThrottler&) = delete;
    CpuThrottler(CpuThrottler&&) = delete;
    CpuThrottler& operator=(CpuThrottler&&) = delete;

    bool enabled_ = false;
    std::string config_;
    std::atomic<bool> running_{false};
    std::atomic<double> utilization_{0.0};
    std::thread worker_;
};

} // namespace nexus::utility::chaos
