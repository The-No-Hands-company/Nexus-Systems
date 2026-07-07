#pragma once
#include <string>
#include <vector>
#include <chrono>
#include <unordered_map>
namespace nexus::utility::database {
class TransactionTracker {
public:
    struct Transaction { int id; std::string name; std::chrono::system_clock::time_point started; std::chrono::microseconds duration{0}; bool committed,rolledBack; };
    static TransactionTracker& instance(); void initialize(); void shutdown(); bool isEnabled() const;
    int beginTransaction(const std::string& n=""); void commit(int id); void rollback(int id);
    std::vector<Transaction> getActive()const; std::vector<Transaction> getHistory()const; size_t getActiveCount()const; void clear();
private:
    TransactionTracker()=default; ~TransactionTracker()=default; bool enabled_=false;
    int nextId_=1; std::unordered_map<int,Transaction> transactions_;
};
} // namespace nexus::utility::database