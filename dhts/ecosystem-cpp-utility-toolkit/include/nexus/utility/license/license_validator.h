#pragma once

#include <string>
#include <vector>
#include <map>
#include <set>
#include <algorithm>
#include <mutex>
#include <cstddef>

namespace nexus::utility::license {

/**
 * @brief Validate software licenses and check pairwise compatibility.
 *
 * Licenses are registered with their terms text. validateLicense() confirms a
 * license is known and has non-empty terms. isCompatible() decides whether two
 * licenses can be combined, using a built-in model of permissive vs. copyleft
 * licenses plus any explicitly registered compatibility rules.
 *
 * Thread safety: all public methods are protected by a mutex.
 *
 * @category License
 * @version 1.0.0
 */
class LicenseValidator {
public:
    /// @brief Get singleton instance
    static LicenseValidator& instance() {
        static LicenseValidator inst;
        return inst;
    }

    /// @brief Initialize the utility
    void initialize(const std::string& config = "") {
        std::lock_guard<std::mutex> lock(mutex_);
        config_ = config;
        enabled_ = true;
        licenses_.clear();
        compatible_pairs_.clear();
    }

    /// @brief Shutdown the utility and cleanup resources
    void shutdown() {
        std::lock_guard<std::mutex> lock(mutex_);
        enabled_ = false;
        licenses_.clear();
        compatible_pairs_.clear();
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

    /// @brief Register a license with its terms text.
    void addLicense(const std::string& name, const std::string& terms) {
        std::lock_guard<std::mutex> lock(mutex_);
        licenses_[name] = terms;
    }

    /// @brief Explicitly register that two licenses are compatible.
    void addCompatibility(const std::string& a, const std::string& b) {
        std::lock_guard<std::mutex> lock(mutex_);
        compatible_pairs_.insert(key(a, b));
    }

    /// @brief Validate a license: it must be registered and have non-empty terms.
    bool validateLicense(const std::string& name) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = licenses_.find(name);
        return it != licenses_.end() && !it->second.empty();
    }

    /// @brief Determine whether two licenses are compatible.
    /// A license is compatible with itself; a permissive license (MIT/BSD/Apache
    /// /ISC/Zlib) is compatible with anything; otherwise an explicit rule is
    /// required.
    bool isCompatible(const std::string& a, const std::string& b) const {
        std::lock_guard<std::mutex> lock(mutex_);
        if (a == b) return true;
        if (isPermissive(a) || isPermissive(b)) return true;
        return compatible_pairs_.count(key(a, b)) > 0;
    }

    /// @brief Whether a license has been registered.
    bool hasLicense(const std::string& name) const {
        std::lock_guard<std::mutex> lock(mutex_);
        return licenses_.find(name) != licenses_.end();
    }

    /// @brief Number of registered licenses.
    std::size_t getLicenseCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return licenses_.size();
    }

    /// @brief Get utility statistics/status
    std::string getStatus() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return "LicenseValidator[licenses=" + std::to_string(licenses_.size()) +
               ", rules=" + std::to_string(compatible_pairs_.size()) + "]";
    }

    /// @brief Reset utility state
    void reset() {
        std::lock_guard<std::mutex> lock(mutex_);
        licenses_.clear();
        compatible_pairs_.clear();
    }

private:
    LicenseValidator() = default;
    ~LicenseValidator() = default;

    LicenseValidator(const LicenseValidator&) = delete;
    LicenseValidator& operator=(const LicenseValidator&) = delete;
    LicenseValidator(LicenseValidator&&) = delete;
    LicenseValidator& operator=(LicenseValidator&&) = delete;

    static bool isPermissive(const std::string& name) {
        static const char* kPermissive[] = {"MIT", "BSD", "Apache", "ISC", "Zlib"};
        for (const char* p : kPermissive) {
            if (name.find(p) != std::string::npos) return true;
        }
        return false;
    }

    static std::string key(const std::string& a, const std::string& b) {
        return a < b ? a + "|" + b : b + "|" + a;
    }

    mutable std::mutex mutex_;
    bool enabled_ = false;
    std::string config_;
    std::map<std::string, std::string> licenses_;
    std::set<std::string> compatible_pairs_;
};

} // namespace nexus::utility::license
