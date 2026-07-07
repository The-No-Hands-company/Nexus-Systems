#pragma once
#include <string>
#include <vector>
#include <regex>

namespace nexus::utility::security {

class CodeInjectionDetector {
public:
    struct InjectionResult { bool detected = false; std::string type; std::string payload; size_t position; };

    static CodeInjectionDetector& instance();
    void initialize();
    void shutdown();
    bool isEnabled() const;

    InjectionResult scan(const std::string& input);
    size_t getDetectionCount() const;
    std::vector<InjectionResult> getHistory() const;
    void clear();

private:
    CodeInjectionDetector() = default;
    ~CodeInjectionDetector() = default;
    bool enabled_ = false;
    size_t detectionCount_ = 0;
    std::vector<std::pair<std::string, std::regex>> patterns_;
    std::vector<InjectionResult> history_;
    void buildPatterns();
};
} // namespace nexus::utility::security
