#pragma once
#include <string>
#include <vector>
namespace nexus::utility::error {
class FaultInjector {
public:
    static FaultInjector& instance(); void initialize(); void shutdown(); bool isEnabled() const;
    void record(const std::string& data); std::vector<std::string> getHistory() const; size_t getCount() const; void clear();
private:
    FaultInjector()=default; ~FaultInjector()=default; bool enabled_=false; std::vector<std::string> history_;
};
} // namespace nexus::utility::error