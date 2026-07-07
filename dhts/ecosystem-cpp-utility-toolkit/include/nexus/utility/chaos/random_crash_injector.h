#pragma once

#include <string>
#include <atomic>
#include <random>
#include <thread>
#include <chrono>
#include <cstdlib>
#include <cstdint>

namespace nexus::utility::chaos {

class RandomCrashInjector {
public:
    static RandomCrashInjector& instance() {
        static RandomCrashInjector inst;
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
        return "crash prob: " + std::to_string(crash_probability_.load())
             + "%, delay: " + std::to_string(crash_delay_sec_.load()) + "s";
    }

    void reset() {
        crash_probability_ = 0.0;
        crash_delay_sec_ = 0;
    }

    void setCrashProbability(double percent) {
        crash_probability_ = percent;
    }

    void setCrashDelay(int64_t seconds) {
        crash_delay_sec_ = seconds;
    }

    void maybeCrash() {
        if (!enabled_) return;
        double prob = crash_probability_.load();
        if (prob <= 0.0) return;
        thread_local std::mt19937 rng(std::random_device{}());
        thread_local std::uniform_real_distribution<double> dist(0.0, 100.0);
        if (dist(rng) < prob) {
            int64_t delay = crash_delay_sec_.load();
            if (delay > 0) {
                std::this_thread::sleep_for(std::chrono::seconds(delay));
            }
            std::abort();
        }
    }

private:
    RandomCrashInjector() = default;
    ~RandomCrashInjector() = default;

    RandomCrashInjector(const RandomCrashInjector&) = delete;
    RandomCrashInjector& operator=(const RandomCrashInjector&) = delete;
    RandomCrashInjector(RandomCrashInjector&&) = delete;
    RandomCrashInjector& operator=(RandomCrashInjector&&) = delete;

    bool enabled_ = false;
    std::string config_;
    std::atomic<double> crash_probability_{0.0};
    std::atomic<int64_t> crash_delay_sec_{0};
};

} // namespace nexus::utility::chaos
