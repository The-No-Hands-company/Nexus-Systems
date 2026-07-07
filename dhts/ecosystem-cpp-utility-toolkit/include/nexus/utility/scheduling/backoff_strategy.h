#pragma once

#include <chrono>
#include <random>
#include <algorithm>
#include <thread>
#include <cstdint>
#include <mutex>

namespace nexus::utility::scheduling {

/// Exponential backoff with jitter for retry strategies
class BackoffStrategy {
public:
    struct Config {
        std::chrono::milliseconds initial_delay{100};
        std::chrono::milliseconds max_delay{30000};
        double multiplier = 2.0;
        bool use_jitter = true;
        double jitter_factor = 0.1;  // 10% jitter
    };

    BackoffStrategy() : config_{} {}
    explicit BackoffStrategy(Config config) : config_(config) {}

    /// Get the next delay and increment attempt counter. Thread-safe.
    [[nodiscard]] std::chrono::milliseconds nextDelay() {
        std::lock_guard lock(mutex_);
        auto delay = config_.initial_delay * std::pow(config_.multiplier, attempt_);
        auto delay_ms = std::chrono::duration_cast<std::chrono::milliseconds>(delay);
        if (delay_ms > config_.max_delay) delay_ms = config_.max_delay;

        if (config_.use_jitter) {
            auto jitter_range = static_cast<int64_t>(delay_ms.count() * config_.jitter_factor);
            if (jitter_range > 0) {
                std::uniform_int_distribution<int64_t> dist(-jitter_range, jitter_range);
                delay_ms += std::chrono::milliseconds(dist(rng_));
            }
        }

        ++attempt_;
        return std::chrono::milliseconds(std::max<int64_t>(1, delay_ms.count()));
    }

    [[nodiscard]] int attempts() const noexcept { return attempt_; }

    /// Sleep for the backoff duration
    void wait() {
        std::this_thread::sleep_for(nextDelay());
    }

    /// Reset the attempt counter
    void reset() noexcept { attempt_ = 0; }

private:
    Config config_;
    int attempt_ = 0;
    std::mt19937_64 rng_{std::random_device{}()};
    mutable std::mutex mutex_;
};

} // namespace nexus::utility::scheduling
