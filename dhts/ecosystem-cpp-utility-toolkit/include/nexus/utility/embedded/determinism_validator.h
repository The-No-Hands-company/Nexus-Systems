#pragma once

#include <string>
#include <unordered_map>
#include <mutex>
#include <cstdint>

namespace nexus::utility::embedded {

class DeterminismValidator {
public:
    static DeterminismValidator& instance() {
        static DeterminismValidator inst;
        return inst;
    }

    void initialize() { std::lock_guard<std::mutex> lk(mutex_); enabled_ = true; }
    void shutdown() { std::lock_guard<std::mutex> lk(mutex_); enabled_ = false; }
    bool isEnabled() const { return enabled_; }

    void setExpectedOutput(const std::string& test, uint64_t hash) {
        std::lock_guard<std::mutex> lk(mutex_);
        expected_[test] = hash;
    }

    bool validateOutput(const std::string& test, uint64_t hash) {
        std::lock_guard<std::mutex> lk(mutex_);
        auto it = expected_.find(test);
        if (it == expected_.end()) return false;
        if (it->second != hash) {
            mismatch_count_++;
            return false;
        }
        return true;
    }

    size_t getMismatchCount() const {
        std::lock_guard<std::mutex> lk(mutex_);
        return mismatch_count_;
    }

private:
    DeterminismValidator() = default;
    ~DeterminismValidator() = default;
    mutable std::mutex mutex_;
    bool enabled_ = false;
    std::unordered_map<std::string, uint64_t> expected_;
    size_t mismatch_count_ = 0;
};

} // namespace nexus::utility::embedded
