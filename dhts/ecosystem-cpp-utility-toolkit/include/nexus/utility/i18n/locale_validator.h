#pragma once

#include <algorithm>
#include <cctype>
#include <mutex>
#include <string>
#include <vector>

namespace nexus::utility::i18n {

class LocaleValidator {
public:
    static LocaleValidator& instance() {
        static LocaleValidator inst;
        return inst;
    }

    void initialize(const std::string& = "") { std::lock_guard<std::mutex> lock(mutex_); enabled_ = true; }
    void shutdown() { std::lock_guard<std::mutex> lock(mutex_); enabled_ = false; }
    bool isEnabled() const { return enabled_; }
    void enable() { std::lock_guard<std::mutex> lock(mutex_); enabled_ = true; }
    void disable() { std::lock_guard<std::mutex> lock(mutex_); enabled_ = false; }
    std::string getStatus() const { return "LocaleValidator: enabled=" + std::string(enabled_ ? "true" : "false"); }
    void reset() {}

    bool isValidLocale(const std::string& locale) const {
        if (locale.size() < 2) return false;
        if (!std::isalpha(static_cast<unsigned char>(locale[0])) ||
            !std::isalpha(static_cast<unsigned char>(locale[1]))) return false;
        if (locale.size() >= 3 && locale[2] != '-' && locale[2] != '_') return false;
        return true;
    }

    struct LocaleInfo {
        std::string language;
        std::string region;
        std::string script;
    };

    LocaleInfo parseLocale(const std::string& locale) const {
        LocaleInfo info;
        if (locale.size() >= 2) {
            info.language = locale.substr(0, 2);
        }
        size_t sep_pos = locale.find_first_of("-_");
        if (sep_pos != std::string::npos && sep_pos + 1 < locale.size()) {
            size_t next_sep = locale.find_first_of("-_", sep_pos + 1);
            if (next_sep != std::string::npos) {
                info.script = locale.substr(sep_pos + 1, next_sep - sep_pos - 1);
                info.region = locale.substr(next_sep + 1);
            } else {
                std::string second = locale.substr(sep_pos + 1);
                if (second.size() == 4) info.script = second;
                else info.region = second;
            }
        }
        return info;
    }

    std::vector<std::string> getSupportedLocales() const {
        return {"en", "en-US", "en-GB", "fr", "fr-FR", "de", "de-DE",
                "es", "es-ES", "ru", "ru-RU", "ar", "ar-SA", "zh",
                "zh-Hans", "zh-Hant", "ja", "ja-JP", "ko", "ko-KR"};
    }

private:
    LocaleValidator() = default;
    ~LocaleValidator() = default;
    LocaleValidator(const LocaleValidator&) = delete;
    LocaleValidator& operator=(const LocaleValidator&) = delete;
    LocaleValidator(LocaleValidator&&) = delete;
    LocaleValidator& operator=(LocaleValidator&&) = delete;

    mutable std::mutex mutex_;
    bool enabled_ = false;
};

} // namespace nexus::utility::i18n
