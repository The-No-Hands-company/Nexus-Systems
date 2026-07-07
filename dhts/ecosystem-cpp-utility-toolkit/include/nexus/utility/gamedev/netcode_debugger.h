#pragma once

#include <string>
#include <vector>
#include <functional>
#include <source_location>
#include <map>
#include <mutex>

namespace nexus::utility::gamedev {

struct PacketStats {
    size_t packets_sent = 0;
    size_t packets_received = 0;
    size_t bytes_sent = 0;
    size_t bytes_received = 0;
};

class NetcodeDebugger {
public:
    static NetcodeDebugger& instance() {
        static NetcodeDebugger inst;
        return inst;
    }

    void initialize(const std::string& config = "") { config_ = config; enabled_ = true; }
    void shutdown() { enabled_ = false; }
    bool isEnabled() const { return enabled_; }
    void enable() { enabled_ = true; }
    void disable() { enabled_ = false; }
    std::string getStatus() const { return enabled_ ? "active" : "inactive"; }
    void reset() {
        std::lock_guard<std::mutex> lock(mutex_);
        packet_stats_.clear();
        latency_values_.clear();
    }

    void recordPacketSent(const std::string& type, size_t bytes) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto& stats = packet_stats_[type];
        ++stats.packets_sent;
        stats.bytes_sent += bytes;
    }

    void recordPacketReceived(const std::string& type, size_t bytes) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto& stats = packet_stats_[type];
        ++stats.packets_received;
        stats.bytes_received += bytes;
    }

    std::map<std::string, PacketStats> getPacketStats() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return packet_stats_;
    }

    PacketStats getBandwidthUsage() const {
        std::lock_guard<std::mutex> lock(mutex_);
        PacketStats total;
        for (const auto& [_, stats] : packet_stats_) {
            total.packets_sent += stats.packets_sent;
            total.packets_received += stats.packets_received;
            total.bytes_sent += stats.bytes_sent;
            total.bytes_received += stats.bytes_received;
        }
        return total;
    }

    void recordLatency(double ms) {
        std::lock_guard<std::mutex> lock(mutex_);
        latency_values_.push_back(ms);
    }

    double getAverageLatency() const {
        std::lock_guard<std::mutex> lock(mutex_);
        if (latency_values_.empty()) return 0.0;
        double sum = 0.0;
        for (double v : latency_values_) sum += v;
        return sum / static_cast<double>(latency_values_.size());
    }

private:
    NetcodeDebugger() = default;
    ~NetcodeDebugger() = default;

    NetcodeDebugger(const NetcodeDebugger&) = delete;
    NetcodeDebugger& operator=(const NetcodeDebugger&) = delete;
    NetcodeDebugger(NetcodeDebugger&&) = delete;
    NetcodeDebugger& operator=(NetcodeDebugger&&) = delete;

    mutable std::mutex mutex_;
    bool enabled_ = false;
    std::string config_;
    std::map<std::string, PacketStats> packet_stats_;
    std::vector<double> latency_values_;
};

} // namespace nexus::utility::gamedev
