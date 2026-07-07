#pragma once
#include <string>
#include <vector>
namespace nexus::utility::sanitizer {
class UbsanHandlers {
public:
    static UbsanHandlers& instance(); void initialize(); void shutdown(); bool isEnabled() const;
    void record(const std::string& data); std::vector<std::string> getHistory() const; size_t getCount() const; void clear();
private:
    UbsanHandlers()=default; ~UbsanHandlers()=default; bool enabled_=false; std::vector<std::string> history_;
};
} // namespace nexus::utility::sanitizer