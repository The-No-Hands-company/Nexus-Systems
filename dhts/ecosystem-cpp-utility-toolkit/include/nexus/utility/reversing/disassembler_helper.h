#pragma once

#include <string>
#include <vector>
#include <sstream>
#include <iomanip>
#include <atomic>
#include <mutex>
#include <cstdint>

namespace nexus::utility::reversing {

/**
 * @brief Record decoded instructions with their addresses (disassembly helper).
 */
class DisassemblerHelper {
public:
    struct Instruction {
        std::uint64_t address = 0;
        std::vector<std::uint8_t> bytes;
        std::string mnemonic;
        std::string operands;
        std::size_t length() const { return bytes.size(); }
    };

    static DisassemblerHelper& instance() {
        static DisassemblerHelper inst;
        return inst;
    }

    void initialize(const std::string& config = "") {
        std::lock_guard<std::mutex> lk(mutex_);
        config_ = config;
        enabled_ = true;
    }
    void shutdown() { enabled_ = false; }
    bool isEnabled() const { return enabled_.load(); }

    void recordInstruction(std::uint64_t address, const std::vector<std::uint8_t>& bytes,
                           const std::string& mnemonic, const std::string& operands = "") {
        std::lock_guard<std::mutex> lk(mutex_);
        instructions_.push_back({address, bytes, mnemonic, operands});
    }

    const Instruction* atAddress(std::uint64_t address) const {
        std::lock_guard<std::mutex> lk(mutex_);
        for (const auto& insn : instructions_)
            if (insn.address == address) return &insn;
        return nullptr;
    }

    std::size_t count() const {
        std::lock_guard<std::mutex> lk(mutex_);
        return instructions_.size();
    }

    /// Total number of bytes decoded across all recorded instructions.
    std::size_t totalBytesDecoded() const {
        std::lock_guard<std::mutex> lk(mutex_);
        std::size_t n = 0;
        for (const auto& insn : instructions_) n += insn.bytes.size();
        return n;
    }

    std::string listing() const {
        std::lock_guard<std::mutex> lk(mutex_);
        std::ostringstream os;
        for (const auto& insn : instructions_) {
            os << std::hex << std::setw(16) << std::setfill('0') << insn.address
               << "  " << insn.mnemonic;
            if (!insn.operands.empty()) os << " " << insn.operands;
            os << "\n";
        }
        return os.str();
    }

    std::vector<Instruction> instructions() const {
        std::lock_guard<std::mutex> lk(mutex_);
        return instructions_;
    }

    void reset() {
        std::lock_guard<std::mutex> lk(mutex_);
        instructions_.clear();
    }

private:
    DisassemblerHelper() = default;
    ~DisassemblerHelper() = default;
    DisassemblerHelper(const DisassemblerHelper&) = delete;
    DisassemblerHelper& operator=(const DisassemblerHelper&) = delete;

    mutable std::mutex mutex_;
    std::atomic<bool> enabled_{false};
    std::string config_;
    std::vector<Instruction> instructions_;
};

} // namespace nexus::utility::reversing
