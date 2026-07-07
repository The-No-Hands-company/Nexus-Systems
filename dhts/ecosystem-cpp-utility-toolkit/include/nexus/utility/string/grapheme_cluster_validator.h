#pragma once

#include <string>
#include <cstddef>
#include <mutex>

namespace nexus::utility::string {

/// @brief Approximate grapheme cluster validation over UTF-8 text.
///
/// Uses a simple heuristic: a UTF-8 lead byte starts a new cluster unless the
/// code point it introduces is a combining mark. Combining marks attach to the
/// preceding base character rather than forming a new cluster.
class GraphemeClusterValidator {
public:
    static GraphemeClusterValidator& instance() {
        static GraphemeClusterValidator inst;
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

    /// Approximate number of grapheme clusters in the text.
    size_t countClusters(const std::string& text) const {
        size_t clusters = 0;
        size_t i = 0;
        while (i < text.size()) {
            size_t len = utf8Length(static_cast<unsigned char>(text[i]));
            if (i + len > text.size()) len = 1;
            unsigned int cp = decode(text, i, len);
            if (!isCombining(cp) || clusters == 0) {
                ++clusters;
            }
            i += len;
        }
        return clusters;
    }

    /// Returns false if the text begins with a combining mark (invalid boundary).
    bool validateClusterBoundaries(const std::string& text) const {
        if (text.empty()) return true;
        size_t len = utf8Length(static_cast<unsigned char>(text[0]));
        if (len > text.size()) return false;
        unsigned int cp = decode(text, 0, len);
        return !isCombining(cp);
    }

private:
    GraphemeClusterValidator() = default;
    ~GraphemeClusterValidator() = default;
    GraphemeClusterValidator(const GraphemeClusterValidator&) = delete;
    GraphemeClusterValidator& operator=(const GraphemeClusterValidator&) = delete;

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

    /// Detect common combining mark ranges.
    static bool isCombining(unsigned int cp) {
        return (cp >= 0x0300u && cp <= 0x036Fu) || // Combining Diacritical Marks
               (cp >= 0x1AB0u && cp <= 0x1AFFu) || // Combining Diacritical Marks Extended
               (cp >= 0x1DC0u && cp <= 0x1DFFu) || // Combining Diacritical Marks Supplement
               (cp >= 0x20D0u && cp <= 0x20FFu) || // Combining Diacritical Marks for Symbols
               (cp >= 0xFE20u && cp <= 0xFE2Fu);   // Combining Half Marks
    }

    mutable std::mutex mutex_;
    bool enabled_ = false;
};

} // namespace nexus::utility::string
