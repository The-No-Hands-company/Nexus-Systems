#pragma once

#include <functional>
#include <chrono>
#include <mutex>
#include <cstdint>

namespace nexus::utility::container {

class KubernetesProbeHelper {
public:
    static KubernetesProbeHelper& instance() {
        static KubernetesProbeHelper inst;
        return inst;
    }

    void initialize(const std::string& config = "") { std::lock_guard<std::mutex> lk(mutex_); enabled_ = true; config_ = config; }
    void shutdown() { std::lock_guard<std::mutex> lk(mutex_); enabled_ = false; }
    bool isEnabled() const { return enabled_; }
    void enable() { std::lock_guard<std::mutex> lk(mutex_); enabled_ = true; }
    void disable() { std::lock_guard<std::mutex> lk(mutex_); enabled_ = false; }
    void reset() { std::lock_guard<std::mutex> lk(mutex_); liveness_check_ = nullptr; readiness_check_ = nullptr; }

    void setLivenessCheck(std::function<bool()> fn) {
        std::lock_guard<std::mutex> lk(mutex_);
        liveness_check_ = std::move(fn);
    }

    void setReadinessCheck(std::function<bool()> fn) {
        std::lock_guard<std::mutex> lk(mutex_);
        readiness_check_ = std::move(fn);
    }

    bool checkLiveness() {
        std::lock_guard<std::mutex> lk(mutex_);
        last_probe_time_ = std::chrono::steady_clock::now();
        return liveness_check_ ? liveness_check_() : true;
    }

    bool checkReadiness() {
        std::lock_guard<std::mutex> lk(mutex_);
        last_probe_time_ = std::chrono::steady_clock::now();
        return readiness_check_ ? readiness_check_() : true;
    }

    uint64_t getLastProbeTime() const {
        std::lock_guard<std::mutex> lk(mutex_);
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            last_probe_time_.time_since_epoch()).count();
    }

private:
    KubernetesProbeHelper() = default;
    ~KubernetesProbeHelper() = default;
    KubernetesProbeHelper(const KubernetesProbeHelper&) = delete;
    KubernetesProbeHelper& operator=(const KubernetesProbeHelper&) = delete;
    KubernetesProbeHelper(KubernetesProbeHelper&&) = delete;
    KubernetesProbeHelper& operator=(KubernetesProbeHelper&&) = delete;

    mutable std::mutex mutex_;
    bool enabled_ = false;
    std::string config_;
    std::function<bool()> liveness_check_;
    std::function<bool()> readiness_check_;
    std::chrono::steady_clock::time_point last_probe_time_{};
};

} // namespace nexus::utility::container
