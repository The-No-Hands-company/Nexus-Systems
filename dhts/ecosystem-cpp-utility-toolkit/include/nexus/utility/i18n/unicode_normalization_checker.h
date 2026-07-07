#pragma once

#include <algorithm>
#include <map>
#include <mutex>
#include <string>
#include <vector>

namespace nexus::utility::i18n {

class UnicodeNormalizationChecker {
public:
    static UnicodeNormalizationChecker& instance() {
        static UnicodeNormalizationChecker inst;
        return inst;
    }

    void initialize(const std::string& = "") { std::lock_guard<std::mutex> lock(mutex_); enabled_ = true; }
    void shutdown() { std::lock_guard<std::mutex> lock(mutex_); enabled_ = false; }
    bool isEnabled() const { return enabled_; }
    void enable() { std::lock_guard<std::mutex> lock(mutex_); enabled_ = true; }
    void disable() { std::lock_guard<std::mutex> lock(mutex_); enabled_ = false; }
    std::string getStatus() const { return "UnicodeNormalizationChecker: enabled=" + std::string(enabled_ ? "true" : "false"); }
    void reset() {}

    bool isNormalized(const std::string& text, const std::string& form) const {
        if (text.empty()) return true;
        if (form == "NFD" || form == "NFKD") {
            return !hasComposed(text);
        }
        if (form == "NFC" || form == "NFKC") {
            return !hasDecomposed(text);
        }
        return true;
    }

    std::string normalize(const std::string& text, const std::string& form) const {
        if (text.empty()) return text;

        static const std::map<std::string, std::string> composed_to_decomposed = {
            {"é", "e\xCC\x81"}, {"è", "e\xCC\x80"}, {"ê", "e\xCC\x82"},
            {"ë", "e\xCC\x88"}, {"à", "a\xCC\x80"}, {"â", "a\xCC\x82"},
            {"ù", "u\xCC\x80"}, {"û", "u\xCC\x82"}, {"ü", "u\xCC\x88"},
            {"ô", "o\xCC\x82"}, {"ö", "o\xCC\x88"}, {"î", "i\xCC\x82"},
            {"ï", "i\xCC\x88"}, {"ç", "c\xCC\xA7"}, {"ñ", "n\xCC\x83"},
        };

        static const std::map<std::string, std::string> decomposed_to_composed = {
            {"e\xCC\x81", "é"}, {"e\xCC\x80", "è"}, {"e\xCC\x82", "ê"},
            {"e\xCC\x88", "ë"}, {"a\xCC\x80", "à"}, {"a\xCC\x82", "â"},
            {"u\xCC\x80", "ù"}, {"u\xCC\x82", "û"}, {"u\xCC\x88", "ü"},
            {"o\xCC\x82", "ô"}, {"o\xCC\x88", "ö"}, {"i\xCC\x82", "î"},
            {"i\xCC\x88", "ï"}, {"c\xCC\xA7", "ç"}, {"n\xCC\x83", "ñ"},
        };

        std::string result = text;
        if (form == "NFD" || form == "NFKD") {
            for (const auto& [composed, decomposed] : composed_to_decomposed) {
                size_t pos = 0;
                while ((pos = result.find(composed, pos)) != std::string::npos) {
                    result.replace(pos, composed.size(), decomposed);
                    pos += decomposed.size();
                }
            }
        } else if (form == "NFC" || form == "NFKC") {
            for (const auto& [decomposed, composed] : decomposed_to_composed) {
                size_t pos = 0;
                while ((pos = result.find(decomposed, pos)) != std::string::npos) {
                    result.replace(pos, decomposed.size(), composed);
                    pos += composed.size();
                }
            }
        }
        if (form == "NFKD" || form == "NFKC") {
            result.erase(std::remove_if(result.begin(), result.end(), [](unsigned char c) {
                return c == 0x2DC || c == 0x307 || c == 0x311 || c == 0x313;
            }), result.end());
        }
        return result;
    }

private:
    bool hasComposed(const std::string& text) const {
        for (unsigned char c : text) {
            if (c == 0xC3 || c == 0xC4 || c == 0xC5) return true;
        }
        return false;
    }

    bool hasDecomposed(const std::string& text) const {
        for (size_t i = 0; i + 1 < text.size(); ++i) {
            if (static_cast<unsigned char>(text[i]) == 0xCC) return true;
        }
        return false;
    }

    UnicodeNormalizationChecker() = default;
    ~UnicodeNormalizationChecker() = default;
    UnicodeNormalizationChecker(const UnicodeNormalizationChecker&) = delete;
    UnicodeNormalizationChecker& operator=(const UnicodeNormalizationChecker&) = delete;
    UnicodeNormalizationChecker(UnicodeNormalizationChecker&&) = delete;
    UnicodeNormalizationChecker& operator=(UnicodeNormalizationChecker&&) = delete;

    mutable std::mutex mutex_;
    bool enabled_ = false;
};

} // namespace nexus::utility::i18n
