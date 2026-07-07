#pragma once

#include <cstdint>
#include <locale>
#include <map>
#include <mutex>
#include <string>
#include <cstring>

namespace nexus::utility::i18n {

class CollationDebugger {
public:
    static CollationDebugger& instance() {
        static CollationDebugger inst;
        return inst;
    }

    void initialize(const std::string& = "") { std::lock_guard<std::mutex> lock(mutex_); enabled_ = true; }
    void shutdown() { std::lock_guard<std::mutex> lock(mutex_); enabled_ = false; reset(); }
    bool isEnabled() const { return enabled_; }
    void enable() { std::lock_guard<std::mutex> lock(mutex_); enabled_ = true; }
    void disable() { std::lock_guard<std::mutex> lock(mutex_); enabled_ = false; }
    std::string getStatus() const {
        return "CollationDebugger: " + std::to_string(comparison_count_) + " comparisons, locale=" + locale_ +
               ", enabled=" + std::string(enabled_ ? "true" : "false");
    }
    void reset() {
        std::lock_guard<std::mutex> lock(mutex_);
        comparison_count_ = 0;
        locale_ = "";
    }

    void setLocale(const std::string& locale) {
        std::lock_guard<std::mutex> lock(mutex_);
        locale_ = locale;
    }

    int compare(const std::string& a, const std::string& b, const std::string& locale) {
        std::lock_guard<std::mutex> lock(mutex_);
        ++comparison_count_;
        try {
            std::locale loc(locale);
            const auto& collate = std::use_facet<std::collate<char>>(loc);
            return collate.compare(a.data(), a.data() + a.size(), b.data(), b.data() + b.size());
        } catch (...) {
            return a.compare(b);
        }
    }

    uint64_t getComparisonCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return comparison_count_;
    }

private:
    CollationDebugger() = default;
    ~CollationDebugger() = default;
    CollationDebugger(const CollationDebugger&) = delete;
    CollationDebugger& operator=(const CollationDebugger&) = delete;
    CollationDebugger(CollationDebugger&&) = delete;
    CollationDebugger& operator=(CollationDebugger&&) = delete;

    mutable std::mutex mutex_;
    bool enabled_ = false;
    uint64_t comparison_count_ = 0;
    std::string locale_;
};

} // namespace nexus::utility::i18n
