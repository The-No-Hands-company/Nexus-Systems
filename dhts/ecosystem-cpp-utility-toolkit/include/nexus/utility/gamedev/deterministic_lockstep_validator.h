#pragma once

#include <string>
#include <vector>
#include <functional>
#include <source_location>
#include <map>
#include <mutex>

namespace nexus::utility::gamedev {

class DeterministicLockstepValidator {
public:
    static DeterministicLockstepValidator& instance() {
        static DeterministicLockstepValidator inst;
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
        expected_hashes_.clear();
        mismatches_.clear();
    }

    void setExpectedHash(uint64_t tick, uint64_t hash) {
        std::lock_guard<std::mutex> lock(mutex_);
        expected_hashes_[tick] = hash;
    }

    bool validateHash(uint64_t tick, uint64_t actual_hash) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = expected_hashes_.find(tick);
        if (it == expected_hashes_.end()) {
            mismatches_[tick] = {0, actual_hash};
            return false;
        }
        if (it->second != actual_hash) {
            mismatches_[tick] = {it->second, actual_hash};
            return false;
        }
        return true;
    }

    std::map<uint64_t, std::pair<uint64_t, uint64_t>> getMismatches() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return mismatches_;
    }

    size_t getMismatchCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return mismatches_.size();
    }

private:
    DeterministicLockstepValidator() = default;
    ~DeterministicLockstepValidator() = default;

    DeterministicLockstepValidator(const DeterministicLockstepValidator&) = delete;
    DeterministicLockstepValidator& operator=(const DeterministicLockstepValidator&) = delete;
    DeterministicLockstepValidator(DeterministicLockstepValidator&&) = delete;
    DeterministicLockstepValidator& operator=(DeterministicLockstepValidator&&) = delete;

    mutable std::mutex mutex_;
    bool enabled_ = false;
    std::string config_;
    std::map<uint64_t, uint64_t> expected_hashes_;
    std::map<uint64_t, std::pair<uint64_t, uint64_t>> mismatches_;
};

} // namespace nexus::utility::gamedev
