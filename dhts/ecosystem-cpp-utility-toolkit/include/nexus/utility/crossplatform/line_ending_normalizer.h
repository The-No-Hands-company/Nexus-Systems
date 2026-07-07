#pragma once

#include <string>
#include <atomic>
#include <mutex>

namespace nexus::utility::crossplatform {

/**
 * @brief Normalize line endings between CRLF, LF and CR conventions.
 */
class LineEndingNormalizer {
public:
    enum class Style { LF, CRLF, CR };

    static LineEndingNormalizer& instance() {
        static LineEndingNormalizer inst;
        return inst;
    }

    void initialize(const std::string& config = "") {
        std::lock_guard<std::mutex> lk(mutex_);
        config_ = config;
        enabled_ = true;
    }
    void shutdown() { enabled_ = false; }
    bool isEnabled() const { return enabled_.load(); }

    /// Convert all line endings in `text` to LF.
    static std::string toLf(const std::string& text) {
        std::string out;
        out.reserve(text.size());
        for (std::size_t i = 0; i < text.size(); ++i) {
            char c = text[i];
            if (c == '\r') {
                out += '\n';
                if (i + 1 < text.size() && text[i + 1] == '\n') ++i;
            } else {
                out += c;
            }
        }
        return out;
    }

    /// Convert all line endings to CRLF.
    static std::string toCrlf(const std::string& text) {
        std::string lf = toLf(text);
        std::string out;
        out.reserve(lf.size() + lf.size() / 8);
        for (char c : lf) {
            if (c == '\n') out += '\r';
            out += c;
        }
        return out;
    }

    std::string normalize(const std::string& text, Style style) {
        std::lock_guard<std::mutex> lk(mutex_);
        ++conversions_;
        switch (style) {
            case Style::LF:   return toLf(text);
            case Style::CRLF: return toCrlf(text);
            case Style::CR: {
                std::string lf = toLf(text);
                for (char& c : lf) if (c == '\n') c = '\r';
                return lf;
            }
        }
        return text;
    }

    /// Detect the predominant line-ending style in the text.
    static Style detect(const std::string& text) {
        std::size_t crlf = 0, lf = 0, cr = 0;
        for (std::size_t i = 0; i < text.size(); ++i) {
            if (text[i] == '\r') {
                if (i + 1 < text.size() && text[i + 1] == '\n') { ++crlf; ++i; }
                else ++cr;
            } else if (text[i] == '\n') {
                ++lf;
            }
        }
        if (crlf >= lf && crlf >= cr) return Style::CRLF;
        if (cr >= lf) return Style::CR;
        return Style::LF;
    }

    std::size_t conversions() const { return conversions_.load(); }
    void reset() { conversions_ = 0; }

private:
    LineEndingNormalizer() = default;
    ~LineEndingNormalizer() = default;
    LineEndingNormalizer(const LineEndingNormalizer&) = delete;
    LineEndingNormalizer& operator=(const LineEndingNormalizer&) = delete;

    mutable std::mutex mutex_;
    std::atomic<bool> enabled_{false};
    std::string config_;
    std::atomic<std::size_t> conversions_{0};
};

} // namespace nexus::utility::crossplatform
