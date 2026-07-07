#pragma once

#include <string>
#include <atomic>
#include <mutex>
#include <cstdint>

namespace nexus::utility::simd {

/**
 * @brief Suggest optimal memory alignment for SIMD data types.
 */
class AlignmentOptimizer {
public:
    enum class Isa { Scalar, SSE, AVX, AVX512, NEON };

    static AlignmentOptimizer& instance() {
        static AlignmentOptimizer inst;
        return inst;
    }

    void initialize(const std::string& config = "") {
        std::lock_guard<std::mutex> lk(mutex_);
        config_ = config;
        enabled_ = true;
    }
    void shutdown() { enabled_ = false; }
    bool isEnabled() const { return enabled_.load(); }

    /// Recommended alignment (bytes) for a given instruction set.
    static std::size_t recommendedAlignment(Isa isa) {
        switch (isa) {
            case Isa::Scalar: return alignof(std::max_align_t);
            case Isa::SSE:    return 16;
            case Isa::AVX:    return 32;
            case Isa::AVX512: return 64;
            case Isa::NEON:   return 16;
        }
        return 16;
    }

    /// SIMD register width in bytes for an ISA.
    static std::size_t vectorWidth(Isa isa) {
        switch (isa) {
            case Isa::Scalar: return 8;
            case Isa::SSE:    return 16;
            case Isa::AVX:    return 32;
            case Isa::AVX512: return 64;
            case Isa::NEON:   return 16;
        }
        return 16;
    }

    static bool isAligned(const void* ptr, std::size_t alignment) {
        return alignment != 0 &&
               (reinterpret_cast<std::uintptr_t>(ptr) % alignment) == 0;
    }

    /// Padding to add to `count` elements of `elemSize` for full-width vectorization.
    static std::size_t recommendedPadding(std::size_t count, std::size_t elemSize, Isa isa) {
        std::size_t width = vectorWidth(isa);
        std::size_t lanes = elemSize ? width / elemSize : 0;
        if (lanes == 0) return 0;
        std::size_t rem = count % lanes;
        return rem == 0 ? 0 : (lanes - rem);
    }

    void recordCheck(bool aligned) {
        ++checks_;
        if (!aligned) ++misaligned_;
    }

    std::size_t misaligned() const { return misaligned_.load(); }

    void reset() { checks_ = 0; misaligned_ = 0; }

private:
    AlignmentOptimizer() = default;
    ~AlignmentOptimizer() = default;
    AlignmentOptimizer(const AlignmentOptimizer&) = delete;
    AlignmentOptimizer& operator=(const AlignmentOptimizer&) = delete;

    mutable std::mutex mutex_;
    std::atomic<bool> enabled_{false};
    std::string config_;
    std::atomic<std::size_t> checks_{0};
    std::atomic<std::size_t> misaligned_{0};
};

} // namespace nexus::utility::simd
