#pragma once

#include <algorithm>
#include <cstdint>
#include <map>
#include <mutex>
#include <string>

namespace nexus::utility::i18n {

class BidirectionalTextValidator {
public:
    static BidirectionalTextValidator& instance() {
        static BidirectionalTextValidator inst;
        return inst;
    }

    void initialize(const std::string& = "") { std::lock_guard<std::mutex> lock(mutex_); enabled_ = true; }
    void shutdown() { std::lock_guard<std::mutex> lock(mutex_); enabled_ = false; reset(); }
    bool isEnabled() const { return enabled_; }
    void enable() { std::lock_guard<std::mutex> lock(mutex_); enabled_ = true; }
    void disable() { std::lock_guard<std::mutex> lock(mutex_); enabled_ = false; }
    std::string getStatus() const { return "BidirectionalTextValidator: enabled=" + std::string(enabled_ ? "true" : "false"); }
    void reset() { std::lock_guard<std::mutex> lock(mutex_); }

    void setDirection(const std::string& text, bool is_rtl) {
        std::lock_guard<std::mutex> lock(mutex_);
        directions_[text] = is_rtl;
    }

    bool hasMixedDirection(const std::string& text) const {
        bool has_rtl = false;
        bool has_ltr = false;
        for (unsigned char c : text) {
            if (c >= 0xC0) {
                if (c >= 0xD7 && c <= 0xFB) has_rtl = true;
                else has_ltr = true;
            }
        }
        for (unsigned char c : text) {
            if (c < 0x80) has_ltr = true;
        }
        return has_rtl && has_ltr;
    }

    bool validateMarkers(const std::string& text) const {
        uint64_t rtl_markers = 0;
        for (unsigned char c : text) {
            if (c == 0xE2) rtl_markers++;
            if (c == 0xAC) rtl_markers++;
        }
        return (rtl_markers % 2) == 0;
    }

private:
    BidirectionalTextValidator() = default;
    ~BidirectionalTextValidator() = default;
    BidirectionalTextValidator(const BidirectionalTextValidator&) = delete;
    BidirectionalTextValidator& operator=(const BidirectionalTextValidator&) = delete;
    BidirectionalTextValidator(BidirectionalTextValidator&&) = delete;
    BidirectionalTextValidator& operator=(BidirectionalTextValidator&&) = delete;

    mutable std::mutex mutex_;
    bool enabled_ = false;
    std::map<std::string, bool> directions_;
};

} // namespace nexus::utility::i18n
