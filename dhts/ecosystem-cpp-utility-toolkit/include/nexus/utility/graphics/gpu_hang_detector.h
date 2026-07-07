#pragma once
#include <string>
#include <vector>
#include <chrono>
#include <unordered_map>
namespace nexus::utility::graphics {
class GpuHangDetector {
public:
    struct GpuOperation { std::string name; std::chrono::milliseconds elapsed{0}; bool timedOut; bool recovered; };
    static GpuHangDetector& instance(); void initialize(); void shutdown(); bool isEnabled() const;
    void startOperation(const std::string& name);
    void endOperation(const std::string& name, bool recovered=true);
    std::vector<GpuOperation> getOperations() const;
    size_t getTimeoutCount() const;
    void setTimeoutThreshold(std::chrono::milliseconds threshold);
    void clear();
private:
    GpuHangDetector()=default; ~GpuHangDetector()=default; bool enabled_=false;
    std::chrono::milliseconds threshold_{2000};
    std::unordered_map<std::string,std::chrono::steady_clock::time_point> active_;
    std::vector<GpuOperation> history_;
};
} // namespace nexus::utility::graphics