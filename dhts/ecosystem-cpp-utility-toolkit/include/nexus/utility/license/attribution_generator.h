#pragma once

#include <string>
#include <vector>
#include <mutex>
#include <cstddef>

namespace nexus::utility::license {

/**
 * @brief Generate third-party attribution / NOTICE text from tracked
 * dependencies.
 *
 * Each dependency records its name, version, license and project URL. The
 * generated attribution lists every dependency in a human-readable NOTICE-style
 * format suitable for distribution.
 *
 * Thread safety: all public methods are protected by a mutex.
 *
 * @category License
 * @version 1.0.0
 */
class AttributionGenerator {
public:
    struct Dependency {
        std::string name;
        std::string version;
        std::string license;
        std::string url;
    };

    /// @brief Get singleton instance
    static AttributionGenerator& instance() {
        static AttributionGenerator inst;
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

    /// @brief Add a dependency to attribute.
    void addDependency(const std::string& name, const std::string& version,
                       const std::string& license, const std::string& url) {
        std::lock_guard<std::mutex> lock(mutex_);
        deps_.push_back({name, version, license, url});
    }

    /// @brief Generate the full attribution / NOTICE text.
    std::string generateAttribution() const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::string out = "THIRD-PARTY SOFTWARE NOTICES AND INFORMATION\n";
        out += "============================================\n\n";
        for (const auto& d : deps_) {
            out += d.name + " " + d.version + "\n";
            out += "  License: " + d.license + "\n";
            out += "  URL:     " + d.url + "\n\n";
        }
        return out;
    }

    /// @brief Number of dependencies recorded.
    std::size_t getDependencyCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return deps_.size();
    }

    /// @brief Get utility statistics/status
    std::string getStatus() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return "AttributionGenerator[dependencies=" + std::to_string(deps_.size()) + "]";
    }

    /// @brief Reset utility state
    void reset() {
        std::lock_guard<std::mutex> lock(mutex_);
        deps_.clear();
    }

private:
    AttributionGenerator() = default;
    ~AttributionGenerator() = default;

    AttributionGenerator(const AttributionGenerator&) = delete;
    AttributionGenerator& operator=(const AttributionGenerator&) = delete;
    AttributionGenerator(AttributionGenerator&&) = delete;
    AttributionGenerator& operator=(AttributionGenerator&&) = delete;

    mutable std::mutex mutex_;
    bool enabled_ = false;
    std::string config_;
    std::vector<Dependency> deps_;
};

} // namespace nexus::utility::license
