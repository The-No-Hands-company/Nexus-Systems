#pragma once
#include <string>
#include <vector>
#include <chrono>
namespace nexus::utility::network {
class GrpcInterceptorDebug {
public:
    struct RpcCall { std::string service; std::string method; std::chrono::microseconds duration{0};
        bool success = false; std::string error; std::chrono::system_clock::time_point timestamp;
        size_t requestSize = 0; size_t responseSize = 0; };
    static GrpcInterceptorDebug& instance(); void initialize(); void shutdown(); bool isEnabled() const;
    RpcCall startCall(const std::string& service, const std::string& method);
    void finishCall(RpcCall& call, bool success, const std::string& error = "", size_t reqSize = 0, size_t respSize = 0);
    std::vector<RpcCall> getHistory() const;
    std::vector<RpcCall> getHistoryForService(const std::string& service) const;
    size_t getCallCount() const;
    size_t getErrorCount() const;
    std::chrono::microseconds getAverageLatency() const;
    void clear();
private:
    GrpcInterceptorDebug() = default; ~GrpcInterceptorDebug() = default; bool enabled_ = false;
    std::vector<RpcCall> history_; size_t callCount_ = 0; size_t errorCount_ = 0;
    std::chrono::microseconds totalLatency_{0};
};
} // namespace nexus::utility::network