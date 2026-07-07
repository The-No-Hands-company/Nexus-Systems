#pragma once

#include <string>
#include <mutex>

namespace nexus::utility::string {

/// @brief Simple Unicode normalization checks (NFC/NFD) over UTF-8 text.
///
/// This is a lightweight approximation based on combining-character detection,
/// not a full Unicode normalization implementation.
class NormalizationValidator {
public:
    static NormalizationValidator& instance() {
        static NormalizationValidator inst;
        return inst;
    }

    void initialize() {
        std::lock_guard<std::mutex> lock(mutex_);
        enabled_ = true;
    }

    void shutdown() {
        std::lock_guard<std::mutex> lock(mutex_);
        enabled_ = false;
    }

    bool isEnabled() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return enabled_;
    }

    /// Approximate check whether text is in the given normalization form.
    /// NFC: no standalone combining marks present. NFD: any decomposition ok.
    bool isNormalized(const std::string& text, const std::string& form) const {
        bool has_combining = containsCombining(text);
        if (form == "NFC" || form == "NFKC") {
            return !has_combining;
        }
        // NFD / NFKD are treated as always acceptable in this approximation.
        return true;
    }

    /// Approximate normalization. For NFC we strip standalone combining marks;
    /// for NFD we return the text unchanged (already decomposed representation).
    std::string normalize(const std::string& text, const std::string& form) const {
        if (form != "NFC" && form != "NFKC") {
            return text;
        }
        std::string out;
        out.reserve(text.size());
        size_t i = 0;
        while (i < text.size()) {
            size_t len = utf8Length(static_cast<unsigned char>(text[i]));
            if (i + len > text.size()) len = 1;
            unsigned int cp = decode(text, i, len);
            if (!isCombining(cp)) {
                out.append(text, i, len);
            }
            i += len;
        }
        return out;
    }

private:
    NormalizationValidator() = default;
    ~NormalizationValidator() = default;
    NormalizationValidator(const NormalizationValidator&) = delete;
    NormalizationValidator& operator=(const NormalizationValidator&) = delete;

    static bool containsCombining(const std::string& text) {
        size_t i = 0;
        while (i < text.size()) {
            size_t len = utf8Length(static_cast<unsigned char>(text[i]));
            if (i + len > text.size()) len = 1;
            if (isCombining(decode(text, i, len))) return true;
            i += len;
        }
        return false;
    }

    static size_t utf8Length(unsigned char lead) {
        if ((lead & 0x80u) == 0x00u) return 1;
        if ((lead & 0xE0u) == 0xC0u) return 2;
        if ((lead & 0xF0u) == 0xE0u) return 3;
        if ((lead & 0xF8u) == 0xF0u) return 4;
        return 1;
    }

    static unsigned int decode(const std::string& text, size_t pos, size_t len) {
        unsigned char lead = static_cast<unsigned char>(text[pos]);
        unsigned int cp = 0;
        if (len == 1) return lead;
        if (len == 2) cp = lead & 0x1Fu;
        else if (len == 3) cp = lead & 0x0Fu;
        else cp = lead & 0x07u;
        for (size_t k = 1; k < len; ++k) {
            cp = (cp << 6) | (static_cast<unsigned char>(text[pos + k]) & 0x3Fu);
        }
        return cp;
    }

    static bool isCombining(unsigned int cp) {
        return (cp >= 0x0300u && cp <= 0x036Fu) ||
               (cp >= 0x1AB0u && cp <= 0x1AFFu) ||
               (cp >= 0x1DC0u && cp <= 0x1DFFu) ||
               (cp >= 0x20D0u && cp <= 0x20FFu) ||
               (cp >= 0xFE20u && cp <= 0xFE2Fu);
    }

    mutable std::mutex mutex_;
    bool enabled_ = false;
};

} // namespace nexus::utility::string
