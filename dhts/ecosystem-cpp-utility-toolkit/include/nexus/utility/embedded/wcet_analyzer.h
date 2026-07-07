#pragma once

#include <string>
#include <map>
#include <mutex>

namespace nexus::utility::embedded {

class WcetAnalyzer {
public:
    struct ExecStats {
        double wcet = 0.0;
        double total = 0.0;
        size_t count = 0;
    };

    static WcetAnalyzer& instance() {
        static WcetAnalyzer inst;
        return inst;
    }

    void initialize() { std::lock_guard<std::mutex> lk(mutex_); enabled_ = true; }
    void shutdown() { std::lock_guard<std::mutex> lk(mutex_); enabled_ = false; }
    bool isEnabled() const { return enabled_; }

    void recordExecution(const std::string& task, double us) {
        std::lock_guard<std::mutex> lk(mutex_);
        if (!enabled_) return;
        auto& s = tasks_[task];
        s.count++;
        s.total += us;
        if (us > s.wcet) s.wcet = us;
    }

    double getWCET(const std::string& task) const {
        std::lock_guard<std::mutex> lk(mutex_);
        auto it = tasks_.find(task);
        return (it != tasks_.end()) ? it->second.wcet : 0.0;
    }

    double getAvgExecTime(const std::string& task) const {
        std::lock_guard<std::mutex> lk(mutex_);
        auto it = tasks_.find(task);
        if (it == tasks_.end() || it->second.count == 0) return 0.0;
        return it->second.total / it->second.count;
    }

    size_t getExecutionCount(const std::string& task) const {
        std::lock_guard<std::mutex> lk(mutex_);
        auto it = tasks_.find(task);
        return (it != tasks_.end()) ? it->second.count : 0;
    }

private:
    WcetAnalyzer() = default;
    ~WcetAnalyzer() = default;
    mutable std::mutex mutex_;
    bool enabled_ = false;
    std::map<std::string, ExecStats> tasks_;
};

} // namespace nexus::utility::embedded
