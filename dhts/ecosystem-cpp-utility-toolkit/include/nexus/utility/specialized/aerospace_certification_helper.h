#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <atomic>
#include <mutex>

namespace nexus::utility::specialized {

/**
 * @brief Track aerospace software certification (DO-178C) objectives and evidence.
 */
class AerospaceCertificationHelper {
public:
    enum class DAL { A, B, C, D, E };   // Design Assurance Levels
    enum class Status { NotStarted, InProgress, Satisfied, NotApplicable };

    struct Objective {
        std::string id;
        std::string description;
        DAL applicableDown = DAL::D;   // applicable for levels A..applicableDown
        Status status = Status::NotStarted;
        std::vector<std::string> evidence;
    };

    static AerospaceCertificationHelper& instance() {
        static AerospaceCertificationHelper inst;
        return inst;
    }

    void initialize(const std::string& config = "") {
        std::lock_guard<std::mutex> lk(mutex_);
        config_ = config;
        enabled_ = true;
    }
    void shutdown() { enabled_ = false; }
    bool isEnabled() const { return enabled_.load(); }

    void setLevel(DAL level) {
        std::lock_guard<std::mutex> lk(mutex_);
        level_ = level;
    }

    void addObjective(const std::string& id, const std::string& description, DAL applicableDown) {
        std::lock_guard<std::mutex> lk(mutex_);
        objectives_[id] = {id, description, applicableDown, Status::NotStarted, {}};
    }

    void setStatus(const std::string& id, Status status) {
        std::lock_guard<std::mutex> lk(mutex_);
        auto it = objectives_.find(id);
        if (it != objectives_.end()) it->second.status = status;
    }

    void addEvidence(const std::string& id, const std::string& artifact) {
        std::lock_guard<std::mutex> lk(mutex_);
        auto it = objectives_.find(id);
        if (it != objectives_.end()) it->second.evidence.push_back(artifact);
    }

    bool isObjectiveApplicable(const Objective& o) const {
        return static_cast<int>(level_) <= static_cast<int>(o.applicableDown);
    }

    /// Fraction of applicable objectives that are satisfied, in [0,1].
    double compliancePercentage() const {
        std::lock_guard<std::mutex> lk(mutex_);
        std::size_t applicable = 0, satisfied = 0;
        for (const auto& [id, o] : objectives_) {
            if (!isObjectiveApplicable(o) || o.status == Status::NotApplicable) continue;
            ++applicable;
            if (o.status == Status::Satisfied) ++satisfied;
        }
        return applicable == 0 ? 1.0 : static_cast<double>(satisfied) / applicable;
    }

    std::vector<Objective> openObjectives() const {
        std::lock_guard<std::mutex> lk(mutex_);
        std::vector<Objective> out;
        for (const auto& [id, o] : objectives_)
            if (isObjectiveApplicable(o) && o.status != Status::Satisfied &&
                o.status != Status::NotApplicable)
                out.push_back(o);
        return out;
    }

    void reset() {
        std::lock_guard<std::mutex> lk(mutex_);
        objectives_.clear();
    }

private:
    AerospaceCertificationHelper() = default;
    ~AerospaceCertificationHelper() = default;
    AerospaceCertificationHelper(const AerospaceCertificationHelper&) = delete;
    AerospaceCertificationHelper& operator=(const AerospaceCertificationHelper&) = delete;

    mutable std::mutex mutex_;
    std::atomic<bool> enabled_{false};
    std::string config_;
    DAL level_ = DAL::C;
    std::unordered_map<std::string, Objective> objectives_;
};

} // namespace nexus::utility::specialized
