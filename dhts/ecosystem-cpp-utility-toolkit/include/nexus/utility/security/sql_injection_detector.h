#pragma once
#include <string>
#include <vector>
#include <regex>
#include <unordered_set>
#include <mutex>

namespace nexus::utility::security {

class SqlInjectionDetector {
public:
    struct DetectionResult { bool suspicious = false; std::string pattern; std::string input; size_t position; };

    static SqlInjectionDetector& instance();
    void initialize();
    void shutdown();
    bool isEnabled() const;

    DetectionResult scan(const std::string& input);
    bool addCustomPattern(const std::string& pattern);
    size_t getDetectionCount() const;
    std::vector<DetectionResult> getHistory() const;
    void clear();

private:
    SqlInjectionDetector() = default;
    ~SqlInjectionDetector() = default;
    bool enabled_ = false;
    size_t detectionCount_ = 0;
    std::vector<std::regex> patterns_;
    std::vector<DetectionResult> history_;
    mutable std::mutex mutex_;
    void buildDefaultPatterns();
};
} // namespace nexus::utility::security
