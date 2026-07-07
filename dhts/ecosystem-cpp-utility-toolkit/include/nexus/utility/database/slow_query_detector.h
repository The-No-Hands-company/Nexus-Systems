#pragma once
#include <string>
#include <vector>
#include <chrono>
namespace nexus::utility::database {
class SlowQueryDetector {
public:
    struct SlowQuery { std::string query; std::chrono::microseconds duration{0},threshold{0}; };
    static SlowQueryDetector& instance(); void initialize(); void shutdown(); bool isEnabled() const;
    void setThreshold(std::chrono::microseconds t); bool isSlow(std::chrono::microseconds d)const;
    void recordQuery(const std::string& q,std::chrono::microseconds d); std::vector<SlowQuery> getSlowQueries()const;
    size_t getSlowCount()const; void clear();
private:
    SlowQueryDetector()=default; ~SlowQueryDetector()=default; bool enabled_=false;
    std::chrono::microseconds threshold_{100000}; std::vector<SlowQuery> slow_;
};
} // namespace nexus::utility::database