#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <chrono>
namespace nexus::utility::network {
class CircuitBreakerMonitor {
public:
    enum class State { Closed, Open, HalfOpen };
    struct BreakerStats { std::string name; State state = State::Closed; size_t failureCount = 0; size_t successCount = 0;
        size_t openCount = 0; std::chrono::system_clock::time_point lastStateChange;
        std::chrono::milliseconds openDuration{0}; };
    static CircuitBreakerMonitor& instance(); void initialize(); void shutdown(); bool isEnabled() const;
    void registerBreaker(const std::string& name);
    void recordSuccess(const std::string& name);
    void recordFailure(const std::string& name);
    void recordOpen(const std::string& name);
    void recordHalfOpen(const std::string& name);
    void recordClose(const std::string& name);
    BreakerStats getStats(const std::string& name) const;
    std::vector<BreakerStats> getAllStats() const;
    bool isOpen(const std::string& name) const;
    void clear();
private:
    CircuitBreakerMonitor() = default; ~CircuitBreakerMonitor() = default; bool enabled_ = false;
    std::unordered_map<std::string, BreakerStats> breakers_;
};
} // namespace nexus::utility::network