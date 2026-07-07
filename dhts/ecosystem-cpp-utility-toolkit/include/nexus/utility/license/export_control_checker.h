#pragma once

#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <mutex>
#include <cstddef>

namespace nexus::utility::license {

/**
 * @brief Export-control checker for restricted technologies.
 *
 * A restricted technology is registered together with a list of restricted
 * destinations / jurisdictions (its "regulations"). checkExport() returns
 * whether exporting a technology to a destination is permitted: an export is
 * blocked if the destination appears in the technology's restricted list.
 *
 * Thread safety: all public methods are protected by a mutex.
 *
 * @category License
 * @version 1.0.0
 */
class ExportControlChecker {
public:
    /// @brief Get singleton instance
    static ExportControlChecker& instance() {
        static ExportControlChecker inst;
        return inst;
    }

    /// @brief Initialize the utility
    void initialize(const std::string& config = "") {
        std::lock_guard<std::mutex> lock(mutex_);
        config_ = config;
        enabled_ = true;
        restrictions_.clear();
        blocked_count_ = 0;
        allowed_count_ = 0;
    }

    /// @brief Shutdown the utility and cleanup resources
    void shutdown() {
        std::lock_guard<std::mutex> lock(mutex_);
        enabled_ = false;
        restrictions_.clear();
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

    /// @brief Register a restricted technology with its restricted destinations
    /// / regulation jurisdictions.
    void addRestrictedTechnology(const std::string& name,
                                 const std::vector<std::string>& regulations) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto& r = restrictions_[name];
        r.insert(r.end(), regulations.begin(), regulations.end());
    }

    /// @brief Check whether exporting `technology` to `destination` is permitted.
    /// @return true if the export is allowed, false if it is blocked.
    bool checkExport(const std::string& technology, const std::string& destination) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = restrictions_.find(technology);
        bool blocked = it != restrictions_.end() &&
                       std::find(it->second.begin(), it->second.end(), destination) !=
                           it->second.end();
        if (blocked) {
            ++blocked_count_;
        } else {
            ++allowed_count_;
        }
        return !blocked;
    }

    /// @brief Get the restricted destinations / regulations for a technology.
    std::vector<std::string> getRestrictions(const std::string& technology) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = restrictions_.find(technology);
        return it == restrictions_.end() ? std::vector<std::string>{} : it->second;
    }

    /// @brief Whether a technology has any registered restrictions.
    bool isRestricted(const std::string& technology) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = restrictions_.find(technology);
        return it != restrictions_.end() && !it->second.empty();
    }

    /// @brief Number of restricted technologies.
    std::size_t getTechnologyCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return restrictions_.size();
    }

    /// @brief Number of export checks that were blocked.
    std::size_t getBlockedCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return blocked_count_;
    }

    /// @brief Get utility statistics/status
    std::string getStatus() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return "ExportControlChecker[technologies=" + std::to_string(restrictions_.size()) +
               ", blocked=" + std::to_string(blocked_count_) +
               ", allowed=" + std::to_string(allowed_count_) + "]";
    }

    /// @brief Reset utility state
    void reset() {
        std::lock_guard<std::mutex> lock(mutex_);
        restrictions_.clear();
        blocked_count_ = 0;
        allowed_count_ = 0;
    }

private:
    ExportControlChecker() = default;
    ~ExportControlChecker() = default;

    ExportControlChecker(const ExportControlChecker&) = delete;
    ExportControlChecker& operator=(const ExportControlChecker&) = delete;
    ExportControlChecker(ExportControlChecker&&) = delete;
    ExportControlChecker& operator=(ExportControlChecker&&) = delete;

    mutable std::mutex mutex_;
    bool enabled_ = false;
    std::string config_;
    std::map<std::string, std::vector<std::string>> restrictions_;
    std::size_t blocked_count_ = 0;
    std::size_t allowed_count_ = 0;
};

} // namespace nexus::utility::license
