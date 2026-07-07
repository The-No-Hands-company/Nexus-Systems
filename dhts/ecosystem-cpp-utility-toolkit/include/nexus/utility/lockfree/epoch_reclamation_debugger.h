#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <atomic>
#include <mutex>
#include <cstdint>

namespace nexus::utility::lockfree {

/**
 * @brief Debug epoch-based reclamation (EBR) for lock-free memory management.
 */
class EpochReclamationDebugger {
public:
    static EpochReclamationDebugger& instance() {
        static EpochReclamationDebugger inst;
        return inst;
    }

    void initialize(const std::string& config = "") {
        std::lock_guard<std::mutex> lk(mutex_);
        config_ = config;
        enabled_ = true;
    }
    void shutdown() { enabled_ = false; }
    bool isEnabled() const { return enabled_.load(); }

    /// Advance the global epoch.
    std::uint64_t advanceEpoch() {
        return globalEpoch_.fetch_add(1) + 1;
    }
    std::uint64_t currentEpoch() const { return globalEpoch_.load(); }

    /// A thread enters a critical section, pinning it to the current epoch.
    void enterCritical(std::size_t threadId) {
        std::lock_guard<std::mutex> lk(mutex_);
        threadEpochs_[threadId] = globalEpoch_.load();
    }
    void exitCritical(std::size_t threadId) {
        std::lock_guard<std::mutex> lk(mutex_);
        threadEpochs_.erase(threadId);
    }

    /// Retire a node in the current epoch (deferred free).
    void retire(void* ptr) {
        std::lock_guard<std::mutex> lk(mutex_);
        retired_.push_back({ptr, globalEpoch_.load()});
    }

    /// Minimum epoch pinned by any active thread; retired nodes older are safe.
    std::uint64_t safeEpoch() const {
        std::lock_guard<std::mutex> lk(mutex_);
        std::uint64_t minEpoch = globalEpoch_.load();
        for (const auto& [tid, e] : threadEpochs_) if (e < minEpoch) minEpoch = e;
        return minEpoch;
    }

    /// Reclaim nodes whose retire epoch is older than the safe epoch.
    std::size_t reclaim() {
        std::lock_guard<std::mutex> lk(mutex_);
        std::uint64_t safe = globalEpoch_.load();
        for (const auto& [tid, e] : threadEpochs_) if (e < safe) safe = e;
        std::size_t before = retired_.size();
        std::vector<RetiredNode> remaining;
        for (const auto& n : retired_) {
            if (n.epoch < safe) ++reclaimed_;
            else remaining.push_back(n);
        }
        retired_.swap(remaining);
        return before - retired_.size();
    }

    std::size_t pendingRetired() const {
        std::lock_guard<std::mutex> lk(mutex_);
        return retired_.size();
    }
    std::size_t totalReclaimed() const { return reclaimed_.load(); }

    void reset() {
        std::lock_guard<std::mutex> lk(mutex_);
        threadEpochs_.clear();
        retired_.clear();
        globalEpoch_ = 0;
        reclaimed_ = 0;
    }

private:
    EpochReclamationDebugger() = default;
    ~EpochReclamationDebugger() = default;
    EpochReclamationDebugger(const EpochReclamationDebugger&) = delete;
    EpochReclamationDebugger& operator=(const EpochReclamationDebugger&) = delete;

    struct RetiredNode { void* ptr; std::uint64_t epoch; };

    mutable std::mutex mutex_;
    std::atomic<bool> enabled_{false};
    std::string config_;
    std::atomic<std::uint64_t> globalEpoch_{0};
    std::unordered_map<std::size_t, std::uint64_t> threadEpochs_;
    std::vector<RetiredNode> retired_;
    std::atomic<std::size_t> reclaimed_{0};
};

} // namespace nexus::utility::lockfree
