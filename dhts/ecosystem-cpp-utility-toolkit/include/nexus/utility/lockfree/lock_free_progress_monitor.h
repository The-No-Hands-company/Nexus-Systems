#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <atomic>
#include <mutex>

namespace nexus::utility::lockfree {

/**
 * @brief Monitor lock-free progress guarantees per thread.
 */
class LockFreeProgressMonitor {
public:
    enum class Guarantee { WaitFree, LockFree, ObstructionFree, Blocking };

    struct ThreadProgress {
        std::size_t threadId = 0;
        std::size_t attempts = 0;
        std::size_t completions = 0;
        std::size_t consecutiveFailures = 0;
        std::size_t maxConsecutiveFailures = 0;
    };

    static LockFreeProgressMonitor& instance() {
        static LockFreeProgressMonitor inst;
        return inst;
    }

    void initialize(const std::string& config = "") {
        std::lock_guard<std::mutex> lk(mutex_);
        config_ = config;
        enabled_ = true;
    }
    void shutdown() { enabled_ = false; }
    bool isEnabled() const { return enabled_.load(); }

    void recordAttempt(std::size_t threadId, bool completed) {
        std::lock_guard<std::mutex> lk(mutex_);
        auto& p = threads_[threadId];
        p.threadId = threadId;
        ++p.attempts;
        if (completed) {
            ++p.completions;
            p.consecutiveFailures = 0;
            ++totalCompletions_;
        } else {
            ++p.consecutiveFailures;
            if (p.consecutiveFailures > p.maxConsecutiveFailures)
                p.maxConsecutiveFailures = p.consecutiveFailures;
        }
        ++totalAttempts_;
    }

    /// Estimate the strongest guarantee consistent with observed behavior.
    Guarantee estimateGuarantee() const {
        std::lock_guard<std::mutex> lk(mutex_);
        if (threads_.empty()) return Guarantee::WaitFree;
        std::size_t worst = 0;
        bool anyCompletion = totalCompletions_.load() > 0;
        for (const auto& [tid, p] : threads_)
            worst = std::max(worst, p.maxConsecutiveFailures);
        if (worst == 0) return Guarantee::WaitFree;      // everyone always progresses
        if (anyCompletion) return Guarantee::LockFree;   // system-wide progress
        return Guarantee::Blocking;
    }

    ThreadProgress progress(std::size_t threadId) const {
        std::lock_guard<std::mutex> lk(mutex_);
        auto it = threads_.find(threadId);
        return it == threads_.end() ? ThreadProgress{threadId, 0, 0, 0, 0} : it->second;
    }

    std::size_t totalAttempts() const { return totalAttempts_.load(); }
    std::size_t totalCompletions() const { return totalCompletions_.load(); }

    void reset() {
        std::lock_guard<std::mutex> lk(mutex_);
        threads_.clear();
        totalAttempts_ = 0;
        totalCompletions_ = 0;
    }

private:
    LockFreeProgressMonitor() = default;
    ~LockFreeProgressMonitor() = default;
    LockFreeProgressMonitor(const LockFreeProgressMonitor&) = delete;
    LockFreeProgressMonitor& operator=(const LockFreeProgressMonitor&) = delete;

    mutable std::mutex mutex_;
    std::atomic<bool> enabled_{false};
    std::string config_;
    std::unordered_map<std::size_t, ThreadProgress> threads_;
    std::atomic<std::size_t> totalAttempts_{0};
    std::atomic<std::size_t> totalCompletions_{0};
};

} // namespace nexus::utility::lockfree
