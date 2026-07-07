#pragma once

#include <chrono>
#include <atomic>
#include <mutex>
#include <cstdint>
#include <thread>

namespace nexus::utility::scheduling {

/// Token bucket rate limiter for controlling request rates
class RateLimiter {
public:
    /// Create a rate limiter with given rate and burst capacity
    /// @param rate_tokens_per_sec Maximum sustained rate
    /// @param burst_size Maximum burst size (tokens)
    RateLimiter(double rate_tokens_per_sec, size_t burst_size)
        : rate_(rate_tokens_per_sec), burst_(burst_size),
          tokens_(static_cast<double>(burst_size)) {}

    /// Try to consume a token. Returns true if allowed.
    bool tryConsume(size_t count = 1) {
        std::lock_guard lock(mutex_);
        refill();
        if (tokens_ >= static_cast<double>(count)) {
            tokens_ -= static_cast<double>(count);
            return true;
        }
        return false;
    }

    /// Wait until tokens are available (blocking)
    bool waitConsume(size_t count = 1) {
        while (true) {
            {
                std::lock_guard lock(mutex_);
                refill();
                if (tokens_ >= static_cast<double>(count)) {
                    tokens_ -= static_cast<double>(count);
                    return true;
                }
            }
            // Sleep a bit and retry
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

    /// Get current number of available tokens
    double availableTokens() const {
        std::lock_guard lock(mutex_);
        refill();
        return tokens_;
    }

    /// Get configured rate
    double rate() const { return rate_; }

    /// Set a new rate
    void setRate(double tokens_per_sec) {
        std::lock_guard lock(mutex_);
        rate_ = tokens_per_sec;
    }

    /// Get burst capacity
    size_t burst() const { return burst_; }

private:
    double rate_;
    size_t burst_;
    mutable double tokens_;
    mutable std::mutex mutex_;
    mutable std::chrono::steady_clock::time_point last_refill_{
        std::chrono::steady_clock::now()};

    void refill() const {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration<double>(now - last_refill_).count();
        tokens_ = std::min(static_cast<double>(burst_), tokens_ + elapsed * rate_);
        last_refill_ = now;
    }
};

} // namespace nexus::utility::scheduling
