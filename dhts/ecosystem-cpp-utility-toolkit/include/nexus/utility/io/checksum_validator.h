#pragma once
#include <string>
#include <vector>
namespace nexus::utility::io {
class ChecksumValidator {
public:
    static ChecksumValidator& instance(); void initialize(); void shutdown(); bool isEnabled() const;
    void record(const std::string& data); std::vector<std::string> getHistory() const; size_t getCount() const; void clear();
private:
    ChecksumValidator()=default; ~ChecksumValidator()=default; bool enabled_=false; std::vector<std::string> history_;
};
} // namespace nexus::utility::io