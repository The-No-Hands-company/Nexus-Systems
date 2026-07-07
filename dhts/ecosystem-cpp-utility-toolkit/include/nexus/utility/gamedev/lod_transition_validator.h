#pragma once

#include <string>
#include <vector>
#include <functional>
#include <source_location>
#include <mutex>

namespace nexus::utility::gamedev {

class LodTransitionValidator {
public:
    static LodTransitionValidator& instance() {
        static LodTransitionValidator inst;
        return inst;
    }

    void initialize(const std::string& config = "") { config_ = config; enabled_ = true; }
    void shutdown() { enabled_ = false; }
    bool isEnabled() const { return enabled_; }
    void enable() { enabled_ = true; }
    void disable() { enabled_ = false; }
    std::string getStatus() const { return enabled_ ? "active" : "inactive"; }
    void reset() {
        std::lock_guard<std::mutex> lock(mutex_);
        lod_distances_.clear();
        transition_count_ = 0;
    }

    void setLodLevels(const std::vector<double>& distances) {
        std::lock_guard<std::mutex> lock(mutex_);
        lod_distances_ = distances;
    }

    bool validateTransition(double distance, int from_lod, int to_lod) {
        std::lock_guard<std::mutex> lock(mutex_);
        ++transition_count_;

        if (from_lod < 0 || to_lod < 0) return false;
        int max_lod = static_cast<int>(lod_distances_.size());
        if (from_lod >= max_lod || to_lod >= max_lod) return false;

        int expected_lod = 0;
        for (size_t i = 0; i < lod_distances_.size(); ++i) {
            if (distance <= lod_distances_[i]) {
                expected_lod = static_cast<int>(i);
                break;
            }
            if (i == lod_distances_.size() - 1) {
                expected_lod = static_cast<int>(lod_distances_.size());
            }
        }

        return std::abs(to_lod - expected_lod) <= 1;
    }

    size_t getTransitionCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return transition_count_;
    }

private:
    LodTransitionValidator() = default;
    ~LodTransitionValidator() = default;

    LodTransitionValidator(const LodTransitionValidator&) = delete;
    LodTransitionValidator& operator=(const LodTransitionValidator&) = delete;
    LodTransitionValidator(LodTransitionValidator&&) = delete;
    LodTransitionValidator& operator=(LodTransitionValidator&&) = delete;

    mutable std::mutex mutex_;
    bool enabled_ = false;
    std::string config_;
    std::vector<double> lod_distances_;
    size_t transition_count_ = 0;
};

} // namespace nexus::utility::gamedev
