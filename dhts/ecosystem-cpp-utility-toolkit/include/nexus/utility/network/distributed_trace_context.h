#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <chrono>
#include <cstdint>
namespace nexus::utility::network {
class DistributedTraceContext {
public:
    struct Span { std::string traceId; std::string spanId; std::string parentSpanId;
        std::string operationName; std::chrono::system_clock::time_point startTime;
        std::chrono::microseconds duration{0}; std::unordered_map<std::string, std::string> tags; };
    static DistributedTraceContext& instance(); void initialize(); void shutdown(); bool isEnabled() const;
    std::string generateTraceId();
    std::string generateSpanId();
    void startSpan(const std::string& traceId, const std::string& parentSpanId, const std::string& operation);
    void endSpan(const std::string& spanId);
    Span getSpan(const std::string& spanId) const;
    std::vector<Span> getTraceSpans(const std::string& traceId) const;
    void addTag(const std::string& spanId, const std::string& key, const std::string& value);
    std::vector<Span> getAllSpans() const;
    void clear();
private:
    DistributedTraceContext() = default; ~DistributedTraceContext() = default; bool enabled_ = false;
    uint64_t counter_ = 0;
    std::unordered_map<std::string, Span> spans_; // spanId -> Span
    std::unordered_map<std::string, std::vector<std::string>> traceSpans_; // traceId -> [spanId]
};
} // namespace nexus::utility::network