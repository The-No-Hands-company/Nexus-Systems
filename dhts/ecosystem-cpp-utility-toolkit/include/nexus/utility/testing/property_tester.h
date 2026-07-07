#pragma once
#include <string>
#include <vector>
#include <functional>
#include <chrono>
#include <unordered_map>
namespace nexus::utility::testing {
class PropertyTester {
public:
    using PropertyFn = std::function<bool()>;
    struct PropertyResult { std::string name; size_t testsRun; size_t testsPassed; bool propertyHolds; std::string counterexample; std::chrono::milliseconds duration{0}; };
    static PropertyTester& instance(); void initialize(); void shutdown(); bool isEnabled() const;
    void registerProperty(const std::string& name, PropertyFn test);
    PropertyResult runProperty(const std::string& name, size_t iterations=100);
    std::vector<PropertyResult> getResults() const;
    size_t getFailureCount() const;
    void clear();
private:
    PropertyTester()=default; ~PropertyTester()=default; bool enabled_=false;
    std::unordered_map<std::string,PropertyFn> properties_;
    std::vector<PropertyResult> results_;
};
} // namespace nexus::utility::testing