#pragma once
#include <string>
#include <vector>
#include <chrono>
#include <cstdint>
#include <unordered_map>
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
} // namespace nexus::utility::network