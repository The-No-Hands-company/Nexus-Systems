#pragma once
#include <string>
#include <vector>
namespace nexus::utility::assertions {
class RuntimeValidator {
public:
    static RuntimeValidator& instance(); void initialize(); void shutdown(); bool isEnabled() const;
    void record(const std::string& data); std::vector<std::string> getHistory() const; size_t getCount() const; void clear();
private:
    RuntimeValidator()=default; ~RuntimeValidator()=default; bool enabled_=false; std::vector<std::string> history_;
};
} // namespace nexus::utility::assertions