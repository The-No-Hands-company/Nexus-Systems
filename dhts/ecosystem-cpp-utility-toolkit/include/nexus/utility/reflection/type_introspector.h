#pragma once
#include <string>
#include <vector>
namespace nexus::utility::reflection {
class TypeIntrospector {
public:
    static TypeIntrospector& instance(); void initialize(); void shutdown(); bool isEnabled() const;
    void record(const std::string& data); std::vector<std::string> getHistory() const; size_t getCount() const; void clear();
private:
    TypeIntrospector()=default; ~TypeIntrospector()=default; bool enabled_=false; std::vector<std::string> history_;
};
} // namespace nexus::utility::reflection