#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <atomic>
#include <mutex>

namespace nexus::utility::specialized {

/**
 * @brief Track medical device compliance requirements (IEC 62304 / FDA).
 */
class MedicalDeviceCompliance {
public:
    enum class SafetyClass { A, B, C };   // IEC 62304 software safety classes
    enum class Status { Open, InReview, Verified, Failed };

    struct Requirement {
        std::string id;
        std::string description;
        SafetyClass safetyClass = SafetyClass::A;
        Status status = Status::Open;
        std::string verificationMethod;
    };

    static MedicalDeviceCompliance& instance() {
        static MedicalDeviceCompliance inst;
        return inst;
    }

    void initialize(const std::string& config = "") {
        std::lock_guard<std::mutex> lk(mutex_);
        config_ = config;
        enabled_ = true;
    }
    void shutdown() { enabled_ = false; }
    bool isEnabled() const { return enabled_.load(); }

    void addRequirement(const std::string& id, const std::string& description,
                        SafetyClass cls) {
        std::lock_guard<std::mutex> lk(mutex_);
        requirements_[id] = {id, description, cls, Status::Open, ""};
    }

    void setStatus(const std::string& id, Status status,
                   const std::string& verificationMethod = "") {
        std::lock_guard<std::mutex> lk(mutex_);
        auto it = requirements_.find(id);
        if (it != requirements_.end()) {
            it->second.status = status;
            if (!verificationMethod.empty())
                it->second.verificationMethod = verificationMethod;
        }
    }

    bool isFullyCompliant() const {
        std::lock_guard<std::mutex> lk(mutex_);
        for (const auto& [id, r] : requirements_)
            if (r.status != Status::Verified) return false;
        return !requirements_.empty();
    }

    double compliancePercentage() const {
        std::lock_guard<std::mutex> lk(mutex_);
        if (requirements_.empty()) return 100.0;
        std::size_t verified = 0;
        for (const auto& [id, r] : requirements_)
            if (r.status == Status::Verified) ++verified;
        return 100.0 * verified / requirements_.size();
    }

    std::vector<Requirement> failing() const {
        std::lock_guard<std::mutex> lk(mutex_);
        std::vector<Requirement> out;
        for (const auto& [id, r] : requirements_)
            if (r.status == Status::Failed) out.push_back(r);
        return out;
    }

    std::size_t countByClass(SafetyClass cls) const {
        std::lock_guard<std::mutex> lk(mutex_);
        std::size_t n = 0;
        for (const auto& [id, r] : requirements_)
            if (r.safetyClass == cls) ++n;
        return n;
    }

    void reset() {
        std::lock_guard<std::mutex> lk(mutex_);
        requirements_.clear();
    }

private:
    MedicalDeviceCompliance() = default;
    ~MedicalDeviceCompliance() = default;
    MedicalDeviceCompliance(const MedicalDeviceCompliance&) = delete;
    MedicalDeviceCompliance& operator=(const MedicalDeviceCompliance&) = delete;

    mutable std::mutex mutex_;
    std::atomic<bool> enabled_{false};
    std::string config_;
    std::unordered_map<std::string, Requirement> requirements_;
};

} // namespace nexus::utility::specialized
