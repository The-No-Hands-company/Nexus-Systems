#pragma once

#include <string>
#include <vector>
#include <unordered_set>
#include <atomic>
#include <mutex>

namespace nexus::utility::accessibility {

/**
 * @brief Validate keyboard navigability of interactive elements.
 */
class KeyboardNavigationValidator {
public:
    struct InteractiveElement {
        std::string id;
        std::string type;
        bool focusable = true;
        bool hasKeyHandler = false;
        bool hasVisibleFocusIndicator = true;
    };

    static KeyboardNavigationValidator& instance() {
        static KeyboardNavigationValidator inst;
        return inst;
    }

    void initialize(const std::string& config = "") {
        std::lock_guard<std::mutex> lk(mutex_);
        config_ = config;
        enabled_ = true;
    }
    void shutdown() { enabled_ = false; }
    bool isEnabled() const { return enabled_.load(); }

    void addElement(const InteractiveElement& e) {
        std::lock_guard<std::mutex> lk(mutex_);
        elements_.push_back(e);
    }

    /// An interactive element must be focusable and expose keyboard handling.
    bool isKeyboardAccessible(const InteractiveElement& e) const {
        return e.focusable && e.hasKeyHandler && e.hasVisibleFocusIndicator;
    }

    /// Elements that cannot be operated by keyboard.
    std::vector<InteractiveElement> inaccessibleElements() const {
        std::lock_guard<std::mutex> lk(mutex_);
        std::vector<InteractiveElement> out;
        for (const auto& e : elements_)
            if (!isKeyboardAccessible(e)) out.push_back(e);
        return out;
    }

    bool allAccessible() const {
        std::lock_guard<std::mutex> lk(mutex_);
        for (const auto& e : elements_)
            if (!isKeyboardAccessible(e)) return false;
        return true;
    }

    std::size_t elementCount() const {
        std::lock_guard<std::mutex> lk(mutex_);
        return elements_.size();
    }

    void reset() {
        std::lock_guard<std::mutex> lk(mutex_);
        elements_.clear();
    }

private:
    KeyboardNavigationValidator() = default;
    ~KeyboardNavigationValidator() = default;
    KeyboardNavigationValidator(const KeyboardNavigationValidator&) = delete;
    KeyboardNavigationValidator& operator=(const KeyboardNavigationValidator&) = delete;

    mutable std::mutex mutex_;
    std::atomic<bool> enabled_{false};
    std::string config_;
    std::vector<InteractiveElement> elements_;
};

} // namespace nexus::utility::accessibility
