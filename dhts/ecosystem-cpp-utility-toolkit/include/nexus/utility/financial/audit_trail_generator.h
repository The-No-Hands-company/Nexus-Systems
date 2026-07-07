#pragma once

#include <chrono>
#include <mutex>
#include <string>
#include <vector>

namespace nexus::utility::financial {

struct AuditEntry {
    std::chrono::system_clock::time_point timestamp;
    std::string type;
    std::string description;
    double amount;

    AuditEntry(std::string t, std::string d, double a)
        : timestamp(std::chrono::system_clock::now())
        , type(std::move(t))
        , description(std::move(d))
        , amount(a) {}
};

class AuditTrailGenerator {
public:
    static AuditTrailGenerator& instance() {
        static AuditTrailGenerator inst;
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
    }

    bool isEnabled() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return enabled_;
    }

    void enable() {
        std::lock_guard<std::mutex> lock(mutex_);
        enabled_ = true;
    }

    void disable() {
        std::lock_guard<std::mutex> lock(mutex_);
        enabled_ = false;
    }

    std::string getStatus() const {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!enabled_) return "AuditTrailGenerator disabled";
        return "AuditTrailGenerator active, entries=" + std::to_string(entries_.size());
    }

    void reset() {
        std::lock_guard<std::mutex> lock(mutex_);
        entries_.clear();
    }

    void logEvent(const std::string& event_type, const std::string& description, double amount) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!enabled_) return;
        entries_.emplace_back(event_type, description, amount);
    }

    std::vector<AuditEntry> getEntries() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return entries_;
    }

    std::vector<AuditEntry> getEntriesByType(const std::string& type) const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<AuditEntry> result;
        for (const auto& e : entries_) {
            if (e.type == type) {
                result.push_back(e);
            }
        }
        return result;
    }

    double getTotalByType(const std::string& type) const {
        std::lock_guard<std::mutex> lock(mutex_);
        double total = 0.0;
        for (const auto& e : entries_) {
            if (e.type == type) {
                total += e.amount;
            }
        }
        return total;
    }

    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        entries_.clear();
    }

private:
    AuditTrailGenerator() = default;
    ~AuditTrailGenerator() = default;

    AuditTrailGenerator(const AuditTrailGenerator&) = delete;
    AuditTrailGenerator& operator=(const AuditTrailGenerator&) = delete;
    AuditTrailGenerator(AuditTrailGenerator&&) = delete;
    AuditTrailGenerator& operator=(AuditTrailGenerator&&) = delete;

    mutable std::mutex mutex_;
    bool enabled_ = false;
    std::string config_;
    std::vector<AuditEntry> entries_;
};

} // namespace nexus::utility::financial
