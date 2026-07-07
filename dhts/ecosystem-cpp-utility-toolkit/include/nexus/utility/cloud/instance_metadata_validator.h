#pragma once

#include <string>
#include <vector>
#include <set>
#include <map>
#include <mutex>
#include <algorithm>

namespace nexus::utility::cloud {

class InstanceMetadataValidator {
public:
    static InstanceMetadataValidator& instance() {
        static InstanceMetadataValidator inst;
        return inst;
    }

    void initialize(const std::string& config = "") { std::lock_guard<std::mutex> lk(mutex_); enabled_ = true; config_ = config; }
    void shutdown() { std::lock_guard<std::mutex> lk(mutex_); enabled_ = false; }
    bool isEnabled() const { return enabled_; }
    void enable() { std::lock_guard<std::mutex> lk(mutex_); enabled_ = true; }
    void disable() { std::lock_guard<std::mutex> lk(mutex_); enabled_ = false; }
    void reset() { std::lock_guard<std::mutex> lk(mutex_); expected_keys_.clear(); }

    void setExpectedKeys(const std::vector<std::string>& keys) {
        std::lock_guard<std::mutex> lk(mutex_);
        expected_keys_.insert(keys.begin(), keys.end());
    }

    bool validate(const std::map<std::string, std::string>& metadata) const {
        std::lock_guard<std::mutex> lk(mutex_);
        for (const auto& key : expected_keys_) {
            if (metadata.find(key) == metadata.end()) return false;
        }
        return true;
    }

    std::vector<std::string> getMissingKeys() const {
        std::lock_guard<std::mutex> lk(mutex_);
        return std::vector<std::string>(expected_keys_.begin(), expected_keys_.end());
    }

private:
    InstanceMetadataValidator() = default;
    ~InstanceMetadataValidator() = default;
    InstanceMetadataValidator(const InstanceMetadataValidator&) = delete;
    InstanceMetadataValidator& operator=(const InstanceMetadataValidator&) = delete;
    InstanceMetadataValidator(InstanceMetadataValidator&&) = delete;
    InstanceMetadataValidator& operator=(InstanceMetadataValidator&&) = delete;

    mutable std::mutex mutex_;
    bool enabled_ = false;
    std::string config_;
    std::set<std::string> expected_keys_;
};

} // namespace nexus::utility::cloud
