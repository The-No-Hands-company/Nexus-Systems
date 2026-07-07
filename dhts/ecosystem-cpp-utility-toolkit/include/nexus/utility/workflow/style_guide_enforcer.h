#pragma once

#include <string>
#include <vector>
#include <atomic>
#include <mutex>
#include <cctype>

namespace nexus::utility::workflow {

/**
 * @brief Validate simple code style rules (line length, tabs, trailing space).
 */
class StyleGuideEnforcer {
public:
    struct Rule {
        std::string name;
        bool enabled = true;
    };

    struct Violation {
        std::string rule;
        std::size_t line = 0;
        std::string detail;
    };

    static StyleGuideEnforcer& instance() {
        static StyleGuideEnforcer inst;
        return inst;
    }

    void initialize(const std::string& config = "") {
        std::lock_guard<std::mutex> lk(mutex_);
        config_ = config;
        enabled_ = true;
    }
    void shutdown() { enabled_ = false; }
    bool isEnabled() const { return enabled_.load(); }

    void setMaxLineLength(std::size_t maxLen) {
        std::lock_guard<std::mutex> lk(mutex_);
        maxLineLength_ = maxLen;
    }
    void setDisallowTabs(bool disallow) {
        std::lock_guard<std::mutex> lk(mutex_);
        disallowTabs_ = disallow;
    }

    /// Check a single source line; records and returns any violations.
    std::vector<Violation> checkLine(const std::string& line, std::size_t lineNumber) {
        std::size_t maxLen; bool noTabs;
        { std::lock_guard<std::mutex> lk(mutex_); maxLen = maxLineLength_; noTabs = disallowTabs_; }
        std::vector<Violation> found;
        if (line.size() > maxLen)
            found.push_back({"line-length", lineNumber,
                             "line exceeds " + std::to_string(maxLen) + " chars"});
        if (noTabs && line.find('\t') != std::string::npos)
            found.push_back({"no-tabs", lineNumber, "contains tab character"});
        if (!line.empty() && std::isspace(static_cast<unsigned char>(line.back())))
            found.push_back({"trailing-whitespace", lineNumber, "trailing whitespace"});
        std::lock_guard<std::mutex> lk(mutex_);
        for (const auto& v : found) violations_.push_back(v);
        return found;
    }

    /// Check a whole file's text (split on '\n').
    std::vector<Violation> checkText(const std::string& text) {
        std::vector<Violation> all;
        std::size_t line = 1, start = 0;
        for (std::size_t i = 0; i <= text.size(); ++i) {
            if (i == text.size() || text[i] == '\n') {
                std::string ln = text.substr(start, i - start);
                if (!ln.empty() && ln.back() == '\r') ln.pop_back();
                auto v = checkLine(ln, line);
                all.insert(all.end(), v.begin(), v.end());
                ++line;
                start = i + 1;
            }
        }
        return all;
    }

    std::vector<Violation> violations() const {
        std::lock_guard<std::mutex> lk(mutex_);
        return violations_;
    }

    void reset() {
        std::lock_guard<std::mutex> lk(mutex_);
        violations_.clear();
    }

private:
    StyleGuideEnforcer() = default;
    ~StyleGuideEnforcer() = default;
    StyleGuideEnforcer(const StyleGuideEnforcer&) = delete;
    StyleGuideEnforcer& operator=(const StyleGuideEnforcer&) = delete;

    mutable std::mutex mutex_;
    std::atomic<bool> enabled_{false};
    std::string config_;
    std::size_t maxLineLength_ = 100;
    bool disallowTabs_ = true;
    std::vector<Violation> violations_;
};

} // namespace nexus::utility::workflow
