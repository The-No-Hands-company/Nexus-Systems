#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <atomic>
#include <mutex>

namespace nexus::utility::ml {

/**
 * @brief Track convergence quality as a function of batch size.
 */
class BatchSizeOptimizer {
public:
    struct Trial {
        std::size_t batchSize = 0;
        double finalLoss = 0.0;
        std::size_t epochsToConverge = 0;
        double throughputSamplesPerSec = 0.0;
    };

    static BatchSizeOptimizer& instance() {
        static BatchSizeOptimizer inst;
        return inst;
    }

    void initialize(const std::string& config = "") {
        std::lock_guard<std::mutex> lk(mutex_);
        config_ = config;
        enabled_ = true;
    }
    void shutdown() { enabled_ = false; }
    bool isEnabled() const { return enabled_.load(); }

    void recordTrial(std::size_t batchSize, double finalLoss,
                     std::size_t epochsToConverge, double throughput = 0.0) {
        std::lock_guard<std::mutex> lk(mutex_);
        trials_[batchSize] = {batchSize, finalLoss, epochsToConverge, throughput};
    }

    /// Batch size with the lowest final loss.
    std::size_t bestForLoss() const {
        std::lock_guard<std::mutex> lk(mutex_);
        std::size_t best = 0;
        double bestLoss = 1e300;
        for (const auto& [bs, t] : trials_)
            if (t.finalLoss < bestLoss) { bestLoss = t.finalLoss; best = bs; }
        return best;
    }

    /// Batch size with the highest throughput.
    std::size_t bestForThroughput() const {
        std::lock_guard<std::mutex> lk(mutex_);
        std::size_t best = 0;
        double bestTp = -1.0;
        for (const auto& [bs, t] : trials_)
            if (t.throughputSamplesPerSec > bestTp) { bestTp = t.throughputSamplesPerSec; best = bs; }
        return best;
    }

    std::vector<Trial> trials() const {
        std::lock_guard<std::mutex> lk(mutex_);
        std::vector<Trial> out;
        for (const auto& [bs, t] : trials_) out.push_back(t);
        std::sort(out.begin(), out.end(),
                  [](const Trial& a, const Trial& b) { return a.batchSize < b.batchSize; });
        return out;
    }

    void reset() {
        std::lock_guard<std::mutex> lk(mutex_);
        trials_.clear();
    }

private:
    BatchSizeOptimizer() = default;
    ~BatchSizeOptimizer() = default;
    BatchSizeOptimizer(const BatchSizeOptimizer&) = delete;
    BatchSizeOptimizer& operator=(const BatchSizeOptimizer&) = delete;

    mutable std::mutex mutex_;
    std::atomic<bool> enabled_{false};
    std::string config_;
    std::unordered_map<std::size_t, Trial> trials_;
};

} // namespace nexus::utility::ml
