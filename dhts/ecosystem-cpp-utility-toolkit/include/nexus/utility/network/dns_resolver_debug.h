#pragma once
#include <string>
#include <vector>
#include <chrono>
namespace nexus::utility::network {
class DnsResolverDebug {
public:
    struct DnsQuery { std::string hostname; std::vector<std::string> resolvedIps; std::chrono::microseconds resolutionTime{0}; bool success = false; std::string error; };
    static DnsResolverDebug& instance(); void initialize(); void shutdown(); bool isEnabled() const;
    void recordLookup(const std::string& hostname, const std::vector<std::string>& ips, std::chrono::microseconds time, bool success, const std::string& error = "");
    std::vector<DnsQuery> getHistory() const;
    std::vector<DnsQuery> getHistoryForHost(const std::string& hostname) const;
    size_t getQueryCount() const;
    size_t getFailureCount() const;
    std::chrono::microseconds getAverageResolutionTime() const;
    void clear();
private:
    DnsResolverDebug() = default; ~DnsResolverDebug() = default; bool enabled_ = false;
    std::vector<DnsQuery> history_; size_t queryCount_ = 0; size_t failureCount_ = 0;
    std::chrono::microseconds totalTime_{0};
};
} // namespace nexus::utility::network