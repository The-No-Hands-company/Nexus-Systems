#pragma once
#include <string>
#include <vector>
#include <chrono>
namespace nexus::utility::database {
class DeadlockQueryLogger {
public:
    struct DeadlockEvent { std::string query1,query2,resource; std::chrono::system_clock::time_point timestamp; };
    static DeadlockQueryLogger& instance(); void initialize(); void shutdown(); bool isEnabled() const;
    void recordDeadlock(const std::string& q1,const std::string& q2,const std::string& r);
    std::vector<DeadlockEvent> getEvents() const; size_t getDeadlockCount() const; void clear();
private:
    DeadlockQueryLogger()=default; ~DeadlockQueryLogger()=default; bool enabled_=false;
    std::vector<DeadlockEvent> events_;
};
} // namespace nexus::utility::database