#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <atomic>
#include <mutex>
#include <cstdint>

namespace nexus::utility::reversing {

/**
 * @brief Detect function hooks by comparing current prologue bytes to a baseline.
 *
 * Inline hooks typically overwrite the first bytes of a function with a jump
 * (e.g. 0xE9 rel32 or 0xFF 0x25). This tracks baselines and flags divergence.
 */
class HookDetector {
public:
    struct FunctionBaseline {
        std::string name;
        std::uint64_t address = 0;
        std::vector<std::uint8_t> prologue;
        bool hooked = false;
    };

    static HookDetector& instance() {
        static HookDetector inst;
        return inst;
    }

    void initialize(const std::string& config = "") {
        std::lock_guard<std::mutex> lk(mutex_);
        config_ = config;
        enabled_ = true;
    }
    void shutdown() { enabled_ = false; }
    bool isEnabled() const { return enabled_.load(); }

    void registerFunction(const std::string& name, std::uint64_t address,
                          const std::uint8_t* prologue, std::size_t len) {
        std::lock_guard<std::mutex> lk(mutex_);
        baselines_[name] = {name, address,
                            std::vector<std::uint8_t>(prologue, prologue + len), false};
    }

    /// Heuristic: are these bytes a jump/detour typical of an inline hook?
    static bool looksLikeJump(const std::uint8_t* bytes, std::size_t len) {
        if (len >= 1 && bytes[0] == 0xE9) return true;          // jmp rel32
        if (len >= 1 && bytes[0] == 0xEB) return true;          // jmp rel8
        if (len >= 2 && bytes[0] == 0xFF && bytes[1] == 0x25) return true; // jmp [rip+x]
        if (len >= 2 && bytes[0] == 0x68) return true;          // push imm (push/ret)
        return false;
    }

    /// Compare current bytes with the baseline; returns true if a hook is detected.
    bool checkFunction(const std::string& name, const std::uint8_t* current, std::size_t len) {
        std::lock_guard<std::mutex> lk(mutex_);
        auto it = baselines_.find(name);
        if (it == baselines_.end()) return false;
        auto& b = it->second;
        bool changed = false;
        std::size_t n = std::min(len, b.prologue.size());
        for (std::size_t i = 0; i < n; ++i)
            if (current[i] != b.prologue[i]) { changed = true; break; }
        b.hooked = changed && looksLikeJump(current, len);
        if (b.hooked) ++detections_;
        return b.hooked;
    }

    std::vector<FunctionBaseline> hookedFunctions() const {
        std::lock_guard<std::mutex> lk(mutex_);
        std::vector<FunctionBaseline> out;
        for (const auto& [n, b] : baselines_) if (b.hooked) out.push_back(b);
        return out;
    }

    std::size_t detections() const { return detections_.load(); }

    void reset() {
        std::lock_guard<std::mutex> lk(mutex_);
        baselines_.clear();
        detections_ = 0;
    }

private:
    HookDetector() = default;
    ~HookDetector() = default;
    HookDetector(const HookDetector&) = delete;
    HookDetector& operator=(const HookDetector&) = delete;

    mutable std::mutex mutex_;
    std::atomic<bool> enabled_{false};
    std::string config_;
    std::unordered_map<std::string, FunctionBaseline> baselines_;
    std::atomic<std::size_t> detections_{0};
};

} // namespace nexus::utility::reversing
