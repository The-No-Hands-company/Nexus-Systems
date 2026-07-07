#pragma once

#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <cstddef>

namespace nexus::utility::license {

/**
 * @brief Validate GPL copyleft boundaries across linked modules.
 *
 * Modules are registered with their license. Links express a dependency from one
 * module to another. GPL contamination occurs when a non-GPL module links
 * (statically) against a GPL module, because that would force the whole work
 * under the GPL. checkCompliance() flags every such link as a violation.
 *
 * Thread safety: all public methods are protected by a mutex.
 *
 * @category License
 * @version 1.0.0
 */
class GplBoundaryValidator {
public:
    struct Link {
        std::string from;
        std::string to;
    };

    /// @brief Get singleton instance
    static GplBoundaryValidator& instance() {
        static GplBoundaryValidator inst;
        return inst;
    }

    /// @brief Initialize the utility
    void initialize(const std::string& config = "") {
        std::lock_guard<std::mutex> lock(mutex_);
        config_ = config;
        enabled_ = true;
        modules_.clear();
        links_.clear();
        violations_.clear();
    }

    /// @brief Shutdown the utility and cleanup resources
    void shutdown() {
        std::lock_guard<std::mutex> lock(mutex_);
        enabled_ = false;
        modules_.clear();
        links_.clear();
        violations_.clear();
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

    /// @brief Register a module with its license (e.g. "GPL-3.0", "MIT").
    void addModule(const std::string& name, const std::string& license) {
        std::lock_guard<std::mutex> lock(mutex_);
        modules_[name] = license;
    }

    /// @brief Record a link/dependency from one module to another.
    void addLink(const std::string& from_module, const std::string& to_module) {
        std::lock_guard<std::mutex> lock(mutex_);
        links_.push_back({from_module, to_module});
    }

    /// @brief Analyse all links for GPL contamination.
    /// @return true if compliant (no violations detected).
    bool checkCompliance() {
        std::lock_guard<std::mutex> lock(mutex_);
        violations_.clear();
        for (const auto& link : links_) {
            auto from_it = modules_.find(link.from);
            auto to_it = modules_.find(link.to);
            if (to_it == modules_.end()) continue;
            bool to_gpl = isGpl(to_it->second);
            bool from_gpl = from_it != modules_.end() && isGpl(from_it->second);
            if (to_gpl && !from_gpl) {
                violations_.push_back(link.from + " (" +
                    (from_it != modules_.end() ? from_it->second : "unknown") +
                    ") links GPL module " + link.to + " (" + to_it->second + ")");
            }
        }
        return violations_.empty();
    }

    /// @brief Get the violations found by the last checkCompliance().
    std::vector<std::string> getViolations() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return violations_;
    }

    /// @brief Number of registered modules.
    std::size_t getModuleCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return modules_.size();
    }

    /// @brief Number of registered links.
    std::size_t getLinkCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return links_.size();
    }

    /// @brief Get utility statistics/status
    std::string getStatus() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return "GplBoundaryValidator[modules=" + std::to_string(modules_.size()) +
               ", links=" + std::to_string(links_.size()) +
               ", violations=" + std::to_string(violations_.size()) + "]";
    }

    /// @brief Reset utility state
    void reset() {
        std::lock_guard<std::mutex> lock(mutex_);
        modules_.clear();
        links_.clear();
        violations_.clear();
    }

private:
    GplBoundaryValidator() = default;
    ~GplBoundaryValidator() = default;

    GplBoundaryValidator(const GplBoundaryValidator&) = delete;
    GplBoundaryValidator& operator=(const GplBoundaryValidator&) = delete;
    GplBoundaryValidator(GplBoundaryValidator&&) = delete;
    GplBoundaryValidator& operator=(GplBoundaryValidator&&) = delete;

    // A license is treated as strong copyleft GPL if it mentions "GPL" but is
    // not the weaker LGPL/AGPL-exception variants (LGPL permits non-GPL linking).
    static bool isGpl(const std::string& license) {
        return contains(license, "GPL") && !contains(license, "LGPL");
    }

    static bool contains(const std::string& hay, const std::string& needle) {
        return hay.find(needle) != std::string::npos;
    }

    mutable std::mutex mutex_;
    bool enabled_ = false;
    std::string config_;
    std::map<std::string, std::string> modules_;
    std::vector<Link> links_;
    std::vector<std::string> violations_;
};

} // namespace nexus::utility::license
