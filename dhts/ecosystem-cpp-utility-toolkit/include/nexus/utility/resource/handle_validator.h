#pragma once
#include <string>
#include <vector>
namespace nexus::utility::resource {
class HandleValidator {
public:
    static HandleValidator& instance(); void initialize(); void shutdown(); bool isEnabled() const;
    void record(const std::string& data); std::vector<std::string> getHistory() const; size_t getCount() const; void clear();
private:
    HandleValidator()=default; ~HandleValidator()=default; bool enabled_=false; std::vector<std::string> history_;
};
} // namespace nexus::utility::resource