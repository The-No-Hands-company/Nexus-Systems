#pragma once

#include <chrono>
#include <cstdint>
#include <map>
#include <mutex>
#include <string>
#include <vector>

namespace nexus::utility::hardware {

class BusAnalyzer {
public:
    struct BusStats {
        uint64_t transaction_count = 0;
        uint64_t error_count = 0;
        uint64_t byte_count = 0;
        std::chrono::steady_clock::time_point start_time = std::chrono::steady_clock::now();
    };

    static BusAnalyzer& instance() {
        static BusAnalyzer inst;
        return inst;
    }

    void initialize(const std::string& = "") { std::lock_guard<std::mutex> lock(mutex_); enabled_ = true; }
    void shutdown() { std::lock_guard<std::mutex> lock(mutex_); enabled_ = false; reset(); }
    bool isEnabled() const { return enabled_; }
    void enable() { std::lock_guard<std::mutex> lock(mutex_); enabled_ = true; }
    void disable() { std::lock_guard<std::mutex> lock(mutex_); enabled_ = false; }

    std::string getStatus() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return "BusAnalyzer: " + std::to_string(buses_.size()) + " buses monitored, enabled=" + (enabled_ ? "true" : "false");
    }
    void reset() { std::lock_guard<std::mutex> lock(mutex_); buses_.clear(); }

    void recordTransaction(const std::string& bus, uint32_t addr, uint32_t data, bool is_read) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto& stats = buses_[bus];
        ++stats.transaction_count;
        stats.byte_count += 4;
    }

    uint64_t getTransactionCount(const std::string& bus) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = buses_.find(bus);
        return (it != buses_.end()) ? it->second.transaction_count : 0;
    }

    double getBusUtilization(const std::string& bus) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = buses_.find(bus);
        if (it == buses_.end()) return 0.0;
        auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now() - it->second.start_time).count();
        if (elapsed == 0) return 0.0;
        return static_cast<double>(it->second.byte_count) * 1000000.0 / static_cast<double>(elapsed);
    }

    uint64_t getErrorCount(const std::string& bus) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = buses_.find(bus);
        return (it != buses_.end()) ? it->second.error_count : 0;
    }

private:
    BusAnalyzer() = default;
    ~BusAnalyzer() = default;
    BusAnalyzer(const BusAnalyzer&) = delete;
    BusAnalyzer& operator=(const BusAnalyzer&) = delete;
    BusAnalyzer(BusAnalyzer&&) = delete;
    BusAnalyzer& operator=(BusAnalyzer&&) = delete;

    mutable std::mutex mutex_;
    bool enabled_ = false;
    std::map<std::string, BusStats> buses_;
};

} // namespace nexus::utility::hardware
