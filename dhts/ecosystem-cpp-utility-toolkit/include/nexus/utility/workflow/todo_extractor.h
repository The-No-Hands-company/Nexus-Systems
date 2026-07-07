#pragma once
#include <string>
#include <vector>
namespace nexus::utility::workflow {
class TodoExtractor {
public:
    static TodoExtractor& instance(); void initialize(); void shutdown(); bool isEnabled() const;
    void record(const std::string& data); std::vector<std::string> getHistory() const; size_t getCount() const; void clear();
private:
    TodoExtractor()=default; ~TodoExtractor()=default; bool enabled_=false; std::vector<std::string> history_;
};
} // namespace nexus::utility::workflow