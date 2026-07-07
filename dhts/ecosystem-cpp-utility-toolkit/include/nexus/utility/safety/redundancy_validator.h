#pragma once

#include <cstddef>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <vector>

namespace nexus::utility::safety {

/// @brief Validates system redundancy by tracking primary/backup pairs.
class RedundancyValidator {
public:
    static RedundancyValidator& instance() {
        static RedundancyValidator inst;
        return inst;
    }

    void initialize(const std::string& config = "") {
        std::lock_guard<std::mutex> lock(mutex_);
        enabled_ = true;
        config_ = config;
    }

    void shutdown() {
        std::lock_guard<std::mutex> lock(mutex_);
        enabled_ = false;
        backups_.clear();
        components_.clear();
    }

    bool isEnabled() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return enabled_;
    }

    void addComponent(const std::string& name) {
        std::lock_guard<std::mutex> lock(mutex_);
        components_.insert(name);
    }

    void addRedundantPair(const std::string& primary, const std::string& backup) {
        std::lock_guard<std::mutex> lock(mutex_);
        components_.insert(primary);
        components_.insert(backup);
        backups_[primary] = backup;
    }

    /// @brief True if the component has a backup assigned.
    bool checkRedundancy(const std::string& component) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = backups_.find(component);
        return it != backups_.end() && !it->second.empty();
    }

    /// @brief Components that lack a backup (and are not themselves acting as a backup).
    std::vector<std::string> getSinglePointFailures() const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::set<std::string> is_backup;
        for (const auto& [_, backup] : backups_) is_backup.insert(backup);

        std::vector<std::string> spofs;
        for (const auto& component : components_) {
            bool has_backup = backups_.find(component) != backups_.end();
            if (!has_backup && is_backup.find(component) == is_backup.end()) {
                spofs.push_back(component);
            }
        }
        return spofs;
    }

    void reset() {
        std::lock_guard<std::mutex> lock(mutex_);
        backups_.clear();
        components_.clear();
    }

private:
    RedundancyValidator() = default;
    ~RedundancyValidator() = default;
    RedundancyValidator(const RedundancyValidator&) = delete;
    RedundancyValidator& operator=(const RedundancyValidator&) = delete;

    mutable std::mutex mutex_;
    bool enabled_ = false;
    std::string config_;
    std::map<std::string, std::string> backups_;
    std::set<std::string> components_;
};

} // namespace nexus::utility::safety
