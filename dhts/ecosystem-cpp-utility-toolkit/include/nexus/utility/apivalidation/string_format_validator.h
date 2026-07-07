#pragma once

#include <string>
#include <vector>
#include <mutex>
#include <regex>

namespace nexus::utility::apivalidation {

class StringFormatValidator {
public:
    static StringFormatValidator& instance() {
        static StringFormatValidator inst;
        return inst;
    }

    void initialize(const std::string& config = "") { std::lock_guard<std::mutex> lk(mutex_); enabled_ = true; config_ = config; }
    void shutdown() { std::lock_guard<std::mutex> lk(mutex_); enabled_ = false; }
    bool isEnabled() const { return enabled_; }
    void enable() { std::lock_guard<std::mutex> lk(mutex_); enabled_ = true; }
    void disable() { std::lock_guard<std::mutex> lk(mutex_); enabled_ = false; }
    void reset() { std::lock_guard<std::mutex> lk(mutex_); error_count_ = 0; }

    bool validate(const std::string& format_str, size_t arg_count) {
        std::lock_guard<std::mutex> lk(mutex_);
        if (!enabled_) return true;
        size_t specifier_count = countSpecifiers(format_str);
        if (specifier_count != arg_count) {
            error_count_++;
            return false;
        }
        return true;
    }

    size_t getErrorCount() const {
        std::lock_guard<std::mutex> lk(mutex_);
        return error_count_;
    }

private:
    size_t countSpecifiers(const std::string& str) const {
        size_t count = 0;
        for (size_t i = 0; i < str.size(); ++i) {
            if (str[i] == '%') {
                if (i + 1 < str.size() && str[i + 1] == '%') { ++i; continue; }
                ++count;
            }
        }
        return count;
    }

    StringFormatValidator() = default;
    ~StringFormatValidator() = default;
    StringFormatValidator(const StringFormatValidator&) = delete;
    StringFormatValidator& operator=(const StringFormatValidator&) = delete;
    StringFormatValidator(StringFormatValidator&&) = delete;
    StringFormatValidator& operator=(StringFormatValidator&&) = delete;

    mutable std::mutex mutex_;
    bool enabled_ = false;
    std::string config_;
    size_t error_count_ = 0;
};

} // namespace nexus::utility::apivalidation
