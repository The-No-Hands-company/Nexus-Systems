#pragma once

#include <string>
#include <vector>
#include <mutex>
#include <cstddef>

namespace nexus::utility::license {

/**
 * @brief Track third-party dependencies and query them by license.
 *
 * Records each dependency's name, version and license, and supports listing all
 * dependencies or filtering by license identifier.
 *
 * Thread safety: all public methods are protected by a mutex.
 *
 * @category License
 * @version 1.0.0
 */
class ThirdPartyTracker {
public:
    struct Dependency {
        std::string name;
        std::string version;
        std::string license;
    };

    /// @brief Get singleton instance
    static ThirdPartyTracker& instance() {
        static ThirdPartyTracker inst;
        return inst;
    }

    /// @brief Initialize the utility
    void initialize(const std::string& config = "") {
        std::lock_guard<std::mutex> lock(mutex_);
        config_ = config;
        enabled_ = true;
        deps_.clear();
    }

    /// @brief Shutdown the utility and cleanup resources
    void shutdown() {
        std::lock_guard<std::mutex> lock(mutex_);
        enabled_ = false;
        deps_.clear();
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

    /// @brief Add a third-party dependency.
    void addDependency(const std::string& name, const std::string& version,
                       const std::string& license) {
        std::lock_guard<std::mutex> lock(mutex_);
        deps_.push_back({name, version, license});
    }

    /// @brief Get all tracked dependencies.
    std::vector<Dependency> getDependencies() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return deps_;
    }

    /// @brief Get all dependencies using the given license.
    std::vector<Dependency> getByLicense(const std::string& license) const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<Dependency> out;
        for (const auto& d : deps_) {
            if (d.license == license) out.push_back(d);
        }
        return out;
    }

    /// @brief Number of tracked dependencies.
    std::size_t getDependencyCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return deps_.size();
    }

    /// @brief Get utility statistics/status
    std::string getStatus() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return "ThirdPartyTracker[dependencies=" + std::to_string(deps_.size()) + "]";
    }

    /// @brief Reset utility state
    void reset() {
        std::lock_guard<std::mutex> lock(mutex_);
        deps_.clear();
    }

private:
    ThirdPartyTracker() = default;
    ~ThirdPartyTracker() = default;

    ThirdPartyTracker(const ThirdPartyTracker&) = delete;
    ThirdPartyTracker& operator=(const ThirdPartyTracker&) = delete;
    ThirdPartyTracker(ThirdPartyTracker&&) = delete;
    ThirdPartyTracker& operator=(ThirdPartyTracker&&) = delete;

    mutable std::mutex mutex_;
    bool enabled_ = false;
    std::string config_;
    std::vector<Dependency> deps_;
};

} // namespace nexus::utility::license
