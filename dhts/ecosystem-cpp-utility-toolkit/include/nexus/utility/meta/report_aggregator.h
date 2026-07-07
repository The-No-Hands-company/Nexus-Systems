#pragma once

#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <cstddef>

namespace nexus::utility::meta {

/**
 * @brief Aggregate named textual reports into a single combined report.
 *
 * Components add named reports (string content). Reports can be retrieved
 * individually or merged into one document. Adding a report under an existing
 * name appends to that report.
 *
 * Thread safety: all public methods are protected by a mutex.
 *
 * @category Meta
 * @version 1.0.0
 */
class ReportAggregator {
public:
    /// @brief Get singleton instance
    static ReportAggregator& instance() {
        static ReportAggregator inst;
        return inst;
    }

    /// @brief Initialize the utility
    void initialize(const std::string& config = "") {
        std::lock_guard<std::mutex> lock(mutex_);
        config_ = config;
        enabled_ = true;
    }

    /// @brief Shutdown the utility and cleanup resources
    void shutdown() {
        std::lock_guard<std::mutex> lock(mutex_);
        enabled_ = false;
        reports_.clear();
        order_.clear();
    }

    /// @brief Check if the utility is currently enabled
    bool isEnabled() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return enabled_;
    }

    /// @brief Enable the utility
    void enable() {
        std::lock_guard<std::mutex> lock(mutex_);
        enabled_ = true;
    }

    /// @brief Disable the utility
    void disable() {
        std::lock_guard<std::mutex> lock(mutex_);
        enabled_ = false;
    }

    // ── Domain methods ──────────────────────────────────────────────────────

    /// @brief Add a report. If a report with this name already exists, the new
    /// content is appended with a newline separator.
    void addReport(const std::string& name, const std::string& content) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!enabled_) return;
        auto it = reports_.find(name);
        if (it == reports_.end()) {
            reports_.emplace(name, content);
            order_.push_back(name);
        } else {
            it->second += "\n" + content;
        }
    }

    /// @brief Get a single report by name (empty if absent).
    std::string getReport(const std::string& name) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = reports_.find(name);
        return it == reports_.end() ? std::string{} : it->second;
    }

    /// @brief Whether a report with the given name exists.
    bool hasReport(const std::string& name) const {
        std::lock_guard<std::mutex> lock(mutex_);
        return reports_.find(name) != reports_.end();
    }

    /// @brief Merge all reports (in insertion order) into a single document,
    /// each section prefixed with "=== <name> ===".
    std::string mergeReports() const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::string merged;
        for (const auto& name : order_) {
            auto it = reports_.find(name);
            if (it == reports_.end()) continue;
            merged += "=== " + name + " ===\n";
            merged += it->second;
            merged += "\n";
        }
        return merged;
    }

    /// @brief Number of distinct reports.
    std::size_t getReportCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return reports_.size();
    }

    /// @brief Get utility statistics/status
    std::string getStatus() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return "ReportAggregator[reports=" + std::to_string(reports_.size()) + "]";
    }

    /// @brief Reset utility state
    void reset() {
        std::lock_guard<std::mutex> lock(mutex_);
        reports_.clear();
        order_.clear();
    }

private:
    ReportAggregator() = default;
    ~ReportAggregator() = default;

    ReportAggregator(const ReportAggregator&) = delete;
    ReportAggregator& operator=(const ReportAggregator&) = delete;
    ReportAggregator(ReportAggregator&&) = delete;
    ReportAggregator& operator=(ReportAggregator&&) = delete;

    mutable std::mutex mutex_;
    bool enabled_ = false;
    std::string config_;
    std::map<std::string, std::string> reports_;
    std::vector<std::string> order_;
};

} // namespace nexus::utility::meta
