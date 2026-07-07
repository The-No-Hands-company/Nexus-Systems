#pragma once
#include <string>
#include <vector>
#include <chrono>
namespace nexus::utility::database {
class OrmDebugger {
public:
    struct OrmOperation { std::string entity,operation,generatedSQL; std::chrono::microseconds duration{0}; };
    static OrmDebugger& instance(); void initialize(); void shutdown(); bool isEnabled() const;
    void recordOperation(const std::string& e,const std::string& op,const std::string& sql,std::chrono::microseconds d);
    std::vector<OrmOperation> getOperations() const; size_t getOperationCount() const;
    std::chrono::microseconds getTotalTime() const; void clear();
private:
    OrmDebugger()=default; ~OrmDebugger()=default; bool enabled_=false; std::vector<OrmOperation> ops_;
};
} // namespace nexus::utility::database