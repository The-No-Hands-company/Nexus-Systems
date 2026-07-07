#pragma once

#include <chrono>
#include <atomic>
#include <string>
#include <functional>
#include <stdexcept>
#include <type_traits>

namespace nexus::utility::scheduling {

/// Circuit breaker pattern for failing external dependencies
class CircuitBreaker {
public:
    enum class State { Closed, Open, HalfOpen };

    struct Config {
        int failure_threshold = 5;
        std::chrono::milliseconds recovery_timeout{30000};
        std::chrono::milliseconds half_open_max_duration{10000};
        int half_open_success_threshold = 2;
    };

    CircuitBreaker() : config_{} {}
    explicit CircuitBreaker(Config config) : config_(config) {}

    /// Try to execute an action through the circuit breaker
    template<typename Func>
    auto execute(Func&& fn) -> decltype(fn()) {
        if (!allowRequest()) {
            throw CircuitOpenException("Circuit breaker is OPEN");
        }

        try {
            if constexpr (std::is_void_v<decltype(fn())>) {
                fn();
                onSuccess();
            } else {
                auto result = fn();
                onSuccess();
                return result;
            }
        } catch (...) {
            onFailure();
            throw;
        }
    }

    /// Manual state transitions
    void trip() {
        state_.store(State::Open, std::memory_order_release);
        opened_at_.store(std::chrono::steady_clock::now(), std::memory_order_release);
    }

    void reset() {
        state_.store(State::Closed, std::memory_order_release);
        failure_count_.store(0, std::memory_order_release);
        half_open_successes_ = 0;
    }

    State state() const { return state_; }
    int failureCount() const { return failure_count_; }

    /// Get state as string
    std::string stateString() const {
        switch (state_) {
            case State::Closed:   return "CLOSED";
            case State::Open:     return "OPEN";
            case State::HalfOpen: return "HALF_OPEN";
        }
        return "UNKNOWN";
    }

    /// Exception thrown when circuit is open
    class CircuitOpenException : public std::runtime_error {
    public:
        explicit CircuitOpenException(const std::string& msg)
            : std::runtime_error(msg) {}
    };

private:
    Config config_;
    std::atomic<State> state_{State::Closed};
    std::atomic<int> failure_count_{0};
    std::atomic<int> half_open_successes_{0};
    std::atomic<std::chrono::steady_clock::time_point> opened_at_{};

    bool allowRequest() {
        switch (state_.load(std::memory_order_acquire)) {
            case State::Closed:
                return true;

            case State::Open: {
                auto elapsed = std::chrono::steady_clock::now() -
                               opened_at_.load(std::memory_order_acquire);
                if (elapsed >= config_.recovery_timeout) {
                    state_.store(State::HalfOpen, std::memory_order_release);
                    half_open_successes_.store(0, std::memory_order_release);
                    return true;
                }
                return false;
            }

            case State::HalfOpen:
                return true;
        }
        return false;
    }

    void onSuccess() {
        if (state_.load(std::memory_order_acquire) == State::HalfOpen) {
            half_open_successes_.fetch_add(1, std::memory_order_relaxed);
            if (half_open_successes_.load(std::memory_order_acquire) >= config_.half_open_success_threshold) {
                reset();
            }
        }
        failure_count_.store(0, std::memory_order_release);
    }

    void onFailure() {
        failure_count_.fetch_add(1, std::memory_order_relaxed);
        if (failure_count_.load(std::memory_order_acquire) >= config_.failure_threshold) {
            trip();
        }
        if (state_.load(std::memory_order_acquire) == State::HalfOpen) {
            trip();
        }
    }
};

} // namespace nexus::utility::scheduling
