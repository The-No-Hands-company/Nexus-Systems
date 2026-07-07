#pragma once

#include <string>
#include <vector>
#include <algorithm>
#include <atomic>
#include <mutex>
#include <sstream>

namespace nexus::utility::simd {

/**
 * @brief Track per-lane values of SIMD registers for debugging.
 */
class SimdLaneDebugger {
public:
    struct LaneSnapshot {
        std::string label;
        std::vector<double> lanes;
    };

    static SimdLaneDebugger& instance() {
        static SimdLaneDebugger inst;
        return inst;
    }

    void initialize(const std::string& config = "") {
        std::lock_guard<std::mutex> lk(mutex_);
        config_ = config;
        enabled_ = true;
    }
    void shutdown() { enabled_ = false; }
    bool isEnabled() const { return enabled_.load(); }

    template<typename T>
    void capture(const std::string& label, const T* data, std::size_t laneCount) {
        std::vector<double> lanes;
        lanes.reserve(laneCount);
        for (std::size_t i = 0; i < laneCount; ++i)
            lanes.push_back(static_cast<double>(data[i]));
        std::lock_guard<std::mutex> lk(mutex_);
        snapshots_.push_back({label, std::move(lanes)});
    }

    void capture(const std::string& label, const std::vector<double>& lanes) {
        std::lock_guard<std::mutex> lk(mutex_);
        snapshots_.push_back({label, lanes});
    }

    /// Find lanes that differ between the two most recent snapshots.
    std::vector<std::size_t> divergentLanes() const {
        std::lock_guard<std::mutex> lk(mutex_);
        std::vector<std::size_t> out;
        if (snapshots_.size() < 2) return out;
        const auto& a = snapshots_[snapshots_.size() - 2].lanes;
        const auto& b = snapshots_.back().lanes;
        std::size_t n = std::min(a.size(), b.size());
        for (std::size_t i = 0; i < n; ++i) if (a[i] != b[i]) out.push_back(i);
        return out;
    }

    std::string format(const LaneSnapshot& s) const {
        std::ostringstream os;
        os << s.label << ": [";
        for (std::size_t i = 0; i < s.lanes.size(); ++i) {
            if (i) os << ", ";
            os << s.lanes[i];
        }
        os << "]";
        return os.str();
    }

    std::vector<LaneSnapshot> snapshots() const {
        std::lock_guard<std::mutex> lk(mutex_);
        return snapshots_;
    }

    void reset() {
        std::lock_guard<std::mutex> lk(mutex_);
        snapshots_.clear();
    }

private:
    SimdLaneDebugger() = default;
    ~SimdLaneDebugger() = default;
    SimdLaneDebugger(const SimdLaneDebugger&) = delete;
    SimdLaneDebugger& operator=(const SimdLaneDebugger&) = delete;

    mutable std::mutex mutex_;
    std::atomic<bool> enabled_{false};
    std::string config_;
    std::vector<LaneSnapshot> snapshots_;
};

} // namespace nexus::utility::simd
