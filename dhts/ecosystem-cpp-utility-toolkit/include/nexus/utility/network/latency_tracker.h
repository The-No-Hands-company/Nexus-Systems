#pragma once

#include <chrono>
#include <string>
#include <unordered_map>
#include <mutex>
#include <ostream>
#include <vector>

namespace nexus::utility::network {

/// @brief Network error classification
enum class NetworkErrorCategory {
    Timeout,
    ConnectionRefused,
    ConnectionReset,
    HostUnreachable,
    DNSFailure,
    SSLError,
    Unknown
};

/// @brief Classify network errors
class NetworkErrorClassifier {
public:
    static NetworkErrorCategory classify(int error_code);
    static std::string categoryToString(NetworkErrorCategory category);
};

/// @brief Track network latency statistics
class LatencyTracker {
public:
    void recordLatency(const std::string& endpoint, std::chrono::microseconds latency);

    struct LatencyStats {
        size_t count = 0;
        std::chrono::microseconds min{};
        std::chrono::microseconds max{};
        std::chrono::microseconds avg{};
        std::chrono::microseconds p50{};
        std::chrono::microseconds p95{};
        std::chrono::microseconds p99{};
    };

    LatencyStats getStats(const std::string& endpoint) const;
    void printReport(std::ostream& os) const;

private:
    mutable std::mutex mutex_;
    std::unordered_map<std::string, std::vector<std::chrono::microseconds>> latencies_;
};

/// @brief Monitor circuit breaker states
class CircuitBreakerMonitor {
public:
    enum class State {
        Closed,    // Normal operation
        Open,      // Failing, reject requests
        HalfOpen   // Testing if service recovered
    };

    void recordSuccess(const std::string& service);
    void recordFailure(const std::string& service);
    State getState(const std::string& service) const;
    void printReport(std::ostream& os) const;

private:
    struct ServiceState {
        State state = State::Closed;
        size_t failure_count = 0;
        size_t success_count = 0;
        std::chrono::steady_clock::time_point last_failure;
    };

    mutable std::mutex mutex_;
    std::unordered_map<std::string, ServiceState> services_;
};

} // namespace nexus::utility::network
