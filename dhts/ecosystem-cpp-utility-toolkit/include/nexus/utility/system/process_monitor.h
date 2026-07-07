#pragma once
#include <string>
#include <vector>
namespace nexus::utility::system {
class ProcessMonitor {
public:
    static ProcessMonitor& instance(); void initialize(); void shutdown(); bool isEnabled() const;
    void record(const std::string& data); std::vector<std::string> getHistory() const; size_t getCount() const; void clear();
private:
    ProcessMonitor()=default; ~ProcessMonitor()=default; bool enabled_=false; std::vector<std::string> history_;
};
} // namespace nexus::utility::system