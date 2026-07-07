#pragma once

#include <string>
#include <map>
#include <mutex>

namespace nexus::utility::cloud {

class ServerlessColdStartProfiler {
public:
    struct FunctionStats {
        size_t cold_starts = 0;
        size_t warm_starts = 0;
        double total_cold_time = 0.0;
    };

    static ServerlessColdStartProfiler& instance() {
        static ServerlessColdStartProfiler inst;
        return inst;
    }

    void initialize(const std::string& config = "") { std::lock_guard<std::mutex> lk(mutex_); enabled_ = true; config_ = config; }
    void shutdown() { std::lock_guard<std::mutex> lk(mutex_); enabled_ = false; }
    bool isEnabled() const { return enabled_; }
    void enable() { std::lock_guard<std::mutex> lk(mutex_); enabled_ = true; }
    void disable() { std::lock_guard<std::mutex> lk(mutex_); enabled_ = false; }
    void reset() { std::lock_guard<std::mutex> lk(mutex_); functions_.clear(); }

    void recordColdStart(const std::string& function, double ms) {
        std::lock_guard<std::mutex> lk(mutex_);
        if (!enabled_) return;
        auto& s = functions_[function];
        s.cold_starts++;
        s.total_cold_time += ms;
    }

    void recordWarmStart(const std::string& function, double ms) {
        std::lock_guard<std::mutex> lk(mutex_);
        if (!enabled_) return;
        functions_[function].warm_starts++;
    }

    double getColdStartRatio(const std::string& function) const {
        std::lock_guard<std::mutex> lk(mutex_);
        auto it = functions_.find(function);
        if (it == functions_.end()) return 0.0;
        size_t total = it->second.cold_starts + it->second.warm_starts;
        if (total == 0) return 0.0;
        return (static_cast<double>(it->second.cold_starts) / total) * 100.0;
    }

    double getAvgColdStartTime(const std::string& function) const {
        std::lock_guard<std::mutex> lk(mutex_);
        auto it = functions_.find(function);
        if (it == functions_.end() || it->second.cold_starts == 0) return 0.0;
        return it->second.total_cold_time / it->second.cold_starts;
    }

private:
    ServerlessColdStartProfiler() = default;
    ~ServerlessColdStartProfiler() = default;
    ServerlessColdStartProfiler(const ServerlessColdStartProfiler&) = delete;
    ServerlessColdStartProfiler& operator=(const ServerlessColdStartProfiler&) = delete;
    ServerlessColdStartProfiler(ServerlessColdStartProfiler&&) = delete;
    ServerlessColdStartProfiler& operator=(ServerlessColdStartProfiler&&) = delete;

    mutable std::mutex mutex_;
    bool enabled_ = false;
    std::string config_;
    std::map<std::string, FunctionStats> functions_;
};

} // namespace nexus::utility::cloud
