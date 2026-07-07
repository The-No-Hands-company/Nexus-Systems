#pragma once
#include <string>
#include <vector>
#include <unordered_set>
#include <unordered_map>
namespace nexus::utility::testing {
class CoverageMarker {
public:
    static CoverageMarker& instance(); void initialize(); void shutdown(); bool isEnabled() const;
    void markExecuted(const std::string& file, int line);
    std::unordered_set<int> getCoveredLines(const std::string& file) const;
    bool isCovered(const std::string& file, int line) const;
    std::vector<std::string> getTrackedFiles() const;
    double getCoveragePercent(const std::string& file, int totalLines) const;
    void clear();
private:
    CoverageMarker()=default; ~CoverageMarker()=default; bool enabled_=false;
    std::unordered_map<std::string,std::unordered_set<int>> coverage_;
};
} // namespace nexus::utility::testing