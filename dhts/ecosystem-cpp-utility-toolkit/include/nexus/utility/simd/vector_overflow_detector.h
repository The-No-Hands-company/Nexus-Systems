#pragma once

#include <string>
#include <vector>
#include <limits>
#include <algorithm>
#include <atomic>
#include <mutex>
#include <cstdint>

namespace nexus::utility::simd {

/**
 * @brief Detect overflow in SIMD vector integer operations.
 */
class VectorOverflowDetector {
public:
    struct OverflowEvent {
        std::string operation;
        std::size_t lane = 0;
        std::int64_t a = 0;
        std::int64_t b = 0;
    };

    static VectorOverflowDetector& instance() {
        static VectorOverflowDetector inst;
        return inst;
    }

    void initialize(const std::string& config = "") {
        std::lock_guard<std::mutex> lk(mutex_);
        config_ = config;
        enabled_ = true;
    }
    void shutdown() { enabled_ = false; }
    bool isEnabled() const { return enabled_.load(); }

    static bool addOverflows(std::int64_t a, std::int64_t b) {
        if (b > 0 && a > std::numeric_limits<std::int64_t>::max() - b) return true;
        if (b < 0 && a < std::numeric_limits<std::int64_t>::min() - b) return true;
        return false;
    }
    static bool mulOverflows(std::int64_t a, std::int64_t b) {
        if (a == 0 || b == 0) return false;
        std::int64_t r = a * b;
        return r / a != b;
    }

    /// Check a lane-wise add for overflow across all lanes.
    std::vector<std::size_t> checkAdd(const std::string& op,
                                      const std::vector<std::int64_t>& a,
                                      const std::vector<std::int64_t>& b) {
        std::vector<std::size_t> lanes;
        std::size_t n = std::min(a.size(), b.size());
        std::lock_guard<std::mutex> lk(mutex_);
        for (std::size_t i = 0; i < n; ++i) {
            if (addOverflows(a[i], b[i])) {
                lanes.push_back(i);
                events_.push_back({op, i, a[i], b[i]});
                ++overflowCount_;
            }
        }
        return lanes;
    }

    std::vector<std::size_t> checkMul(const std::string& op,
                                      const std::vector<std::int64_t>& a,
                                      const std::vector<std::int64_t>& b) {
        std::vector<std::size_t> lanes;
        std::size_t n = std::min(a.size(), b.size());
        std::lock_guard<std::mutex> lk(mutex_);
        for (std::size_t i = 0; i < n; ++i) {
            if (mulOverflows(a[i], b[i])) {
                lanes.push_back(i);
                events_.push_back({op, i, a[i], b[i]});
                ++overflowCount_;
            }
        }
        return lanes;
    }

    std::size_t overflowCount() const { return overflowCount_.load(); }

    std::vector<OverflowEvent> events() const {
        std::lock_guard<std::mutex> lk(mutex_);
        return events_;
    }

    void reset() {
        std::lock_guard<std::mutex> lk(mutex_);
        events_.clear();
        overflowCount_ = 0;
    }

private:
    VectorOverflowDetector() = default;
    ~VectorOverflowDetector() = default;
    VectorOverflowDetector(const VectorOverflowDetector&) = delete;
    VectorOverflowDetector& operator=(const VectorOverflowDetector&) = delete;

    mutable std::mutex mutex_;
    std::atomic<bool> enabled_{false};
    std::string config_;
    std::vector<OverflowEvent> events_;
    std::atomic<std::size_t> overflowCount_{0};
};

} // namespace nexus::utility::simd
