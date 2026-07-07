#pragma once
#include <string>
#include <vector>
#include <regex>
#include <unordered_set>

namespace nexus::utility::security {

class SecretLeakDetector {
public:
    struct LeakDetection { std::string pattern; std::string value; size_t position; std::string context; };

    static SecretLeakDetector& instance();
    void initialize();
    void shutdown();
    bool isEnabled() const;

    std::vector<LeakDetection> scan(const std::string& content);
    void addSecretPattern(const std::string& name, const std::regex& pattern);
    size_t getDetectionCount() const;
    std::vector<LeakDetection> getHistory() const;
    void clear();

private:
    SecretLeakDetector() = default;
    ~SecretLeakDetector() = default;
    bool enabled_ = false;
    size_t detectionCount_ = 0;
    std::vector<std::pair<std::string, std::regex>> patterns_;
    std::vector<LeakDetection> history_;
    void buildDefaultPatterns();
};
} // namespace nexus::utility::security
