#pragma once

#include <string>
#include <atomic>
#include <mutex>
#include <unordered_map>
#include <cstddef>

namespace nexus::utility::simd {

/**
 * @brief Profile SIMD vectorization by comparing scalar and vectorized timings.
 */
class VectorizationProfiler {
public:
    static VectorizationProfiler& instance() {
        static VectorizationProfiler inst;
        return inst;
    }

    void initialize(const std::string& config = "") {
        std::lock_guard<std::mutex> lk(mutex_);
        config_ = config;
        enabled_ = true;
    }
    void shutdown() { enabled_ = false; }
    void enable() { enabled_ = true; }
    void disable() { enabled_ = false; }
    bool isEnabled() const { return enabled_.load(); }

    /// Record a scalar-path timing (microseconds) for @p operation.
    void recordScalar(const std::string& operation, double us) {
        std::lock_guard<std::mutex> lk(mutex_);
        auto& e = ops_[operation];
        e.scalarSum += us;
        ++e.scalarCount;
    }

    /// Record a vectorized-path timing (microseconds) for @p operation.
    void recordVectorized(const std::string& operation, double us) {
        std::lock_guard<std::mutex> lk(mutex_);
        auto& e = ops_[operation];
        e.vectorSum += us;
        ++e.vectorCount;
    }

    /// Speedup = mean scalar time / mean vectorized time for @p operation.
    double getSpeedup(const std::string& operation) const {
        std::lock_guard<std::mutex> lk(mutex_);
        auto it = ops_.find(operation);
        if (it == ops_.end()) return 0.0;
        return speedupOf(it->second);
    }

    /// Average speedup across all operations that have both timings recorded.
    double getVectorizationEfficiency() const {
        std::lock_guard<std::mutex> lk(mutex_);
        double sum = 0.0;
        std::size_t n = 0;
        for (const auto& [name, e] : ops_) {
            double s = speedupOf(e);
            if (s > 0.0) { sum += s; ++n; }
        }
        return n ? sum / n : 0.0;
    }

    /// Number of distinct operations tracked.
    std::size_t getOperationCount() const {
        std::lock_guard<std::mutex> lk(mutex_);
        return ops_.size();
    }

    std::string getStatus() const {
        std::lock_guard<std::mutex> lk(mutex_);
        return "VectorizationProfiler{operations=" + std::to_string(ops_.size()) + "}";
    }

    void reset() {
        std::lock_guard<std::mutex> lk(mutex_);
        ops_.clear();
    }

private:
    VectorizationProfiler() = default;
    ~VectorizationProfiler() = default;
    VectorizationProfiler(const VectorizationProfiler&) = delete;
    VectorizationProfiler& operator=(const VectorizationProfiler&) = delete;
    VectorizationProfiler(VectorizationProfiler&&) = delete;
    VectorizationProfiler& operator=(VectorizationProfiler&&) = delete;

    struct Entry {
        double scalarSum = 0.0;
        double vectorSum = 0.0;
        std::size_t scalarCount = 0;
        std::size_t vectorCount = 0;
    };

    static double speedupOf(const Entry& e) {
        if (!e.scalarCount || !e.vectorCount) return 0.0;
        double meanScalar = e.scalarSum / e.scalarCount;
        double meanVector = e.vectorSum / e.vectorCount;
        return meanVector > 0.0 ? meanScalar / meanVector : 0.0;
    }

    mutable std::mutex mutex_;
    std::atomic<bool> enabled_{false};
    std::string config_;
    std::unordered_map<std::string, Entry> ops_;
};

} // namespace nexus::utility::simd
