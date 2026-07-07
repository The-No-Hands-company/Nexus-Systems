#pragma once

#include <map>
#include <mutex>
#include <set>
#include <string>
#include <vector>

namespace nexus::utility::i18n {

class I18nStringTracker {
public:
    static I18nStringTracker& instance() {
        static I18nStringTracker inst;
        return inst;
    }

    void initialize(const std::string& = "") { std::lock_guard<std::mutex> lock(mutex_); enabled_ = true; }
    void shutdown() { std::lock_guard<std::mutex> lock(mutex_); enabled_ = false; reset(); }
    bool isEnabled() const { return enabled_; }
    void enable() { std::lock_guard<std::mutex> lock(mutex_); enabled_ = true; }
    void disable() { std::lock_guard<std::mutex> lock(mutex_); enabled_ = false; }
    std::string getStatus() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return "I18nStringTracker: " + std::to_string(defaults_.size()) + " keys, enabled=" +
               std::string(enabled_ ? "true" : "false");
    }
    void reset() {
        std::lock_guard<std::mutex> lock(mutex_);
        defaults_.clear();
        translations_.clear();
    }

    void registerString(const std::string& key, const std::string& default_value) {
        std::lock_guard<std::mutex> lock(mutex_);
        defaults_[key] = default_value;
    }

    void addTranslation(const std::string& key, const std::string& locale, const std::string& value) {
        std::lock_guard<std::mutex> lock(mutex_);
        translations_[locale][key] = value;
    }

    std::string translate(const std::string& key, const std::string& locale) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto loc_it = translations_.find(locale);
        if (loc_it != translations_.end()) {
            auto key_it = loc_it->second.find(key);
            if (key_it != loc_it->second.end()) return key_it->second;
        }
        auto def_it = defaults_.find(key);
        return (def_it != defaults_.end()) ? def_it->second : key;
    }

    std::vector<std::string> getMissingTranslations(const std::string& locale) const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<std::string> missing;
        auto loc_it = translations_.find(locale);
        for (const auto& [key, _] : defaults_) {
            if (loc_it == translations_.end() || loc_it->second.find(key) == loc_it->second.end()) {
                missing.push_back(key);
            }
        }
        return missing;
    }

private:
    I18nStringTracker() = default;
    ~I18nStringTracker() = default;
    I18nStringTracker(const I18nStringTracker&) = delete;
    I18nStringTracker& operator=(const I18nStringTracker&) = delete;
    I18nStringTracker(I18nStringTracker&&) = delete;
    I18nStringTracker& operator=(I18nStringTracker&&) = delete;

    mutable std::mutex mutex_;
    bool enabled_ = false;
    std::map<std::string, std::string> defaults_;
    std::map<std::string, std::map<std::string, std::string>> translations_;
};

} // namespace nexus::utility::i18n
