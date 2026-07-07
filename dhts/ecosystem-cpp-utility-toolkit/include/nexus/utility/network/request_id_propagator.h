#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <chrono>
namespace nexus::utility::network {
class RequestIdPropagator {
public:
    struct RequestTrace { std::string requestId; std::string parentRequestId; std::string serviceName;
        std::chrono::system_clock::time_point startTime; std::chrono::microseconds duration{0};
        bool completed = false; };
    static RequestIdPropagator& instance(); void initialize(); void shutdown(); bool isEnabled() const;
    std::string startRequest(const std::string& service, const std::string& parentId = "");
    void completeRequest(const std::string& requestId);
    RequestTrace getTrace(const std::string& requestId) const;
    std::vector<RequestTrace> getChildRequests(const std::string& parentId) const;
    std::vector<RequestTrace> getAllRequests() const;
    size_t activeRequests() const;
    void clear();
private:
    RequestIdPropagator() = default; ~RequestIdPropagator() = default; bool enabled_ = false;
    uint64_t counter_ = 0;
    std::unordered_map<std::string, RequestTrace> traces_;
};
} // namespace nexus::utility::network