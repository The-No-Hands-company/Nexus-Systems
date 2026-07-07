#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <atomic>
#include <mutex>

namespace nexus::utility::accessibility {

/**
 * @brief Validate ARIA attributes against a set of known valid roles/attributes.
 */
class AriaAttributeValidator {
public:
    struct Issue {
        std::string elementId;
        std::string attribute;
        std::string reason;
    };

    static AriaAttributeValidator& instance() {
        static AriaAttributeValidator inst;
        return inst;
    }

    void initialize(const std::string& config = "") {
        std::lock_guard<std::mutex> lk(mutex_);
        loadKnownAria();
        config_ = config;
        enabled_ = true;
    }
    void shutdown() { enabled_ = false; }
    bool isEnabled() const { return enabled_.load(); }

    bool isValidRole(const std::string& role) const {
        std::lock_guard<std::mutex> lk(mutex_);
        return validRoles_.count(role) > 0;
    }

    bool isValidAttribute(const std::string& attr) const {
        std::lock_guard<std::mutex> lk(mutex_);
        return validAttributes_.count(attr) > 0;
    }

    /// Validate an element's role and aria-* attributes; records any issues.
    bool validate(const std::string& elementId, const std::string& role,
                  const std::vector<std::string>& ariaAttributes) {
        std::lock_guard<std::mutex> lk(mutex_);
        bool ok = true;
        if (!role.empty() && !validRoles_.count(role)) {
            issues_.push_back({elementId, "role", "unknown role: " + role});
            ok = false;
        }
        for (const auto& a : ariaAttributes) {
            if (!validAttributes_.count(a)) {
                issues_.push_back({elementId, a, "unknown aria attribute"});
                ok = false;
            }
        }
        if (!ok) ++invalidElements_;
        return ok;
    }

    std::vector<Issue> issues() const {
        std::lock_guard<std::mutex> lk(mutex_);
        return issues_;
    }

    std::size_t invalidElements() const { return invalidElements_.load(); }

    void reset() {
        std::lock_guard<std::mutex> lk(mutex_);
        issues_.clear();
        invalidElements_ = 0;
    }

private:
    AriaAttributeValidator() { loadKnownAria(); }
    ~AriaAttributeValidator() = default;
    AriaAttributeValidator(const AriaAttributeValidator&) = delete;
    AriaAttributeValidator& operator=(const AriaAttributeValidator&) = delete;

    void loadKnownAria() {
        validRoles_ = {"button", "checkbox", "dialog", "link", "menu", "menuitem",
                       "navigation", "tab", "tabpanel", "textbox", "alert", "banner",
                       "main", "region", "list", "listitem", "heading", "img"};
        validAttributes_ = {"aria-label", "aria-labelledby", "aria-describedby",
                            "aria-hidden", "aria-live", "aria-expanded", "aria-checked",
                            "aria-disabled", "aria-selected", "aria-required",
                            "aria-controls", "aria-haspopup", "aria-current"};
    }

    mutable std::mutex mutex_;
    std::atomic<bool> enabled_{false};
    std::string config_;
    std::unordered_set<std::string> validRoles_;
    std::unordered_set<std::string> validAttributes_;
    std::vector<Issue> issues_;
    std::atomic<std::size_t> invalidElements_{0};
};

} // namespace nexus::utility::accessibility
