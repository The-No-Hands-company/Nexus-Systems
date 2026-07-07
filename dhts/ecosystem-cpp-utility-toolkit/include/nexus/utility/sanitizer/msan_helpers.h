#pragma once
#include <string>
#include <vector>
namespace nexus::utility::sanitizer {
class MsanHelpers {
public:
    static MsanHelpers& instance(); void initialize(); void shutdown(); bool isEnabled() const;
    void record(const std::string& data); std::vector<std::string> getHistory() const; size_t getCount() const; void clear();
private:
    MsanHelpers()=default; ~MsanHelpers()=default; bool enabled_=false; std::vector<std::string> history_;
};
} // namespace nexus::utility::sanitizer