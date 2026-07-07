#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <chrono>
#include <cstdint>
#include <mutex>
namespace nexus::utility::network {
class ConnectionPoolMonitor {
public:
    struct PoolStats { std::string poolName; size_t totalConnections; size_t activeConnections;
        size_t idleConnections; size_t maxConnections; size_t waitCount;
        std::chrono::microseconds avgWaitTime{0}; };
    static ConnectionPoolMonitor& instance(); void initialize(); void shutdown(); bool isEnabled() const;
    void registerPool(const std::string& name, size_t maxConns);
    void recordAcquire(const std::string& name);
    void recordRelease(const std::string& name);
    void recordWait(const std::string& name, std::chrono::microseconds wait);
    PoolStats getStats(const std::string& name) const;
    std::vector<PoolStats> getAllStats() const;
    bool isExhausted(const std::string& name) const;
    void clear();
private:
    ConnectionPoolMonitor() = default; ~ConnectionPoolMonitor() = default; bool enabled_ = false;
    struct Internal { size_t maxConn; size_t active; size_t waitCount; size_t totalWaits; std::chrono::microseconds totalWait{0}; };
    std::unordered_map<std::string, Internal> pools_;
    mutable std::mutex mutex_;
};
} // namespace nexus::utility::network