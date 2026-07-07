#pragma once

#include <cstdint>
#include <map>
#include <mutex>
#include <string>
#include <vector>

namespace nexus::utility::i18n {

class PluralFormValidator {
public:
    static PluralFormValidator& instance() {
        static PluralFormValidator inst;
        return inst;
    }

    void initialize(const std::string& = "") { std::lock_guard<std::mutex> lock(mutex_); enabled_ = true; }
    void shutdown() { std::lock_guard<std::mutex> lock(mutex_); enabled_ = false; reset(); }
    bool isEnabled() const { return enabled_; }
    void enable() { std::lock_guard<std::mutex> lock(mutex_); enabled_ = true; }
    void disable() { std::lock_guard<std::mutex> lock(mutex_); enabled_ = false; }
    std::string getStatus() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return "PluralFormValidator: " + std::to_string(rules_.size()) + " locales with rules, enabled=" +
               std::string(enabled_ ? "true" : "false");
    }
    void reset() { std::lock_guard<std::mutex> lock(mutex_); rules_.clear(); }

    void setPluralRules(const std::string& locale, const std::vector<std::string>& forms) {
        std::lock_guard<std::mutex> lock(mutex_);
        rules_[locale] = forms;
    }

    int getPluralForm(const std::string& locale, int count) const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::string loc = locale.size() >= 2 ? locale.substr(0, 2) : locale;

        if (loc == "en" || loc == "de" || loc == "es") {
            return (count == 1) ? 0 : 1;
        }
        if (loc == "fr") {
            return (count < 2) ? 0 : 1;
        }
        if (loc == "ru") {
            int n = count % 100;
            int n10 = n % 10;
            if (n10 == 1 && n != 11) return 0;
            if (n10 >= 2 && n10 <= 4 && (n < 12 || n > 14)) return 1;
            return 2;
        }
        if (loc == "ar") {
            int n = count % 100;
            if (count == 0) return 0;
            if (count == 1) return 1;
            if (count == 2) return 2;
            if (n >= 3 && n <= 10) return 3;
            if (n >= 11 && n <= 99) return 4;
            return 5;
        }
        if (loc == "zh" || loc == "ja" || loc == "ko") {
            return 0;
        }

        auto it = rules_.find(locale);
        if (it != rules_.end() && !it->second.empty()) {
            return std::min(static_cast<int>(it->second.size()) - 1, 0);
        }
        return (count == 1) ? 0 : 1;
    }

    int getFormCount(const std::string& locale) const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::string loc = locale.size() >= 2 ? locale.substr(0, 2) : locale;
        if (loc == "en" || loc == "de" || loc == "es" || loc == "fr") return 2;
        if (loc == "ru") return 3;
        if (loc == "ar") return 6;
        if (loc == "zh" || loc == "ja" || loc == "ko") return 1;

        auto it = rules_.find(locale);
        return (it != rules_.end()) ? static_cast<int>(it->second.size()) : 2;
    }

private:
    PluralFormValidator() = default;
    ~PluralFormValidator() = default;
    PluralFormValidator(const PluralFormValidator&) = delete;
    PluralFormValidator& operator=(const PluralFormValidator&) = delete;
    PluralFormValidator(PluralFormValidator&&) = delete;
    PluralFormValidator& operator=(PluralFormValidator&&) = delete;

    mutable std::mutex mutex_;
    bool enabled_ = false;
    std::map<std::string, std::vector<std::string>> rules_;
};

} // namespace nexus::utility::i18n
