#pragma once

#include <cstddef>
#include <map>
#include <mutex>
#include <string>
#include <vector>

namespace nexus::utility::safety {

/// @brief Registers safety hazards and tracks which are currently active.
class HazardMonitor {
public:
    struct Hazard {
        std::string id;
        std::string description;
        int severity = 0;
        bool active = false;
    };

    static HazardMonitor& instance() {
        static HazardMonitor inst;
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
        hazards_.clear();
    }

    bool isEnabled() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return enabled_;
    }

    void registerHazard(const std::string& id, const std::string& description, int severity) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto& h = hazards_[id];
        h.id = id;
        h.description = description;
        h.severity = severity;
    }

    void activateHazard(const std::string& id) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = hazards_.find(id);
        if (it != hazards_.end()) it->second.active = true;
    }

    void deactivateHazard(const std::string& id) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = hazards_.find(id);
        if (it != hazards_.end()) it->second.active = false;
    }

    std::vector<Hazard> getActiveHazards() const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<Hazard> out;
        for (const auto& [_, h] : hazards_) {
            if (h.active) out.push_back(h);
        }
        return out;
    }

    size_t getActiveCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        size_t n = 0;
        for (const auto& [_, h] : hazards_) {
            if (h.active) ++n;
        }
        return n;
    }

    void reset() {
        std::lock_guard<std::mutex> lock(mutex_);
        hazards_.clear();
    }

private:
    HazardMonitor() = default;
    ~HazardMonitor() = default;
    HazardMonitor(const HazardMonitor&) = delete;
    HazardMonitor& operator=(const HazardMonitor&) = delete;

    mutable std::mutex mutex_;
    bool enabled_ = false;
    std::string config_;
    std::map<std::string, Hazard> hazards_;
};

} // namespace nexus::utility::safety
