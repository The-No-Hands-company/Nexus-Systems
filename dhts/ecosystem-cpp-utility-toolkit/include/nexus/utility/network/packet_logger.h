#pragma once
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
} // namespace nexus::utility::network