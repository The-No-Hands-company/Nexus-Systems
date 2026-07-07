#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <any>
namespace nexus::utility::testing {
class MockGenerator {
public:
    struct MockCall { std::string method; std::vector<std::any> args; size_t callIndex; };
    using MockBehavior = std::function<std::any(const std::vector<std::any>&)>;
    static MockGenerator& instance(); void initialize(); void shutdown(); bool isEnabled() const;
    void registerMock(const std::string& className);
    void setBehavior(const std::string& className, const std::string& method, MockBehavior behavior);
    void recordCall(const std::string& className, const std::string& method, const std::vector<std::any>& args);
    std::vector<MockCall> getCalls(const std::string& className) const;
    size_t getCallCount(const std::string& className) const;
    void verifyCalled(const std::string& className, const std::string& method, size_t expectedCount);
    void clear();
private:
    MockGenerator()=default; ~MockGenerator()=default; bool enabled_=false;
    std::unordered_map<std::string,std::unordered_map<std::string,MockBehavior>> behaviors_;
    std::unordered_map<std::string,std::vector<MockCall>> calls_;
};
} // namespace nexus::utility::testing