#pragma once

#include <string>
#include <atomic>
#include <chrono>
#include <cstdint>

namespace nexus::utility::chaos {

class ClockSkewInjector {
public:
    static ClockSkewInjector& instance() {
        static ClockSkewInjector inst;
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
        return "skew: " + std::to_string(skew_ms_.load()) + " ms";
    }

    void reset() {
        skew_ms_ = 0;
    }

    void setSkew(int64_t milliseconds) {
        skew_ms_ = milliseconds;
    }

    int64_t getSkew() const {
        return skew_ms_.load();
    }

    std::chrono::system_clock::time_point getSkewedNow() const {
        return std::chrono::system_clock::now() + std::chrono::milliseconds(skew_ms_.load());
    }

private:
    ClockSkewInjector() = default;
    ~ClockSkewInjector() = default;

    ClockSkewInjector(const ClockSkewInjector&) = delete;
    ClockSkewInjector& operator=(const ClockSkewInjector&) = delete;
    ClockSkewInjector(ClockSkewInjector&&) = delete;
    ClockSkewInjector& operator=(ClockSkewInjector&&) = delete;

    bool enabled_ = false;
    std::string config_;
    std::atomic<int64_t> skew_ms_{0};
};

} // namespace nexus::utility::chaos
