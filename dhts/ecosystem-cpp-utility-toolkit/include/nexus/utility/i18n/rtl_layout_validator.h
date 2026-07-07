#pragma once

#include <algorithm>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <vector>

namespace nexus::utility::i18n {

class RtlLayoutValidator {
public:
    static RtlLayoutValidator& instance() {
        static RtlLayoutValidator inst;
        return inst;
    }

    void initialize(const std::string& = "") { std::lock_guard<std::mutex> lock(mutex_); enabled_ = true; }
    void shutdown() { std::lock_guard<std::mutex> lock(mutex_); enabled_ = false; reset(); }
    bool isEnabled() const { return enabled_; }
    void enable() { std::lock_guard<std::mutex> lock(mutex_); enabled_ = true; }
    void disable() { std::lock_guard<std::mutex> lock(mutex_); enabled_ = false; }
    std::string getStatus() const {
        return "RtlLayoutValidator: " + std::to_string(text_directions_.size()) + " texts tracked, enabled=" +
               std::string(enabled_ ? "true" : "false");
    }
    void reset() { std::lock_guard<std::mutex> lock(mutex_); text_directions_.clear(); }

    bool isRtlLanguage(const std::string& locale) const {
        static const std::set<std::string> rtl_langs = {
            "ar", "he", "fa", "ur", "ps", "yi", "dv", "syr", "ku",
            "ar-SA", "ar-EG", "ar-IQ", "he-IL", "fa-IR", "ur-PK"
        };
        return rtl_langs.find(locale) != rtl_langs.end();
    }

    void setTextDirection(const std::string& text, bool is_rtl) {
        std::lock_guard<std::mutex> lock(mutex_);
        text_directions_[text] = is_rtl;
    }

    bool validateLayout(const std::string& text) const {
        if (text.empty()) return true;

        bool has_rtl_char = false;
        for (unsigned char c : text) {
            if (c >= 0xD7 && c <= 0xFB) {
                has_rtl_char = true;
                break;
            }
        }
        if (!has_rtl_char) return true;

        for (size_t i = 0; i < text.size(); ++i) {
            unsigned char c = static_cast<unsigned char>(text[i]);
            if (c == '(' || c == ')' || c == '[' || c == ']') return false;
            if (c >= '0' && c <= '9') {
                if (i + 1 < text.size() &&
                    static_cast<unsigned char>(text[i + 1]) >= 0xD7) return false;
            }
        }

        return true;
    }

private:
    RtlLayoutValidator() = default;
    ~RtlLayoutValidator() = default;
    RtlLayoutValidator(const RtlLayoutValidator&) = delete;
    RtlLayoutValidator& operator=(const RtlLayoutValidator&) = delete;
    RtlLayoutValidator(RtlLayoutValidator&&) = delete;
    RtlLayoutValidator& operator=(RtlLayoutValidator&&) = delete;

    mutable std::mutex mutex_;
    bool enabled_ = false;
    std::map<std::string, bool> text_directions_;
};

} // namespace nexus::utility::i18n
