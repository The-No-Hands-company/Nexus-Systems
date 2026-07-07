#pragma once
#include <string>
#include <vector>
#include <chrono>
namespace nexus::utility::database {
class QueryLogger {
public:
    struct QueryLog { std::string query; std::chrono::microseconds duration{0}; size_t rowsAffected; std::chrono::system_clock::time_point timestamp; };
    static QueryLogger& instance(); void initialize(); void shutdown(); bool isEnabled() const;
    void logQuery(const std::string& q,std::chrono::microseconds d,size_t rows=0);
    std::vector<QueryLog> getHistory() const; std::vector<QueryLog> getSlowQueries(std::chrono::microseconds t)const;
    size_t getQueryCount() const; std::chrono::microseconds getTotalTime() const; void clear();
private:
    QueryLogger()=default; ~QueryLogger()=default; bool enabled_=false; std::vector<QueryLog> history_;
};
} // namespace nexus::utility::database