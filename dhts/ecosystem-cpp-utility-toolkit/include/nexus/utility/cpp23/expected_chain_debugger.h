#pragma once

#include <string>
#include <unordered_map>
#include <mutex>
#include <algorithm>

namespace nexus::utility::cpp23 {

/// @brief Debug std::expected monadic chains: depth, success rate.
class ExpectedChainDebugger {
public:
    struct ChainStats {
        size_t invocations = 0;
        size_t successes = 0;
        size_t max_depth = 0;
    };

    static ExpectedChainDebugger& instance() {
        static ExpectedChainDebugger inst;
        return inst;
    }

    void initialize() {
        std::lock_guard<std::mutex> lock(mutex_);
        enabled_ = true;
    }

    void shutdown() {
        std::lock_guard<std::mutex> lock(mutex_);
        enabled_ = false;
        chains_.clear();
    }

    bool isEnabled() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return enabled_;
    }

    void recordChain(const std::string& name, size_t depth, bool success) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto& s = chains_[name];
        ++s.invocations;
        if (success) ++s.successes;
        s.max_depth = std::max(s.max_depth, depth);
    }

    ChainStats getChainStats(const std::string& name) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = chains_.find(name);
        return it != chains_.end() ? it->second : ChainStats{};
    }

    /// Fraction of successful invocations in [0,1].
    double getSuccessRate(const std::string& name) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = chains_.find(name);
        if (it == chains_.end() || it->second.invocations == 0) return 0.0;
        return static_cast<double>(it->second.successes) /
               static_cast<double>(it->second.invocations);
    }

    size_t getMaxDepth(const std::string& name) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = chains_.find(name);
        return it != chains_.end() ? it->second.max_depth : 0;
    }

private:
    ExpectedChainDebugger() = default;
    ~ExpectedChainDebugger() = default;
    ExpectedChainDebugger(const ExpectedChainDebugger&) = delete;
    ExpectedChainDebugger& operator=(const ExpectedChainDebugger&) = delete;

    mutable std::mutex mutex_;
    bool enabled_ = false;
    std::unordered_map<std::string, ChainStats> chains_;
};

} // namespace nexus::utility::cpp23
