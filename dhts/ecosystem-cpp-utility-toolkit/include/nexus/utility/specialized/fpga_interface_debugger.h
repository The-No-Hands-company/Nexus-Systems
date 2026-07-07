#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <atomic>
#include <mutex>
#include <cstdint>

namespace nexus::utility::specialized {

/**
 * @brief Debug FPGA interfaces: track bitstream loads and register reads/writes.
 */
class FpgaInterfaceDebugger {
public:
    struct RegisterAccess {
        std::uint32_t address = 0;
        std::uint32_t value = 0;
        bool isWrite = false;
    };

    struct BitstreamLoad {
        std::string name;
        std::size_t sizeBytes = 0;
        bool success = false;
    };

    static FpgaInterfaceDebugger& instance() {
        static FpgaInterfaceDebugger inst;
        return inst;
    }

    void initialize(const std::string& config = "") {
        std::lock_guard<std::mutex> lk(mutex_);
        config_ = config;
        enabled_ = true;
    }
    void shutdown() { enabled_ = false; }
    bool isEnabled() const { return enabled_.load(); }

    void loadBitstream(const std::string& name, std::size_t sizeBytes, bool success) {
        std::lock_guard<std::mutex> lk(mutex_);
        loads_.push_back({name, sizeBytes, success});
        if (success) ++successfulLoads_;
    }

    void writeRegister(std::uint32_t address, std::uint32_t value) {
        std::lock_guard<std::mutex> lk(mutex_);
        accesses_.push_back({address, value, true});
        registers_[address] = value;
    }

    std::uint32_t readRegister(std::uint32_t address) {
        std::lock_guard<std::mutex> lk(mutex_);
        std::uint32_t value = registers_.count(address) ? registers_[address] : 0;
        accesses_.push_back({address, value, false});
        return value;
    }

    std::size_t writeCount() const {
        std::lock_guard<std::mutex> lk(mutex_);
        std::size_t n = 0;
        for (const auto& a : accesses_) if (a.isWrite) ++n;
        return n;
    }

    std::size_t readCount() const {
        std::lock_guard<std::mutex> lk(mutex_);
        std::size_t n = 0;
        for (const auto& a : accesses_) if (!a.isWrite) ++n;
        return n;
    }

    std::size_t bitstreamLoads() const {
        std::lock_guard<std::mutex> lk(mutex_);
        return loads_.size();
    }
    std::size_t successfulLoads() const { return successfulLoads_.load(); }

    std::vector<RegisterAccess> accessHistory() const {
        std::lock_guard<std::mutex> lk(mutex_);
        return accesses_;
    }

    void reset() {
        std::lock_guard<std::mutex> lk(mutex_);
        accesses_.clear();
        registers_.clear();
        loads_.clear();
        successfulLoads_ = 0;
    }

private:
    FpgaInterfaceDebugger() = default;
    ~FpgaInterfaceDebugger() = default;
    FpgaInterfaceDebugger(const FpgaInterfaceDebugger&) = delete;
    FpgaInterfaceDebugger& operator=(const FpgaInterfaceDebugger&) = delete;

    mutable std::mutex mutex_;
    std::atomic<bool> enabled_{false};
    std::string config_;
    std::vector<RegisterAccess> accesses_;
    std::unordered_map<std::uint32_t, std::uint32_t> registers_;
    std::vector<BitstreamLoad> loads_;
    std::atomic<std::size_t> successfulLoads_{0};
};

} // namespace nexus::utility::specialized
