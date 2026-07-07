#!/usr/bin/env python3
"""Flesh out network TODO skeleton tools (11 files)."""
import os
BASE = "/run/media/zajferx/Data/dev/The-No-hands-Company/projects/Nexus-Systems/dhts/dht_nexus_debug"
INC = f"{BASE}/include/nexus/utility/network"
SRC = f"{BASE}/src/utility/network"
os.makedirs(SRC, exist_ok=True)
def w(p,c):
    with open(p,'w') as f: f.write(c)

# 1. connection_pool_monitor
w(f"{INC}/connection_pool_monitor.h", '''#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <chrono>
#include <cstdint>
namespace nexus::utility::network {
class ConnectionPoolMonitor {
public:
    struct PoolStats { std::string poolName; size_t totalConnections; size_t activeConnections;
        size_t idleConnections; size_t maxConnections; size_t waitCount;
        std::chrono::microseconds avgWaitTime{0}; };
    static ConnectionPoolMonitor& instance(); void initialize(); void shutdown(); bool isEnabled() const;
    void registerPool(const std::string& name, size_t maxConns);
    void recordAcquire(const std::string& name);
    void recordRelease(const std::string& name);
    void recordWait(const std::string& name, std::chrono::microseconds wait);
    PoolStats getStats(const std::string& name) const;
    std::vector<PoolStats> getAllStats() const;
    bool isExhausted(const std::string& name) const;
    void clear();
private:
    ConnectionPoolMonitor() = default; ~ConnectionPoolMonitor() = default; bool enabled_ = false;
    struct Internal { size_t maxConn; size_t active; size_t waitCount; size_t totalWaits; std::chrono::microseconds totalWait{0}; };
    std::unordered_map<std::string, Internal> pools_;
};
}''')
w(f"{SRC}/connection_pool_monitor.cpp", '''#include "nexus/utility/network/connection_pool_monitor.h"
namespace nexus::utility::network {
ConnectionPoolMonitor& ConnectionPoolMonitor::instance() { static ConnectionPoolMonitor i; return i; }
void ConnectionPoolMonitor::initialize() { enabled_ = true; pools_.clear(); }
void ConnectionPoolMonitor::shutdown() { enabled_ = false; }
bool ConnectionPoolMonitor::isEnabled() const { return enabled_; }
void ConnectionPoolMonitor::registerPool(const std::string& name, size_t maxConns) { pools_[name] = {maxConns, 0, 0, 0, std::chrono::microseconds(0)}; }
void ConnectionPoolMonitor::recordAcquire(const std::string& name) { auto it = pools_.find(name); if (it != pools_.end()) it->second.active++; }
void ConnectionPoolMonitor::recordRelease(const std::string& name) { auto it = pools_.find(name); if (it != pools_.end() && it->second.active > 0) it->second.active--; }
void ConnectionPoolMonitor::recordWait(const std::string& name, std::chrono::microseconds w) { auto it = pools_.find(name); if (it != pools_.end()) { it->second.waitCount++; it->second.totalWaits++; it->second.totalWait += w; } }
ConnectionPoolMonitor::PoolStats ConnectionPoolMonitor::getStats(const std::string& name) const {
    PoolStats s; auto it = pools_.find(name); if (it == pools_.end()) return s;
    s.poolName = name; s.totalConnections = it->second.maxConn; s.activeConnections = it->second.active;
    s.idleConnections = it->second.maxConn > it->second.active ? it->second.maxConn - it->second.active : 0;
    s.maxConnections = it->second.maxConn; s.waitCount = it->second.waitCount;
    s.avgWaitTime = it->second.totalWaits > 0 ? std::chrono::microseconds(it->second.totalWait.count() / it->second.totalWaits) : std::chrono::microseconds(0);
    return s;
}
auto ConnectionPoolMonitor::getAllStats() const -> std::vector<PoolStats> { std::vector<PoolStats> r; for (auto& [n,_] : pools_) r.push_back(getStats(n)); return r; }
bool ConnectionPoolMonitor::isExhausted(const std::string& name) const { auto it = pools_.find(name); return it != pools_.end() && it->second.active >= it->second.maxConn; }
void ConnectionPoolMonitor::clear() { pools_.clear(); }
}''')

# 2. distributed_trace_context
w(f"{INC}/distributed_trace_context.h", '''#pragma once
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
}''')
w(f"{SRC}/distributed_trace_context.cpp", '''#include "nexus/utility/network/distributed_trace_context.h"
#include <sstream>
#include <iomanip>
#include <random>
namespace nexus::utility::network {
DistributedTraceContext& DistributedTraceContext::instance() { static DistributedTraceContext i; return i; }
void DistributedTraceContext::initialize() { enabled_ = true; counter_ = 0; spans_.clear(); traceSpans_.clear(); }
void DistributedTraceContext::shutdown() { enabled_ = false; }
bool DistributedTraceContext::isEnabled() const { return enabled_; }
std::string DistributedTraceContext::generateTraceId() {
    std::ostringstream os; os << std::hex << std::setfill('0') << std::setw(16) << (++counter_);
    return os.str();
}
std::string DistributedTraceContext::generateSpanId() {
    std::ostringstream os; os << std::hex << std::setfill('0') << std::setw(8) << (++counter_);
    return os.str();
}
void DistributedTraceContext::startSpan(const std::string& traceId, const std::string& parentId, const std::string& op) {
    if (!enabled_) return;
    std::string spanId = generateSpanId();
    Span s{traceId, spanId, parentId, op, std::chrono::system_clock::now(), std::chrono::microseconds(0), {}};
    spans_[spanId] = s; traceSpans_[traceId].push_back(spanId);
}
void DistributedTraceContext::endSpan(const std::string& spanId) {
    auto it = spans_.find(spanId);
    if (it != spans_.end()) it->second.duration = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now() - it->second.startTime);
}
auto DistributedTraceContext::getSpan(const std::string& spanId) const -> Span { auto it = spans_.find(spanId); return it != spans_.end() ? it->second : Span{}; }
auto DistributedTraceContext::getTraceSpans(const std::string& traceId) const -> std::vector<Span> {
    std::vector<Span> r; auto it = traceSpans_.find(traceId);
    if (it != traceSpans_.end()) for (auto& sid : it->second) { auto s = spans_.find(sid); if (s != spans_.end()) r.push_back(s->second); }
    return r;
}
void DistributedTraceContext::addTag(const std::string& spanId, const std::string& key, const std::string& val) { auto it = spans_.find(spanId); if (it != spans_.end()) it->second.tags[key] = val; }
auto DistributedTraceContext::getAllSpans() const -> std::vector<Span> { std::vector<Span> r; for (auto& [_,s] : spans_) r.push_back(s); return r; }
void DistributedTraceContext::clear() { spans_.clear(); traceSpans_.clear(); }
}''')

# 3. grpc_interceptor_debug
w(f"{INC}/grpc_interceptor_debug.h", '''#pragma once
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
}''')
w(f"{SRC}/grpc_interceptor_debug.cpp", '''#include "nexus/utility/network/grpc_interceptor_debug.h"
namespace nexus::utility::network {
GrpcInterceptorDebug& GrpcInterceptorDebug::instance() { static GrpcInterceptorDebug i; return i; }
void GrpcInterceptorDebug::initialize() { enabled_ = true; clear(); }
void GrpcInterceptorDebug::shutdown() { enabled_ = false; }
bool GrpcInterceptorDebug::isEnabled() const { return enabled_; }
GrpcInterceptorDebug::RpcCall GrpcInterceptorDebug::startCall(const std::string& svc, const std::string& method) {
    RpcCall c; c.service = svc; c.method = method; c.timestamp = std::chrono::system_clock::now(); return c;
}
void GrpcInterceptorDebug::finishCall(RpcCall& c, bool ok, const std::string& err, size_t reqSz, size_t respSz) {
    if (!enabled_) return;
    c.duration = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now() - c.timestamp);
    c.success = ok; c.error = err; c.requestSize = reqSz; c.responseSize = respSz;
    history_.push_back(c); callCount_++; totalLatency_ += c.duration; if (!ok) errorCount_++;
}
auto GrpcInterceptorDebug::getHistory() const -> std::vector<RpcCall> { return history_; }
auto GrpcInterceptorDebug::getHistoryForService(const std::string& svc) const -> std::vector<RpcCall> { std::vector<RpcCall> r; for (auto& c : history_) if (c.service == svc) r.push_back(c); return r; }
size_t GrpcInterceptorDebug::getCallCount() const { return callCount_; }
size_t GrpcInterceptorDebug::getErrorCount() const { return errorCount_; }
std::chrono::microseconds GrpcInterceptorDebug::getAverageLatency() const { return callCount_ > 0 ? std::chrono::microseconds(totalLatency_.count() / callCount_) : std::chrono::microseconds(0); }
void GrpcInterceptorDebug::clear() { history_.clear(); callCount_ = 0; errorCount_ = 0; totalLatency_ = std::chrono::microseconds(0); }
}''')

# 4. network_error_classifier
w(f"{INC}/network_error_classifier.h", '''#pragma once
#include <string>
#include <vector>
#include <unordered_map>
namespace nexus::utility::network {
class NetworkErrorClassifier {
public:
    enum class Category { Timeout, ConnectionRefused, DNSError, SSLHandshake, Reset, Unreachable, Unknown };
    struct ErrorRecord { Category category; int errorCode; std::string message; size_t count; };
    static NetworkErrorClassifier& instance(); void initialize(); void shutdown(); bool isEnabled() const;
    void recordError(int errorCode, const std::string& message);
    ErrorRecord getStats(Category cat) const;
    std::vector<ErrorRecord> getAllStats() const;
    size_t getTotalErrors() const;
    static Category classify(int errorCode, const std::string& message);
    void clear();
private:
    NetworkErrorClassifier() = default; ~NetworkErrorClassifier() = default; bool enabled_ = false;
    std::unordered_map<Category, ErrorRecord> stats_;
    static std::string categoryName(Category c);
};
}''')
w(f"{SRC}/network_error_classifier.cpp", '''#include "nexus/utility/network/network_error_classifier.h"
namespace nexus::utility::network {
NetworkErrorClassifier& NetworkErrorClassifier::instance() { static NetworkErrorClassifier i; return i; }
void NetworkErrorClassifier::initialize() { enabled_ = true; stats_.clear(); }
void NetworkErrorClassifier::shutdown() { enabled_ = false; }
bool NetworkErrorClassifier::isEnabled() const { return enabled_; }

NetworkErrorClassifier::Category NetworkErrorClassifier::classify(int code, const std::string& msg) {
    if (msg.find("timeout") != std::string::npos || msg.find("timed out") != std::string::npos || code == 110) return Category::Timeout;
    if (msg.find("refused") != std::string::npos || code == 111) return Category::ConnectionRefused;
    if (msg.find("resolve") != std::string::npos || msg.find("name") != std::string::npos || code == -2) return Category::DNSError;
    if (msg.find("SSL") != std::string::npos || msg.find("certificate") != std::string::npos) return Category::SSLHandshake;
    if (msg.find("reset") != std::string::npos || code == 104) return Category::Reset;
    if (msg.find("unreachable") != std::string::npos || code == 113 || code == 101) return Category::Unreachable;
    return Category::Unknown;
}

std::string NetworkErrorClassifier::categoryName(Category c) {
    switch (c) { case Category::Timeout: return "Timeout"; case Category::ConnectionRefused: return "ConnectionRefused";
    case Category::DNSError: return "DNSError"; case Category::SSLHandshake: return "SSLHandshake";
    case Category::Reset: return "Reset"; case Category::Unreachable: return "Unreachable"; default: return "Unknown"; }
}

void NetworkErrorClassifier::recordError(int code, const std::string& msg) {
    if (!enabled_) return;
    Category c = classify(code, msg); auto& r = stats_[c]; r.category = c; r.errorCode = code; r.message = msg; r.count++;
}
auto NetworkErrorClassifier::getStats(Category cat) const -> ErrorRecord { auto it = stats_.find(cat); return it != stats_.end() ? it->second : ErrorRecord{cat, 0, "", 0}; }
auto NetworkErrorClassifier::getAllStats() const -> std::vector<ErrorRecord> { std::vector<ErrorRecord> r; for (auto& [c,s] : stats_) r.push_back(s); return r; }
size_t NetworkErrorClassifier::getTotalErrors() const { size_t t = 0; for (auto& [_,s] : stats_) t += s.count; return t; }
void NetworkErrorClassifier::clear() { stats_.clear(); }
}''')

# 5. network_partition_simulator
w(f"{INC}/network_partition_simulator.h", '''#pragma once
#include <string>
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <functional>
namespace nexus::utility::network {
class NetworkPartitionSimulator {
public:
    struct Partition { std::string name; std::unordered_set<std::string> nodes; };
    using CommunicationFn = std::function<bool(const std::string& from, const std::string& to)>;
    static NetworkPartitionSimulator& instance(); void initialize(); void shutdown(); bool isEnabled() const;
    void createPartition(const std::string& name, const std::vector<std::string>& nodes);
    void removePartition(const std::string& name);
    bool canCommunicate(const std::string& nodeA, const std::string& nodeB) const;
    bool isPartitioned(const std::string& node) const;
    std::vector<std::string> getPartitionMembers(const std::string& name) const;
    std::vector<Partition> getActivePartitions() const;
    size_t partitionCount() const;
    void clear();
private:
    NetworkPartitionSimulator() = default; ~NetworkPartitionSimulator() = default; bool enabled_ = false;
    std::unordered_map<std::string, Partition> partitions_;
    std::unordered_map<std::string, std::string> nodeToPartition_; // node -> partition name
};
}''')
w(f"{SRC}/network_partition_simulator.cpp", '''#include "nexus/utility/network/network_partition_simulator.h"
namespace nexus::utility::network {
NetworkPartitionSimulator& NetworkPartitionSimulator::instance() { static NetworkPartitionSimulator i; return i; }
void NetworkPartitionSimulator::initialize() { enabled_ = true; clear(); }
void NetworkPartitionSimulator::shutdown() { enabled_ = false; }
bool NetworkPartitionSimulator::isEnabled() const { return enabled_; }
void NetworkPartitionSimulator::createPartition(const std::string& name, const std::vector<std::string>& nodes) {
    if (!enabled_) return;
    Partition p{name, {}};
    for (auto& n : nodes) { p.nodes.insert(n); nodeToPartition_[n] = name; }
    partitions_[name] = p;
}
void NetworkPartitionSimulator::removePartition(const std::string& name) {
    auto it = partitions_.find(name);
    if (it != partitions_.end()) { for (auto& n : it->second.nodes) nodeToPartition_.erase(n); partitions_.erase(it); }
}
bool NetworkPartitionSimulator::canCommunicate(const std::string& a, const std::string& b) const {
    if (!enabled_) return true;
    auto pa = nodeToPartition_.find(a), pb = nodeToPartition_.find(b);
    if (pa == nodeToPartition_.end() || pb == nodeToPartition_.end()) return true;
    return pa->second == pb->second;
}
bool NetworkPartitionSimulator::isPartitioned(const std::string& node) const { return nodeToPartition_.find(node) != nodeToPartition_.end(); }
auto NetworkPartitionSimulator::getPartitionMembers(const std::string& name) const -> std::vector<std::string> { auto it = partitions_.find(name); if (it != partitions_.end()) return std::vector<std::string>(it->second.nodes.begin(), it->second.nodes.end()); return {}; }
auto NetworkPartitionSimulator::getActivePartitions() const -> std::vector<Partition> { std::vector<Partition> r; for (auto& [_,p] : partitions_) r.push_back(p); return r; }
size_t NetworkPartitionSimulator::partitionCount() const { return partitions_.size(); }
void NetworkPartitionSimulator::clear() { partitions_.clear(); nodeToPartition_.clear(); }
}''')

# 6. request_id_propagator
w(f"{INC}/request_id_propagator.h", '''#pragma once
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
}''')
w(f"{SRC}/request_id_propagator.cpp", '''#include "nexus/utility/network/request_id_propagator.h"
#include <sstream>
#include <iomanip>
namespace nexus::utility::network {
RequestIdPropagator& RequestIdPropagator::instance() { static RequestIdPropagator i; return i; }
void RequestIdPropagator::initialize() { enabled_ = true; counter_ = 0; traces_.clear(); }
void RequestIdPropagator::shutdown() { enabled_ = false; }
bool RequestIdPropagator::isEnabled() const { return enabled_; }
std::string RequestIdPropagator::startRequest(const std::string& svc, const std::string& parentId) {
    if (!enabled_) return "";
    std::ostringstream os; os << std::hex << std::setfill('0') << std::setw(16) << (++counter_);
    std::string id = os.str();
    traces_[id] = {id, parentId, svc, std::chrono::system_clock::now(), std::chrono::microseconds(0), false};
    return id;
}
void RequestIdPropagator::completeRequest(const std::string& id) {
    auto it = traces_.find(id); if (it == traces_.end()) return;
    it->second.duration = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now() - it->second.startTime);
    it->second.completed = true;
}
auto RequestIdPropagator::getTrace(const std::string& id) const -> RequestTrace { auto it = traces_.find(id); return it != traces_.end() ? it->second : RequestTrace{}; }
auto RequestIdPropagator::getChildRequests(const std::string& parentId) const -> std::vector<RequestTrace> {
    std::vector<RequestTrace> r; for (auto& [_,t] : traces_) if (t.parentRequestId == parentId) r.push_back(t); return r;
}
auto RequestIdPropagator::getAllRequests() const -> std::vector<RequestTrace> { std::vector<RequestTrace> r; for (auto& [_,t] : traces_) r.push_back(t); return r; }
size_t RequestIdPropagator::activeRequests() const { size_t c = 0; for (auto& [_,t] : traces_) if (!t.completed) c++; return c; }
void RequestIdPropagator::clear() { traces_.clear(); }
}''')

# 7. packet_logger
w(f"{INC}/packet_logger.h", '''#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <chrono>
#include <sstream>
#include <iomanip>
namespace nexus::utility::network {
class PacketLogger {
public:
    enum class Direction { Sent, Received };
    struct PacketEntry { Direction direction; std::vector<uint8_t> data; size_t size; std::chrono::system_clock::time_point timestamp; std::string protocol; };
    static PacketLogger& instance(); void initialize(); void shutdown(); bool isEnabled() const;
    void logPacket(Direction dir, const void* data, size_t size, const std::string& protocol = "TCP");
    std::vector<PacketEntry> getHistory() const;
    std::string formatPacket(const PacketEntry& pkt) const;
    size_t getPacketCount() const;
    size_t getTotalBytes() const;
    void setMaxPackets(size_t max);
    void clear();
private:
    PacketLogger() = default; ~PacketLogger() = default; bool enabled_ = false;
    size_t maxPackets_ = 1000; size_t totalBytes_ = 0;
    std::vector<PacketEntry> history_;
};
}''')
w(f"{SRC}/packet_logger.cpp", '''#include "nexus/utility/network/packet_logger.h"
#include <cstring>
namespace nexus::utility::network {
PacketLogger& PacketLogger::instance() { static PacketLogger i; return i; }
void PacketLogger::initialize() { enabled_ = true; history_.clear(); totalBytes_ = 0; }
void PacketLogger::shutdown() { enabled_ = false; }
bool PacketLogger::isEnabled() const { return enabled_; }
void PacketLogger::logPacket(Direction dir, const void* data, size_t sz, const std::string& proto) {
    if (!enabled_) return;
    PacketEntry e; e.direction = dir; e.size = sz; e.timestamp = std::chrono::system_clock::now(); e.protocol = proto;
    const uint8_t* bytes = static_cast<const uint8_t*>(data);
    e.data.assign(bytes, bytes + std::min(sz, (size_t)256));
    history_.push_back(e); totalBytes_ += sz;
    if (history_.size() > maxPackets_) history_.erase(history_.begin());
}
auto PacketLogger::getHistory() const -> std::vector<PacketEntry> { return history_; }
std::string PacketLogger::formatPacket(const PacketEntry& pkt) const {
    std::ostringstream os; os << (pkt.direction == Direction::Sent ? "SENT" : "RECV") << " " << pkt.protocol << " " << pkt.size << "B\\n";
    return os.str();
}
size_t PacketLogger::getPacketCount() const { return history_.size(); }
size_t PacketLogger::getTotalBytes() const { return totalBytes_; }
void PacketLogger::setMaxPackets(size_t m) { maxPackets_ = m; }
void PacketLogger::clear() { history_.clear(); totalBytes_ = 0; }
}''')

# 8. dns_resolver_debug
w(f"{INC}/dns_resolver_debug.h", '''#pragma once
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
}''')
w(f"{SRC}/dns_resolver_debug.cpp", '''#include "nexus/utility/network/dns_resolver_debug.h"
namespace nexus::utility::network {
DnsResolverDebug& DnsResolverDebug::instance() { static DnsResolverDebug i; return i; }
void DnsResolverDebug::initialize() { enabled_ = true; clear(); }
void DnsResolverDebug::shutdown() { enabled_ = false; }
bool DnsResolverDebug::isEnabled() const { return enabled_; }
void DnsResolverDebug::recordLookup(const std::string& host, const std::vector<std::string>& ips, std::chrono::microseconds t, bool ok, const std::string& err) {
    if (!enabled_) return;
    history_.push_back({host, ips, t, ok, err}); queryCount_++; totalTime_ += t; if (!ok) failureCount_++;
}
auto DnsResolverDebug::getHistory() const -> std::vector<DnsQuery> { return history_; }
auto DnsResolverDebug::getHistoryForHost(const std::string& host) const -> std::vector<DnsQuery> { std::vector<DnsQuery> r; for (auto& q : history_) if (q.hostname == host) r.push_back(q); return r; }
size_t DnsResolverDebug::getQueryCount() const { return queryCount_; }
size_t DnsResolverDebug::getFailureCount() const { return failureCount_; }
std::chrono::microseconds DnsResolverDebug::getAverageResolutionTime() const { return queryCount_ > 0 ? std::chrono::microseconds(totalTime_.count() / queryCount_) : std::chrono::microseconds(0); }
void DnsResolverDebug::clear() { history_.clear(); queryCount_ = 0; failureCount_ = 0; totalTime_ = std::chrono::microseconds(0); }
}''')

# 9. retry_policy_debugger
w(f"{INC}/retry_policy_debugger.h", '''#pragma once
#include <string>
#include <vector>
#include <chrono>
namespace nexus::utility::network {
class RetryPolicyDebugger {
public:
    struct RetryAttempt { std::string policyName; size_t attemptNumber; std::chrono::milliseconds delay; std::chrono::system_clock::time_point timestamp; bool success; };
    static RetryPolicyDebugger& instance(); void initialize(); void shutdown(); bool isEnabled() const;
    void registerPolicy(const std::string& name, size_t maxAttempts, std::chrono::milliseconds baseDelay);
    void recordAttempt(const std::string& policy, size_t attempt, std::chrono::milliseconds delay, bool success);
    std::vector<RetryAttempt> getHistory() const;
    size_t getTotalAttempts(const std::string& policy) const;
    double getSuccessRate(const std::string& policy) const;
    bool exceededMaxAttempts(const std::string& policy) const;
    void clear();
private:
    RetryPolicyDebugger() = default; ~RetryPolicyDebugger() = default; bool enabled_ = false;
    struct PolicyInfo { size_t maxAttempts; std::chrono::milliseconds baseDelay; size_t totalAttempts = 0; size_t successes = 0; };
    std::unordered_map<std::string, PolicyInfo> policies_;
    std::vector<RetryAttempt> history_;
};
}''')
w(f"{SRC}/retry_policy_debugger.cpp", '''#include "nexus/utility/network/retry_policy_debugger.h"
namespace nexus::utility::network {
RetryPolicyDebugger& RetryPolicyDebugger::instance() { static RetryPolicyDebugger i; return i; }
void RetryPolicyDebugger::initialize() { enabled_ = true; clear(); }
void RetryPolicyDebugger::shutdown() { enabled_ = false; }
bool RetryPolicyDebugger::isEnabled() const { return enabled_; }
void RetryPolicyDebugger::registerPolicy(const std::string& name, size_t max, std::chrono::milliseconds delay) { policies_[name] = {max, delay, 0, 0}; }
void RetryPolicyDebugger::recordAttempt(const std::string& policy, size_t attempt, std::chrono::milliseconds delay, bool ok) {
    if (!enabled_) return;
    history_.push_back({policy, attempt, delay, std::chrono::system_clock::now(), ok});
    auto it = policies_.find(policy); if (it != policies_.end()) { it->second.totalAttempts++; if (ok) it->second.successes++; }
}
auto RetryPolicyDebugger::getHistory() const -> std::vector<RetryAttempt> { return history_; }
size_t RetryPolicyDebugger::getTotalAttempts(const std::string& policy) const { auto it = policies_.find(policy); return it != policies_.end() ? it->second.totalAttempts : 0; }
double RetryPolicyDebugger::getSuccessRate(const std::string& policy) const { auto it = policies_.find(policy); if (!it || it->second.totalAttempts == 0) return 0.0; return (double)it->second.successes / it->second.totalAttempts * 100.0; }
bool RetryPolicyDebugger::exceededMaxAttempts(const std::string& policy) const { auto it = policies_.find(policy); return it != policies_.end() && it->second.totalAttempts >= it->second.maxAttempts; }
void RetryPolicyDebugger::clear() { policies_.clear(); history_.clear(); }
}''')

# 10. websocket_debugger
w(f"{INC}/websocket_debugger.h", '''#pragma once
#include <string>
#include <vector>
#include <chrono>
#include <cstdint>
namespace nexus::utility::network {
class WebsocketDebugger {
public:
    struct FrameRecord { bool isText; size_t size; std::chrono::system_clock::time_point timestamp; bool isControl; std::string controlType; };
    struct SessionStats { std::string endpoint; std::chrono::system_clock::time_point connectTime; std::chrono::microseconds duration{0}; size_t framesSent = 0; size_t framesReceived = 0; size_t bytesSent = 0; size_t bytesReceived = 0; size_t pingsSent = 0; size_t pongsReceived = 0; bool disconnected = false; };
    static WebsocketDebugger& instance(); void initialize(); void shutdown(); bool isEnabled() const;
    void recordConnect(const std::string& endpoint);
    void recordDisconnect(const std::string& endpoint);
    void recordFrame(const std::string& endpoint, bool isText, size_t size, bool sent);
    void recordPing(const std::string& endpoint);
    void recordPong(const std::string& endpoint);
    SessionStats getSession(const std::string& endpoint) const;
    std::vector<SessionStats> getAllSessions() const;
    void clear();
private:
    WebsocketDebugger() = default; ~WebsocketDebugger() = default; bool enabled_ = false;
    std::unordered_map<std::string, SessionStats> sessions_;
};
}''')
w(f"{SRC}/websocket_debugger.cpp", '''#include "nexus/utility/network/websocket_debugger.h"
namespace nexus::utility::network {
WebsocketDebugger& WebsocketDebugger::instance() { static WebsocketDebugger i; return i; }
void WebsocketDebugger::initialize() { enabled_ = true; sessions_.clear(); }
void WebsocketDebugger::shutdown() { enabled_ = false; }
bool WebsocketDebugger::isEnabled() const { return enabled_; }
void WebsocketDebugger::recordConnect(const std::string& ep) { if (!enabled_) return; sessions_[ep] = {ep, std::chrono::system_clock::now(), std::chrono::microseconds(0), 0, 0, 0, 0, 0, 0, false}; }
void WebsocketDebugger::recordDisconnect(const std::string& ep) { auto it = sessions_.find(ep); if (it != sessions_.end()) { it->second.duration = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now() - it->second.connectTime); it->second.disconnected = true; } }
void WebsocketDebugger::recordFrame(const std::string& ep, bool isText, size_t sz, bool sent) { auto it = sessions_.find(ep); if (it != sessions_.end()) { if (sent) { it->second.framesSent++; it->second.bytesSent += sz; } else { it->second.framesReceived++; it->second.bytesReceived += sz; } } }
void WebsocketDebugger::recordPing(const std::string& ep) { auto it = sessions_.find(ep); if (it != sessions_.end()) it->second.pingsSent++; }
void WebsocketDebugger::recordPong(const std::string& ep) { auto it = sessions_.find(ep); if (it != sessions_.end()) it->second.pongsReceived++; }
auto WebsocketDebugger::getSession(const std::string& ep) const -> SessionStats { auto it = sessions_.find(ep); return it != sessions_.end() ? it->second : SessionStats{}; }
auto WebsocketDebugger::getAllSessions() const -> std::vector<SessionStats> { std::vector<SessionStats> r; for (auto& [_,s] : sessions_) r.push_back(s); return r; }
void WebsocketDebugger::clear() { sessions_.clear(); }
}''')

# 11. circuit_breaker_monitor
w(f"{INC}/circuit_breaker_monitor.h", '''#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <chrono>
namespace nexus::utility::network {
class CircuitBreakerMonitor {
public:
    enum class State { Closed, Open, HalfOpen };
    struct BreakerStats { std::string name; State state = State::Closed; size_t failureCount = 0; size_t successCount = 0;
        size_t openCount = 0; std::chrono::system_clock::time_point lastStateChange;
        std::chrono::milliseconds openDuration{0}; };
    static CircuitBreakerMonitor& instance(); void initialize(); void shutdown(); bool isEnabled() const;
    void registerBreaker(const std::string& name);
    void recordSuccess(const std::string& name);
    void recordFailure(const std::string& name);
    void recordOpen(const std::string& name);
    void recordHalfOpen(const std::string& name);
    void recordClose(const std::string& name);
    BreakerStats getStats(const std::string& name) const;
    std::vector<BreakerStats> getAllStats() const;
    bool isOpen(const std::string& name) const;
    void clear();
private:
    CircuitBreakerMonitor() = default; ~CircuitBreakerMonitor() = default; bool enabled_ = false;
    std::unordered_map<std::string, BreakerStats> breakers_;
};
}''')
w(f"{SRC}/circuit_breaker_monitor.cpp", '''#include "nexus/utility/network/circuit_breaker_monitor.h"
namespace nexus::utility::network {
CircuitBreakerMonitor& CircuitBreakerMonitor::instance() { static CircuitBreakerMonitor i; return i; }
void CircuitBreakerMonitor::initialize() { enabled_ = true; breakers_.clear(); }
void CircuitBreakerMonitor::shutdown() { enabled_ = false; }
bool CircuitBreakerMonitor::isEnabled() const { return enabled_; }
void CircuitBreakerMonitor::registerBreaker(const std::string& name) { breakers_[name] = {name, State::Closed, 0, 0, 0, std::chrono::system_clock::now(), std::chrono::milliseconds(0)}; }
void CircuitBreakerMonitor::recordSuccess(const std::string& name) { auto it = breakers_.find(name); if (it != breakers_.end()) it->second.successCount++; }
void CircuitBreakerMonitor::recordFailure(const std::string& name) { auto it = breakers_.find(name); if (it != breakers_.end()) it->second.failureCount++; }
void CircuitBreakerMonitor::recordOpen(const std::string& name) { auto it = breakers_.find(name); if (it != breakers_.end()) { it->second.state = State::Open; it->second.openCount++; it->second.lastStateChange = std::chrono::system_clock::now(); } }
void CircuitBreakerMonitor::recordHalfOpen(const std::string& name) { auto it = breakers_.find(name); if (it != breakers_.end()) { it->second.state = State::HalfOpen; it->second.lastStateChange = std::chrono::system_clock::now(); } }
void CircuitBreakerMonitor::recordClose(const std::string& name) { auto it = breakers_.find(name); if (it != breakers_.end()) { it->second.state = State::Closed; it->second.lastStateChange = std::chrono::system_clock::now(); } }
auto CircuitBreakerMonitor::getStats(const std::string& name) const -> BreakerStats { auto it = breakers_.find(name); return it != breakers_.end() ? it->second : BreakerStats{}; }
auto CircuitBreakerMonitor::getAllStats() const -> std::vector<BreakerStats> { std::vector<BreakerStats> r; for (auto& [_,s] : breakers_) r.push_back(s); return r; }
bool CircuitBreakerMonitor::isOpen(const std::string& name) const { auto it = breakers_.find(name); return it != breakers_.end() && it->second.state == State::Open; }
void CircuitBreakerMonitor::clear() { breakers_.clear(); }
}''')

print("Generated 11 network tools")
