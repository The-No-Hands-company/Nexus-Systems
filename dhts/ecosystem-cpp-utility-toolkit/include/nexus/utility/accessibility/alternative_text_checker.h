#pragma once

#include <string>
#include <vector>
#include <atomic>
#include <mutex>

namespace nexus::utility::accessibility {

/**
 * @brief Check that images/media elements have alternative text.
 */
class AlternativeTextChecker {
public:
    struct Element {
        std::string id;
        std::string type;      // "img", "video", "svg", ...
        std::string altText;
        bool decorative = false;
        bool hasAlt() const { return decorative || !altText.empty(); }
    };

    static AlternativeTextChecker& instance() {
        static AlternativeTextChecker inst;
        return inst;
    }

    void initialize(const std::string& config = "") {
        std::lock_guard<std::mutex> lk(mutex_);
        config_ = config;
        enabled_ = true;
    }
    void shutdown() { enabled_ = false; }
    bool isEnabled() const { return enabled_.load(); }

    /// Register an element; returns true if it passes the alt-text requirement.
    bool checkElement(const std::string& id, const std::string& type,
                      const std::string& altText, bool decorative = false) {
        Element e{id, type, altText, decorative};
        std::lock_guard<std::mutex> lk(mutex_);
        elements_.push_back(e);
        if (!e.hasAlt()) ++missing_;
        return e.hasAlt();
    }

    std::vector<Element> missingAltText() const {
        std::lock_guard<std::mutex> lk(mutex_);
        std::vector<Element> out;
        for (const auto& e : elements_) if (!e.hasAlt()) out.push_back(e);
        return out;
    }

    std::size_t missingCount() const { return missing_.load(); }

    void reset() {
        std::lock_guard<std::mutex> lk(mutex_);
        elements_.clear();
        missing_ = 0;
    }

private:
    AlternativeTextChecker() = default;
    ~AlternativeTextChecker() = default;
    AlternativeTextChecker(const AlternativeTextChecker&) = delete;
    AlternativeTextChecker& operator=(const AlternativeTextChecker&) = delete;

    mutable std::mutex mutex_;
    std::atomic<bool> enabled_{false};
    std::string config_;
    std::vector<Element> elements_;
    std::atomic<std::size_t> missing_{0};
};

} // namespace nexus::utility::accessibility
