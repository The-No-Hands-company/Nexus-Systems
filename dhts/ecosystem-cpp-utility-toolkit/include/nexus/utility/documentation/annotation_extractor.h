#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <cctype>
#include <atomic>
#include <mutex>

namespace nexus::utility::documentation {

/**
 * @brief Extract annotations (e.g. @tag value) from documentation comments.
 */
class AnnotationExtractor {
public:
    struct Annotation {
        std::string tag;
        std::string value;
        std::string symbol;
    };

    static AnnotationExtractor& instance() {
        static AnnotationExtractor inst;
        return inst;
    }

    void initialize(const std::string& config = "") {
        std::lock_guard<std::mutex> lk(mutex_);
        config_ = config;
        enabled_ = true;
    }
    void shutdown() { enabled_ = false; }
    bool isEnabled() const { return enabled_.load(); }

    /// Extract @tag [value] annotations from a comment block.
    std::vector<Annotation> extract(const std::string& comment, const std::string& symbol = "") {
        std::vector<Annotation> found;
        std::size_t pos = 0;
        while ((pos = comment.find('@', pos)) != std::string::npos) {
            std::size_t start = pos + 1;
            std::size_t tagEnd = start;
            while (tagEnd < comment.size() &&
                   (std::isalnum(static_cast<unsigned char>(comment[tagEnd])) ||
                    comment[tagEnd] == '_')) ++tagEnd;
            if (tagEnd == start) { pos = start; continue; }
            std::string tag = comment.substr(start, tagEnd - start);
            std::size_t lineEnd = comment.find('\n', tagEnd);
            if (lineEnd == std::string::npos) lineEnd = comment.size();
            std::string value = comment.substr(tagEnd, lineEnd - tagEnd);
            std::size_t s = value.find_first_not_of(" \t");
            std::size_t e = value.find_last_not_of(" \t\r");
            value = (s == std::string::npos) ? "" : value.substr(s, e - s + 1);
            found.push_back({tag, value, symbol});
            pos = lineEnd;
        }
        std::lock_guard<std::mutex> lk(mutex_);
        for (const auto& a : found) { annotations_.push_back(a); ++countByTag_[a.tag]; }
        return found;
    }

    std::size_t countFor(const std::string& tag) const {
        std::lock_guard<std::mutex> lk(mutex_);
        auto it = countByTag_.find(tag);
        return it == countByTag_.end() ? 0 : it->second;
    }

    std::vector<Annotation> all() const {
        std::lock_guard<std::mutex> lk(mutex_);
        return annotations_;
    }

    void reset() {
        std::lock_guard<std::mutex> lk(mutex_);
        annotations_.clear();
        countByTag_.clear();
    }

private:
    AnnotationExtractor() = default;
    ~AnnotationExtractor() = default;
    AnnotationExtractor(const AnnotationExtractor&) = delete;
    AnnotationExtractor& operator=(const AnnotationExtractor&) = delete;

    mutable std::mutex mutex_;
    std::atomic<bool> enabled_{false};
    std::string config_;
    std::vector<Annotation> annotations_;
    std::unordered_map<std::string, std::size_t> countByTag_;
};

} // namespace nexus::utility::documentation
