#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <atomic>
#include <mutex>
#include <cstdint>

namespace nexus::utility::protocol {

/**
 * @brief Debug packet fragmentation and reassembly.
 */
class PacketFragmenterDebugger {
public:
    struct Fragment {
        std::uint32_t packetId = 0;
        std::size_t offset = 0;
        std::vector<std::uint8_t> data;
        bool lastFragment = false;
    };

    struct ReassemblyState {
        std::uint32_t packetId = 0;
        std::size_t receivedBytes = 0;
        std::size_t expectedBytes = 0;   // known once last fragment seen
        std::size_t fragmentCount = 0;
        bool complete = false;
    };

    static PacketFragmenterDebugger& instance() {
        static PacketFragmenterDebugger inst;
        return inst;
    }

    void initialize(const std::string& config = "") {
        std::lock_guard<std::mutex> lk(mutex_);
        config_ = config;
        enabled_ = true;
    }
    void shutdown() { enabled_ = false; }
    bool isEnabled() const { return enabled_.load(); }

    /// Fragment a payload into fragments of at most `mtu` bytes.
    std::vector<Fragment> fragment(std::uint32_t packetId,
                                   const std::vector<std::uint8_t>& payload,
                                   std::size_t mtu) {
        std::vector<Fragment> frags;
        if (mtu == 0) return frags;
        for (std::size_t off = 0; off < payload.size(); off += mtu) {
            std::size_t end = std::min(off + mtu, payload.size());
            Fragment f;
            f.packetId = packetId;
            f.offset = off;
            f.data.assign(payload.begin() + off, payload.begin() + end);
            f.lastFragment = (end == payload.size());
            frags.push_back(std::move(f));
        }
        std::lock_guard<std::mutex> lk(mutex_);
        fragmentsSent_ += frags.size();
        return frags;
    }

    /// Feed a received fragment into the reassembly buffer.
    ReassemblyState receiveFragment(const Fragment& f) {
        std::lock_guard<std::mutex> lk(mutex_);
        auto& s = states_[f.packetId];
        s.packetId = f.packetId;
        s.receivedBytes += f.data.size();
        ++s.fragmentCount;
        if (f.lastFragment) s.expectedBytes = f.offset + f.data.size();
        s.complete = (s.expectedBytes != 0 && s.receivedBytes >= s.expectedBytes);
        ++fragmentsReceived_;
        if (s.complete) ++reassembled_;
        return s;
    }

    bool isComplete(std::uint32_t packetId) const {
        std::lock_guard<std::mutex> lk(mutex_);
        auto it = states_.find(packetId);
        return it != states_.end() && it->second.complete;
    }

    std::size_t fragmentsSent() const { return fragmentsSent_.load(); }
    std::size_t fragmentsReceived() const { return fragmentsReceived_.load(); }
    std::size_t reassembled() const { return reassembled_.load(); }

    void reset() {
        std::lock_guard<std::mutex> lk(mutex_);
        states_.clear();
        fragmentsSent_ = 0;
        fragmentsReceived_ = 0;
        reassembled_ = 0;
    }

private:
    PacketFragmenterDebugger() = default;
    ~PacketFragmenterDebugger() = default;
    PacketFragmenterDebugger(const PacketFragmenterDebugger&) = delete;
    PacketFragmenterDebugger& operator=(const PacketFragmenterDebugger&) = delete;

    mutable std::mutex mutex_;
    std::atomic<bool> enabled_{false};
    std::string config_;
    std::unordered_map<std::uint32_t, ReassemblyState> states_;
    std::atomic<std::size_t> fragmentsSent_{0};
    std::atomic<std::size_t> fragmentsReceived_{0};
    std::atomic<std::size_t> reassembled_{0};
};

} // namespace nexus::utility::protocol
