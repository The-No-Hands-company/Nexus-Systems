#pragma once

#include <cstdint>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <vector>

namespace nexus::utility::i18n {

class TranslationCoverageTracker {
public:
    static TranslationCoverageTracker& instance() {
        static TranslationCoverageTracker inst;
        return inst;
    }

    void initialize(const std::string& = "") { std::lock_guard<std::mutex> lock(mutex_); enabled_ = true; }
    void shutdown() { std::lock_guard<std::mutex> lock(mutex_); enabled_ = false; reset(); }
    bool isEnabled() const { return enabled_; }
    void enable() { std::lock_guard<std::mutex> lock(mutex_); enabled_ = true; }
    void disable() { std::lock_guard<std::mutex> lock(mutex_); enabled_ = false; }
    std::string getStatus() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return "TranslationCoverageTracker: total=" + std::to_string(total_strings_) +
               ", " + std::to_string(translations_.size()) + " locales, enabled=" +
               std::string(enabled_ ? "true" : "false");
    }
    void reset() {
        std::lock_guard<std::mutex> lock(mutex_);
        total_strings_ = 0;
        translations_.clear();
    }

    void setTotalStrings(size_t count) {
        std::lock_guard<std::mutex> lock(mutex_);
        total_strings_ = count;
    }

    void markTranslated(const std::string& locale, const std::string& key) {
        std::lock_guard<std::mutex> lock(mutex_);
        translations_[locale].insert(key);
    }

    double getCoverage(const std::string& locale) const {
        std::lock_guard<std::mutex> lock(mutex_);
        if (total_strings_ == 0) return 0.0;
        auto it = translations_.find(locale);
        size_t translated = (it != translations_.end()) ? it->second.size() : 0;
        return static_cast<double>(translated) / total_strings_ * 100.0;
    }

    std::vector<std::string> getUntranslated(const std::string& locale) const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<std::string> untranslated;
        auto it = translations_.find(locale);
        const std::set<std::string>& translated_set = (it != translations_.end()) ?
            it->second : std::set<std::string>{};
        for (size_t i = 0; i < total_strings_; ++i) {
            std::string key = "key_" + std::to_string(i);
            if (translated_set.find(key) == translated_set.end()) {
                untranslated.push_back(key);
            }
        }
        return untranslated;
    }

private:
    TranslationCoverageTracker() = default;
    ~TranslationCoverageTracker() = default;
    TranslationCoverageTracker(const TranslationCoverageTracker&) = delete;
    TranslationCoverageTracker& operator=(const TranslationCoverageTracker&) = delete;
    TranslationCoverageTracker(TranslationCoverageTracker&&) = delete;
    TranslationCoverageTracker& operator=(TranslationCoverageTracker&&) = delete;

    mutable std::mutex mutex_;
    bool enabled_ = false;
    size_t total_strings_ = 0;
    std::map<std::string, std::set<std::string>> translations_;
};

} // namespace nexus::utility::i18n
