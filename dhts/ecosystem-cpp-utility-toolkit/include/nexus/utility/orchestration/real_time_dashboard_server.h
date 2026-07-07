#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <deque>
#include <atomic>
#include <mutex>
#include <chrono>
#include <cstdint>

namespace nexus::utility::orchestration {

/**
 * @brief Push metric updates with timestamps for a real-time dashboard.
 */
class RealTimeDashboardServer {
public:
    struct Update {
        std::string metric;
        double value = 0.0;
        std::uint64_t timestampMs = 0;
    };

    static RealTimeDashboardServer& instance() {
        static RealTimeDashboardServer inst;
        return inst;
    }

    void initialize(const std::string& config = "") {
        std::lock_guard<std::mutex> lk(mutex_);
        config_ = config;
        enabled_ = true;
    }
    void shutdown() { enabled_ = false; }
    bool isEnabled() const { return enabled_.load(); }

    void setHistoryLimit(std::size_t limit) {
        std::lock_guard<std::mutex> lk(mutex_);
        historyLimit_ = limit;
    }

    void push(const std::string& metric, double value) {
        auto ts = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        push(metric, value, static_cast<std::uint64_t>(ts));
    }

    void push(const std::string& metric, double value, std::uint64_t timestampMs) {
        std::lock_guard<std::mutex> lk(mutex_);
        auto& q = streams_[metric];
        q.push_back({metric, value, timestampMs});
        while (q.size() > historyLimit_) q.pop_front();
        latest_[metric] = {metric, value, timestampMs};
        ++totalPushes_;
    }

    Update latest(const std::string& metric) const {
        std::lock_guard<std::mutex> lk(mutex_);
        auto it = latest_.find(metric);
        return it == latest_.end() ? Update{metric, 0.0, 0} : it->second;
    }

    std::vector<Update> history(const std::string& metric) const {
        std::lock_guard<std::mutex> lk(mutex_);
        std::vector<Update> out;
        auto it = streams_.find(metric);
        if (it != streams_.end()) out.assign(it->second.begin(), it->second.end());
        return out;
    }

    std::size_t totalPushes() const { return totalPushes_.load(); }

    void reset() {
        std::lock_guard<std::mutex> lk(mutex_);
        streams_.clear();
        latest_.clear();
        totalPushes_ = 0;
    }

private:
    RealTimeDashboardServer() = default;
    ~RealTimeDashboardServer() = default;
    RealTimeDashboardServer(const RealTimeDashboardServer&) = delete;
    RealTimeDashboardServer& operator=(const RealTimeDashboardServer&) = delete;

    mutable std::mutex mutex_;
    std::atomic<bool> enabled_{false};
    std::string config_;
    std::size_t historyLimit_ = 256;
    std::unordered_map<std::string, std::deque<Update>> streams_;
    std::unordered_map<std::string, Update> latest_;
    std::atomic<std::size_t> totalPushes_{0};
};

} // namespace nexus::utility::orchestration
