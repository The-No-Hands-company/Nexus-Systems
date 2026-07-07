#pragma once

#include <ctime>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <vector>
#include <cstring>

namespace nexus::utility::i18n {

class DateFormatValidator {
public:
    static DateFormatValidator& instance() {
        static DateFormatValidator inst;
        return inst;
    }

    void initialize(const std::string& = "") { std::lock_guard<std::mutex> lock(mutex_); enabled_ = true; }
    void shutdown() { std::lock_guard<std::mutex> lock(mutex_); enabled_ = false; }
    bool isEnabled() const { return enabled_; }
    void enable() { std::lock_guard<std::mutex> lock(mutex_); enabled_ = true; }
    void disable() { std::lock_guard<std::mutex> lock(mutex_); enabled_ = false; }
    std::string getStatus() const { return "DateFormatValidator: enabled=" + std::string(enabled_ ? "true" : "false"); }
    void reset() {}

    bool validateFormat(const std::string& date_str, const std::string& format) {
        std::tm tm = {};
        if (format == "ISO8601" || format == "%Y-%m-%d") {
            return sscanf(date_str.c_str(), "%d-%d-%d", &tm.tm_year, &tm.tm_mon, &tm.tm_mday) == 3;
        }
        if (format == "%d/%m/%Y") {
            return sscanf(date_str.c_str(), "%d/%d/%d", &tm.tm_mday, &tm.tm_mon, &tm.tm_year) == 3;
        }
        if (format == "%m/%d/%Y") {
            return sscanf(date_str.c_str(), "%d/%d/%d", &tm.tm_mon, &tm.tm_mday, &tm.tm_year) == 3;
        }
        return false;
    }

    std::optional<std::tm> parseDate(const std::string& date_str, const std::string& format) {
        std::tm tm = {};
        std::memset(&tm, 0, sizeof(tm));
        if (format == "ISO8601" || format == "%Y-%m-%d") {
            if (sscanf(date_str.c_str(), "%d-%d-%d", &tm.tm_year, &tm.tm_mon, &tm.tm_mday) == 3) {
                tm.tm_year -= 1900;
                tm.tm_mon -= 1;
                return tm;
            }
        }
        if (format == "%d/%m/%Y") {
            if (sscanf(date_str.c_str(), "%d/%d/%d", &tm.tm_mday, &tm.tm_mon, &tm.tm_year) == 3) {
                tm.tm_year -= 1900;
                tm.tm_mon -= 1;
                return tm;
            }
        }
        if (format == "%m/%d/%Y") {
            if (sscanf(date_str.c_str(), "%d/%d/%d", &tm.tm_mon, &tm.tm_mday, &tm.tm_year) == 3) {
                tm.tm_year -= 1900;
                tm.tm_mon -= 1;
                return tm;
            }
        }
        return std::nullopt;
    }

    std::vector<std::string> getSupportedFormats() const {
        return {"ISO8601", "%Y-%m-%d", "%d/%m/%Y", "%m/%d/%Y"};
    }

private:
    DateFormatValidator() = default;
    ~DateFormatValidator() = default;
    DateFormatValidator(const DateFormatValidator&) = delete;
    DateFormatValidator& operator=(const DateFormatValidator&) = delete;
    DateFormatValidator(DateFormatValidator&&) = delete;
    DateFormatValidator& operator=(DateFormatValidator&&) = delete;

    mutable std::mutex mutex_;
    bool enabled_ = false;
};

} // namespace nexus::utility::i18n
