#pragma once

#include <string>
#include <vector>
#include <functional>
#include <source_location>
#include <unordered_set>
#include <mutex>

namespace nexus::utility::gamedev {

struct Keyframe {
    double time = 0.0;
    std::vector<double> values;
};

struct Bone {
    std::string name;
    int parent_index = -1;
};

class AnimationValidator {
public:
    static AnimationValidator& instance() {
        static AnimationValidator inst;
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
        error_count_ = 0;
    }

    bool validateKeyframes(const std::vector<Keyframe>& frames) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (frames.empty()) {
            ++error_count_;
            return false;
        }
        bool valid = true;
        double prev_time = frames[0].time;
        for (size_t i = 1; i < frames.size(); ++i) {
            if (frames[i].time <= prev_time) {
                ++error_count_;
                valid = false;
                break;
            }
            prev_time = frames[i].time;
        }
        if (valid && !frames.empty() && !frames[0].values.empty()) {
            size_t expected = frames[0].values.size();
            for (const auto& f : frames) {
                if (f.values.size() != expected) {
                    ++error_count_;
                    valid = false;
                    break;
                }
            }
        }
        return valid;
    }

    bool validateBoneHierarchy(const std::vector<Bone>& bones) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (bones.empty()) {
            ++error_count_;
            return false;
        }

        for (size_t i = 0; i < bones.size(); ++i) {
            int parent = bones[i].parent_index;
            if (parent >= static_cast<int>(bones.size())) {
                ++error_count_;
                return false;
            }
        }

        std::unordered_set<int> visited;
        for (size_t i = 0; i < bones.size(); ++i) {
            visited.clear();
            int current = static_cast<int>(i);
            while (current != -1) {
                if (visited.count(current)) {
                    ++error_count_;
                    return false;
                }
                visited.insert(current);
                current = bones[current].parent_index;
            }
        }

        return true;
    }

    size_t getErrorCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return error_count_;
    }

private:
    AnimationValidator() = default;
    ~AnimationValidator() = default;

    AnimationValidator(const AnimationValidator&) = delete;
    AnimationValidator& operator=(const AnimationValidator&) = delete;
    AnimationValidator(AnimationValidator&&) = delete;
    AnimationValidator& operator=(AnimationValidator&&) = delete;

    mutable std::mutex mutex_;
    bool enabled_ = false;
    std::string config_;
    size_t error_count_ = 0;
};

} // namespace nexus::utility::gamedev
