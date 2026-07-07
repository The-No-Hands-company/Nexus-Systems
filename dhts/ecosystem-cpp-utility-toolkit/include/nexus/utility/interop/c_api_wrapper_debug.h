#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <atomic>
#include <mutex>

namespace nexus::utility::interop {

/**
 * @brief Track C API calls crossing the language boundary.
 */
class CApiWrapperDebug {
public:
    struct CallStats {
        std::string function;
        std::size_t calls = 0;
        std::size_t errors = 0;
        double totalUs = 0.0;
        double averageUs() const { return calls ? totalUs / calls : 0.0; }
    };

    static CApiWrapperDebug& instance() {
        static CApiWrapperDebug inst;
        return inst;
    }

    void initialize(const std::string& config = "") {
        std::lock_guard<std::mutex> lk(mutex_);
        config_ = config;
        enabled_ = true;
    }
    void shutdown() { enabled_ = false; }
    bool isEnabled() const { return enabled_.load(); }

    void recordCall(const std::string& function, double durationUs = 0.0, bool error = false) {
        std::lock_guard<std::mutex> lk(mutex_);
        auto& s = stats_[function];
        s.function = function;
        ++s.calls;
        s.totalUs += durationUs;
        if (error) ++s.errors;
        ++total_;
    }

    CallStats stats(const std::string& function) const {
        std::lock_guard<std::mutex> lk(mutex_);
        auto it = stats_.find(function);
        return it == stats_.end() ? CallStats{function, 0, 0, 0} : it->second;
    }

    std::vector<CallStats> allStats() const {
        std::lock_guard<std::mutex> lk(mutex_);
        std::vector<CallStats> out;
        for (const auto& [f, s] : stats_) out.push_back(s);
        return out;
    }

    std::size_t totalCalls() const { return total_.load(); }

    void reset() {
        std::lock_guard<std::mutex> lk(mutex_);
        stats_.clear();
        total_ = 0;
    }

private:
    CApiWrapperDebug() = default;
    ~CApiWrapperDebug() = default;
    CApiWrapperDebug(const CApiWrapperDebug&) = delete;
    CApiWrapperDebug& operator=(const CApiWrapperDebug&) = delete;

    mutable std::mutex mutex_;
    std::atomic<bool> enabled_{false};
    std::string config_;
    std::unordered_map<std::string, CallStats> stats_;
    std::atomic<std::size_t> total_{0};
};

} // namespace nexus::utility::interop
