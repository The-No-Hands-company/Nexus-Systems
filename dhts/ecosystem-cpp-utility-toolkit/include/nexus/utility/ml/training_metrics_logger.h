#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <atomic>
#include <mutex>

namespace nexus::utility::ml {

/**
 * @brief Log training metrics (loss, accuracy, ...) per epoch.
 */
class TrainingMetricsLogger {
public:
    struct EpochMetrics {
        std::size_t epoch = 0;
        double trainLoss = 0.0;
        double valLoss = 0.0;
        double trainAccuracy = 0.0;
        double valAccuracy = 0.0;
        double learningRate = 0.0;
    };

    static TrainingMetricsLogger& instance() {
        static TrainingMetricsLogger inst;
        return inst;
    }

    void initialize(const std::string& config = "") {
        std::lock_guard<std::mutex> lk(mutex_);
        config_ = config;
        enabled_ = true;
    }
    void shutdown() { enabled_ = false; }
    bool isEnabled() const { return enabled_.load(); }

    void logEpoch(const EpochMetrics& m) {
        std::lock_guard<std::mutex> lk(mutex_);
        epochs_.push_back(m);
    }

    EpochMetrics best(bool byValLoss = true) const {
        std::lock_guard<std::mutex> lk(mutex_);
        EpochMetrics best{};
        bool first = true;
        for (const auto& e : epochs_) {
            double metric = byValLoss ? e.valLoss : -e.valAccuracy;
            double bestMetric = byValLoss ? best.valLoss : -best.valAccuracy;
            if (first || metric < bestMetric) { best = e; first = false; }
        }
        return best;
    }

    /// Detect overfitting: validation loss rising while training loss falls.
    bool isOverfitting(std::size_t window = 3) const {
        std::lock_guard<std::mutex> lk(mutex_);
        if (epochs_.size() < window + 1) return false;
        std::size_t n = epochs_.size();
        double valDelta = epochs_[n - 1].valLoss - epochs_[n - 1 - window].valLoss;
        double trainDelta = epochs_[n - 1].trainLoss - epochs_[n - 1 - window].trainLoss;
        return valDelta > 0.0 && trainDelta < 0.0;
    }

    std::vector<EpochMetrics> history() const {
        std::lock_guard<std::mutex> lk(mutex_);
        return epochs_;
    }

    std::size_t epochCount() const {
        std::lock_guard<std::mutex> lk(mutex_);
        return epochs_.size();
    }

    void reset() {
        std::lock_guard<std::mutex> lk(mutex_);
        epochs_.clear();
    }

private:
    TrainingMetricsLogger() = default;
    ~TrainingMetricsLogger() = default;
    TrainingMetricsLogger(const TrainingMetricsLogger&) = delete;
    TrainingMetricsLogger& operator=(const TrainingMetricsLogger&) = delete;

    mutable std::mutex mutex_;
    std::atomic<bool> enabled_{false};
    std::string config_;
    std::vector<EpochMetrics> epochs_;
};

} // namespace nexus::utility::ml
