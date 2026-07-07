#pragma once

#include <cstddef>
#include <map>
#include <mutex>
#include <string>
#include <vector>

namespace nexus::utility::safety {

/// @brief Tracks safety certification requirements and their compliance evidence.
class CertificationHelper {
public:
    struct Requirement {
        std::string id;
        std::string description;
        bool met = false;
        std::string evidence;
    };

    static CertificationHelper& instance() {
        static CertificationHelper inst;
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
        requirements_.clear();
        order_.clear();
    }

    bool isEnabled() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return enabled_;
    }

    void addRequirement(const std::string& id, const std::string& description) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (requirements_.find(id) == requirements_.end()) {
            order_.push_back(id);
        }
        auto& r = requirements_[id];
        r.id = id;
        r.description = description;
    }

    void markRequirementMet(const std::string& id, const std::string& evidence) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = requirements_.find(id);
        if (it != requirements_.end()) {
            it->second.met = true;
            it->second.evidence = evidence;
        }
    }

    std::vector<Requirement> getUnmetRequirements() const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<Requirement> out;
        for (const auto& id : order_) {
            const auto& r = requirements_.at(id);
            if (!r.met) out.push_back(r);
        }
        return out;
    }

    /// @brief Percentage of requirements marked met (0.0 if none defined).
    double getCompliancePercent() const {
        std::lock_guard<std::mutex> lock(mutex_);
        if (requirements_.empty()) return 0.0;
        size_t met = 0;
        for (const auto& [_, r] : requirements_) {
            if (r.met) ++met;
        }
        return 100.0 * static_cast<double>(met) / static_cast<double>(requirements_.size());
    }

    void reset() {
        std::lock_guard<std::mutex> lock(mutex_);
        requirements_.clear();
        order_.clear();
    }

private:
    CertificationHelper() = default;
    ~CertificationHelper() = default;
    CertificationHelper(const CertificationHelper&) = delete;
    CertificationHelper& operator=(const CertificationHelper&) = delete;

    mutable std::mutex mutex_;
    bool enabled_ = false;
    std::string config_;
    std::map<std::string, Requirement> requirements_;
    std::vector<std::string> order_;
};

} // namespace nexus::utility::safety
