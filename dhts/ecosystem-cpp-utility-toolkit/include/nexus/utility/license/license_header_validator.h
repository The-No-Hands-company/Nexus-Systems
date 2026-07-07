#pragma once

#include <string>
#include <vector>
#include <mutex>
#include <cstddef>

namespace nexus::utility::license {

/**
 * @brief Validate that source files contain an expected license header.
 *
 * An expected header pattern is configured once. validate() checks whether a
 * file's content contains that pattern; files that do not are recorded so they
 * can be reported later.
 *
 * Thread safety: all public methods are protected by a mutex.
 *
 * @category License
 * @version 1.0.0
 */
class LicenseHeaderValidator {
public:
    /// @brief Get singleton instance
    static LicenseHeaderValidator& instance() {
        static LicenseHeaderValidator inst;
        return inst;
    }

    /// @brief Initialize the utility
    void initialize(const std::string& config = "") {
        std::lock_guard<std::mutex> lock(mutex_);
        config_ = config;
        enabled_ = true;
        expected_.clear();
        invalid_files_.clear();
        checked_ = 0;
    }

    /// @brief Shutdown the utility and cleanup resources
    void shutdown() {
        std::lock_guard<std::mutex> lock(mutex_);
        enabled_ = false;
        invalid_files_.clear();
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

    /// @brief Set the expected license header pattern (substring to require).
    void setExpectedHeader(const std::string& header_pattern) {
        std::lock_guard<std::mutex> lock(mutex_);
        expected_ = header_pattern;
    }

    /// @brief Validate a file's content against the expected header.
    /// @param file_content The full text of the file to validate.
    /// @param filename Optional identifier recorded for invalid files.
    /// @return true if the header is present.
    bool validate(const std::string& file_content, const std::string& filename = "") {
        std::lock_guard<std::mutex> lock(mutex_);
        ++checked_;
        bool ok = !expected_.empty() &&
                  file_content.find(expected_) != std::string::npos;
        if (!ok) {
            invalid_files_.push_back(filename.empty() ? "<unnamed>" : filename);
        }
        return ok;
    }

    /// @brief Get the list of files that failed validation.
    std::vector<std::string> getInvalidFiles() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return invalid_files_;
    }

    /// @brief Number of files that failed validation.
    std::size_t getInvalidCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return invalid_files_.size();
    }

    /// @brief Number of files checked.
    std::size_t getCheckedCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return checked_;
    }

    /// @brief Get utility statistics/status
    std::string getStatus() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return "LicenseHeaderValidator[checked=" + std::to_string(checked_) +
               ", invalid=" + std::to_string(invalid_files_.size()) + "]";
    }

    /// @brief Reset utility state
    void reset() {
        std::lock_guard<std::mutex> lock(mutex_);
        invalid_files_.clear();
        checked_ = 0;
    }

private:
    LicenseHeaderValidator() = default;
    ~LicenseHeaderValidator() = default;

    LicenseHeaderValidator(const LicenseHeaderValidator&) = delete;
    LicenseHeaderValidator& operator=(const LicenseHeaderValidator&) = delete;
    LicenseHeaderValidator(LicenseHeaderValidator&&) = delete;
    LicenseHeaderValidator& operator=(LicenseHeaderValidator&&) = delete;

    mutable std::mutex mutex_;
    bool enabled_ = false;
    std::string config_;
    std::string expected_;
    std::vector<std::string> invalid_files_;
    std::size_t checked_ = 0;
};

} // namespace nexus::utility::license
