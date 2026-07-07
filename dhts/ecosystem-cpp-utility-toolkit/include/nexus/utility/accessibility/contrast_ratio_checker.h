#pragma once

#include <string>
#include <vector>
#include <cmath>
#include <algorithm>
#include <atomic>
#include <mutex>
#include <cstdint>

namespace nexus::utility::accessibility {

/**
 * @brief Compute WCAG color-contrast ratios and check compliance.
 */
class ContrastRatioChecker {
public:
    struct Color { std::uint8_t r = 0, g = 0, b = 0; };

    struct Result {
        double ratio = 1.0;
        bool passAA = false;      // >= 4.5 (normal) / 3.0 (large)
        bool passAAA = false;     // >= 7.0 (normal) / 4.5 (large)
    };

    static ContrastRatioChecker& instance() {
        static ContrastRatioChecker inst;
        return inst;
    }

    void initialize(const std::string& config = "") {
        std::lock_guard<std::mutex> lk(mutex_);
        config_ = config;
        enabled_ = true;
    }
    void shutdown() { enabled_ = false; }
    bool isEnabled() const { return enabled_.load(); }

    /// Relative luminance per WCAG 2.x.
    static double luminance(const Color& c) {
        auto chan = [](std::uint8_t v) {
            double s = v / 255.0;
            return s <= 0.03928 ? s / 12.92 : std::pow((s + 0.055) / 1.055, 2.4);
        };
        return 0.2126 * chan(c.r) + 0.7152 * chan(c.g) + 0.0722 * chan(c.b);
    }

    static double contrastRatio(const Color& a, const Color& b) {
        double la = luminance(a), lb = luminance(b);
        double lighter = std::max(la, lb), darker = std::min(la, lb);
        return (lighter + 0.05) / (darker + 0.05);
    }

    Result check(const Color& foreground, const Color& background, bool largeText = false) {
        Result r;
        r.ratio = contrastRatio(foreground, background);
        r.passAA = r.ratio >= (largeText ? 3.0 : 4.5);
        r.passAAA = r.ratio >= (largeText ? 4.5 : 7.0);
        std::lock_guard<std::mutex> lk(mutex_);
        ++checks_;
        if (!r.passAA) ++failures_;
        return r;
    }

    std::size_t checks() const { return checks_.load(); }
    std::size_t failures() const { return failures_.load(); }

    void reset() { checks_ = 0; failures_ = 0; }

private:
    ContrastRatioChecker() = default;
    ~ContrastRatioChecker() = default;
    ContrastRatioChecker(const ContrastRatioChecker&) = delete;
    ContrastRatioChecker& operator=(const ContrastRatioChecker&) = delete;

    mutable std::mutex mutex_;
    std::atomic<bool> enabled_{false};
    std::string config_;
    std::atomic<std::size_t> checks_{0};
    std::atomic<std::size_t> failures_{0};
};

} // namespace nexus::utility::accessibility
