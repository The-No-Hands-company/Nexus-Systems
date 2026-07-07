#pragma once

#include <string>
#include <vector>
#include <unordered_set>
#include <algorithm>
#include <atomic>
#include <mutex>

namespace nexus::utility::accessibility {

/**
 * @brief Validate focus (tab) order of interactive elements.
 */
class FocusOrderValidator {
public:
    struct FocusableElement {
        std::string id;
        int tabIndex = 0;
        int domOrder = 0;
        bool visible = true;
    };

    static FocusOrderValidator& instance() {
        static FocusOrderValidator inst;
        return inst;
    }

    void initialize(const std::string& config = "") {
        std::lock_guard<std::mutex> lk(mutex_);
        config_ = config;
        enabled_ = true;
    }
    void shutdown() { enabled_ = false; }
    bool isEnabled() const { return enabled_.load(); }

    void addElement(const std::string& id, int tabIndex, int domOrder, bool visible = true) {
        std::lock_guard<std::mutex> lk(mutex_);
        elements_.push_back({id, tabIndex, domOrder, visible});
    }

    /// Positive tabindex values are an anti-pattern (they disrupt natural order).
    std::vector<FocusableElement> positiveTabIndexElements() const {
        std::lock_guard<std::mutex> lk(mutex_);
        std::vector<FocusableElement> out;
        for (const auto& e : elements_) if (e.tabIndex > 0) out.push_back(e);
        return out;
    }

    /// Check whether focus order (by tabindex then DOM order) follows DOM order.
    bool isLogicalOrder() const {
        std::lock_guard<std::mutex> lk(mutex_);
        std::vector<FocusableElement> visible;
        for (const auto& e : elements_) if (e.visible) visible.push_back(e);
        std::stable_sort(visible.begin(), visible.end(),
            [](const FocusableElement& a, const FocusableElement& b) {
                int ka = a.tabIndex > 0 ? a.tabIndex : 1000000;
                int kb = b.tabIndex > 0 ? b.tabIndex : 1000000;
                if (ka != kb) return ka < kb;
                return a.domOrder < b.domOrder;
            });
        for (std::size_t i = 1; i < visible.size(); ++i)
            if (visible[i].domOrder < visible[i - 1].domOrder) return false;
        return true;
    }

    void reset() {
        std::lock_guard<std::mutex> lk(mutex_);
        elements_.clear();
    }

private:
    FocusOrderValidator() = default;
    ~FocusOrderValidator() = default;
    FocusOrderValidator(const FocusOrderValidator&) = delete;
    FocusOrderValidator& operator=(const FocusOrderValidator&) = delete;

    mutable std::mutex mutex_;
    std::atomic<bool> enabled_{false};
    std::string config_;
    std::vector<FocusableElement> elements_;
};

} // namespace nexus::utility::accessibility
