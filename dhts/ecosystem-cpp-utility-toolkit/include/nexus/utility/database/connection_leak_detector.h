#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <chrono>
namespace nexus::utility::database {
class ConnectionLeakDetector {
public:
    struct Connection { int id; std::string db; std::chrono::system_clock::time_point opened; bool closed; };
    static ConnectionLeakDetector& instance(); void initialize(); void shutdown(); bool isEnabled() const;
    int recordOpen(const std::string& db); void recordClose(int id);
    std::vector<Connection> getLeaked() const; size_t getLeakCount() const;
    std::chrono::milliseconds getLongestOpen() const; void clear();
private:
    ConnectionLeakDetector()=default; ~ConnectionLeakDetector()=default; bool enabled_=false;
    int nextId_=1; std::unordered_map<int,Connection> connections_;
};
} // namespace nexus::utility::database