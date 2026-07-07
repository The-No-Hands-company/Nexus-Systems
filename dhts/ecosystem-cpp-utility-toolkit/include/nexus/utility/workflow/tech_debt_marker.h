#pragma once

#include <string>
#include <vector>
#include <atomic>
#include <mutex>
#include <cstdint>

namespace nexus::utility::workflow {

/**
 * @brief Register and track technical debt items.
 */
class TechDebtMarker {
public:
    enum class Severity { Low, Medium, High, Critical };

    struct DebtItem {
        std::uint64_t id = 0;
        std::string location;
        std::string description;
        Severity severity = Severity::Medium;
        int estimatedHours = 0;
        bool resolved = false;
    };

    static TechDebtMarker& instance() {
        static TechDebtMarker inst;
        return inst;
    }

    void initialize(const std::string& config = "") {
        std::lock_guard<std::mutex> lk(mutex_);
        config_ = config;
        enabled_ = true;
    }
    void shutdown() { enabled_ = false; }
    bool isEnabled() const { return enabled_.load(); }

    std::uint64_t mark(const std::string& location, const std::string& description,
                       Severity severity, int estimatedHours = 0) {
        std::lock_guard<std::mutex> lk(mutex_);
        std::uint64_t id = ++nextId_;
        items_.push_back({id, location, description, severity, estimatedHours, false});
        return id;
    }

    bool resolve(std::uint64_t id) {
        std::lock_guard<std::mutex> lk(mutex_);
        for (auto& it : items_)
            if (it.id == id) { it.resolved = true; return true; }
        return false;
    }

    /// Total estimated remediation effort for unresolved items.
    int totalOutstandingHours() const {
        std::lock_guard<std::mutex> lk(mutex_);
        int total = 0;
        for (const auto& it : items_) if (!it.resolved) total += it.estimatedHours;
        return total;
    }

    std::size_t countBySeverity(Severity severity) const {
        std::lock_guard<std::mutex> lk(mutex_);
        std::size_t n = 0;
        for (const auto& it : items_)
            if (!it.resolved && it.severity == severity) ++n;
        return n;
    }

    std::vector<DebtItem> outstanding() const {
        std::lock_guard<std::mutex> lk(mutex_);
        std::vector<DebtItem> out;
        for (const auto& it : items_) if (!it.resolved) out.push_back(it);
        return out;
    }

    void reset() {
        std::lock_guard<std::mutex> lk(mutex_);
        items_.clear();
        nextId_ = 0;
    }

private:
    TechDebtMarker() = default;
    ~TechDebtMarker() = default;
    TechDebtMarker(const TechDebtMarker&) = delete;
    TechDebtMarker& operator=(const TechDebtMarker&) = delete;

    mutable std::mutex mutex_;
    std::atomic<bool> enabled_{false};
    std::string config_;
    std::vector<DebtItem> items_;
    std::uint64_t nextId_ = 0;
};

} // namespace nexus::utility::workflow
