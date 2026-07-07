#pragma once

#include <string>
#include <vector>
#include <atomic>
#include <mutex>

namespace nexus::utility::simd {

/**
 * @brief Record and report whether loops were auto-vectorized by the compiler.
 */
class AutovectorizationChecker {
public:
    struct LoopInfo {
        std::string function;
        int line = 0;
        bool vectorized = false;
        int vectorWidth = 0;
        std::string reason;   // reason it was NOT vectorized
    };

    static AutovectorizationChecker& instance() {
        static AutovectorizationChecker inst;
        return inst;
    }

    void initialize(const std::string& config = "") {
        std::lock_guard<std::mutex> lk(mutex_);
        config_ = config;
        enabled_ = true;
    }
    void shutdown() { enabled_ = false; }
    bool isEnabled() const { return enabled_.load(); }

    void recordLoop(const std::string& function, int line, bool vectorized,
                    int vectorWidth = 0, const std::string& reason = "") {
        std::lock_guard<std::mutex> lk(mutex_);
        loops_.push_back({function, line, vectorized, vectorWidth, reason});
        if (vectorized) ++vectorized_; else ++notVectorized_;
    }

    std::vector<LoopInfo> unvectorizedLoops() const {
        std::lock_guard<std::mutex> lk(mutex_);
        std::vector<LoopInfo> out;
        for (const auto& l : loops_) if (!l.vectorized) out.push_back(l);
        return out;
    }

    double vectorizationRate() const {
        std::size_t v = vectorized_.load(), n = notVectorized_.load();
        return (v + n) == 0 ? 0.0 : static_cast<double>(v) / (v + n);
    }

    std::size_t vectorizedCount() const { return vectorized_.load(); }
    std::size_t notVectorizedCount() const { return notVectorized_.load(); }

    std::vector<LoopInfo> loops() const {
        std::lock_guard<std::mutex> lk(mutex_);
        return loops_;
    }

    void reset() {
        std::lock_guard<std::mutex> lk(mutex_);
        loops_.clear();
        vectorized_ = 0;
        notVectorized_ = 0;
    }

private:
    AutovectorizationChecker() = default;
    ~AutovectorizationChecker() = default;
    AutovectorizationChecker(const AutovectorizationChecker&) = delete;
    AutovectorizationChecker& operator=(const AutovectorizationChecker&) = delete;

    mutable std::mutex mutex_;
    std::atomic<bool> enabled_{false};
    std::string config_;
    std::vector<LoopInfo> loops_;
    std::atomic<std::size_t> vectorized_{0};
    std::atomic<std::size_t> notVectorized_{0};
};

} // namespace nexus::utility::simd
