#pragma once
#include <string>
#include <vector>
#include <filesystem>

namespace nexus::utility::security {

class PathTraversalValidator {
public:
    struct PathResult { bool safe = true; std::string input; std::string resolved; std::string error; };

    static PathTraversalValidator& instance();
    void initialize();
    void shutdown();
    bool isEnabled() const;

    PathResult validate(const std::string& path, const std::string& baseDir = "");
    bool containsTraversal(const std::string& path) const;
    void setBaseDirectory(const std::string& baseDir);
    std::string getBaseDirectory() const;
    size_t getBlockedCount() const;
    void clear();

private:
    PathTraversalValidator() = default;
    ~PathTraversalValidator() = default;
    bool enabled_ = false;
    std::string baseDir_;
    size_t blockedCount_ = 0;
    static bool hasDangerousPattern(const std::string& p);
};
} // namespace nexus::utility::security
