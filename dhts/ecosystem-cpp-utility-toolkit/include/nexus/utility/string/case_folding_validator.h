#pragma once

#include <string>
#include <cctype>
#include <mutex>

namespace nexus::utility::string {

/// @brief Validate simple (ASCII) case-folding operations.
class CaseFoldingValidator {
public:
    static CaseFoldingValidator& instance() {
        static CaseFoldingValidator inst;
        return inst;
    }

    void initialize() {
        std::lock_guard<std::mutex> lock(mutex_);
        enabled_ = true;
    }

    void shutdown() {
        std::lock_guard<std::mutex> lock(mutex_);
        enabled_ = false;
    }

    bool isEnabled() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return enabled_;
    }

    void setLocale(const std::string& locale) {
        std::lock_guard<std::mutex> lock(mutex_);
        locale_ = locale;
    }

    std::string getLocale() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return locale_;
    }

    /// Fold case to lowercase (simple ASCII folding).
    std::string foldCase(const std::string& text) const {
        std::string out;
        out.reserve(text.size());
        for (unsigned char c : text) {
            out.push_back(static_cast<char>(std::tolower(c)));
        }
        return out;
    }

    /// Returns true if `folded` is the correct case-fold of `original`.
    bool validateFold(const std::string& original, const std::string& folded) const {
        return foldCase(original) == folded;
    }

private:
    CaseFoldingValidator() = default;
    ~CaseFoldingValidator() = default;
    CaseFoldingValidator(const CaseFoldingValidator&) = delete;
    CaseFoldingValidator& operator=(const CaseFoldingValidator&) = delete;

    mutable std::mutex mutex_;
    bool enabled_ = false;
    std::string locale_ = "C";
};

} // namespace nexus::utility::string
