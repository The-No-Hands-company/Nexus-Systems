#pragma once

#include <string>
#include <vector>
#include <mutex>

namespace nexus::utility::apivalidation {

class NullParameterDetector {
public:
    struct NullCall {
        std::string function;
        std::string parameter;
    };

    static NullParameterDetector& instance() {
        static NullParameterDetector inst;
        return inst;
    }

    void initialize(const std::string& config = "") { std::lock_guard<std::mutex> lk(mutex_); enabled_ = true; config_ = config; }
    void shutdown() { std::lock_guard<std::mutex> lk(mutex_); enabled_ = false; }
    bool isEnabled() const { return enabled_; }
    void enable() { std::lock_guard<std::mutex> lk(mutex_); enabled_ = true; }
    void disable() { std::lock_guard<std::mutex> lk(mutex_); enabled_ = false; }
    void reset() { std::lock_guard<std::mutex> lk(mutex_); null_calls_.clear(); }

    void checkParam(const std::string& func, const std::string& param, const void* ptr) {
        std::lock_guard<std::mutex> lk(mutex_);
        if (!enabled_) return;
        if (ptr == nullptr) {
            null_calls_.push_back({func, param});
        }
    }

    size_t getNullCallCount() const {
        std::lock_guard<std::mutex> lk(mutex_);
        return null_calls_.size();
    }

    std::vector<NullCall> getNullCalls() const {
        std::lock_guard<std::mutex> lk(mutex_);
        return null_calls_;
    }

private:
    NullParameterDetector() = default;
    ~NullParameterDetector() = default;
    NullParameterDetector(const NullParameterDetector&) = delete;
    NullParameterDetector& operator=(const NullParameterDetector&) = delete;
    NullParameterDetector(NullParameterDetector&&) = delete;
    NullParameterDetector& operator=(NullParameterDetector&&) = delete;

    mutable std::mutex mutex_;
    bool enabled_ = false;
    std::string config_;
    std::vector<NullCall> null_calls_;
};

} // namespace nexus::utility::apivalidation
