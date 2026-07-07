#pragma once

#include <mutex>
#include <chrono>
#include <cstdint>

namespace nexus::utility::embedded {

class WatchdogWrapper {
public:
    static WatchdogWrapper& instance() {
        static WatchdogWrapper inst;
        return inst;
    }

    void initialize() { std::lock_guard<std::mutex> lk(mutex_); enabled_ = true; }
    void shutdown() { std::lock_guard<std::mutex> lk(mutex_); enabled_ = false; }
    bool isEnabled() const { return enabled_; }

    void configure(uint32_t timeout_ms) {
        std::lock_guard<std::mutex> lk(mutex_);
        timeout_ms_ = timeout_ms;
        last_kick_ = std::chrono::steady_clock::now();
    }

    void kick() {
        std::lock_guard<std::mutex> lk(mutex_);
        last_kick_ = std::chrono::steady_clock::now();
    }

    bool isExpired() const {
        std::lock_guard<std::mutex> lk(mutex_);
        auto elapsed = std::chrono::steady_clock::now() - last_kick_;
        return std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count() >= timeout_ms_;
    }

    uint64_t getTimeSinceKick() const {
        std::lock_guard<std::mutex> lk(mutex_);
        auto elapsed = std::chrono::steady_clock::now() - last_kick_;
        return std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
    }

private:
    WatchdogWrapper() = default;
    ~WatchdogWrapper() = default;
    mutable std::mutex mutex_;
    bool enabled_ = false;
    uint32_t timeout_ms_ = 0;
    std::chrono::steady_clock::time_point last_kick_{};
};

} // namespace nexus::utility::embedded
