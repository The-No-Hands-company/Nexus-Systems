#pragma once

#include <string>
#include <vector>
#include <atomic>
#include <mutex>
#include <cstdint>

#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
#define NEXUS_SIMD_X86 1
#if defined(_MSC_VER)
#include <intrin.h>
#else
#include <cpuid.h>
#endif
#endif

namespace nexus::utility::simd {

/**
 * @brief Detect CPU SIMD feature support at runtime.
 */
class CpuFeatureDetector {
public:
    struct Features {
        bool sse = false, sse2 = false, sse3 = false, ssse3 = false;
        bool sse4_1 = false, sse4_2 = false;
        bool avx = false, avx2 = false, avx512f = false;
        bool fma = false, neon = false;
    };

    static CpuFeatureDetector& instance() {
        static CpuFeatureDetector inst;
        return inst;
    }

    void initialize(const std::string& config = "") {
        std::lock_guard<std::mutex> lk(mutex_);
        features_ = detect();
        config_ = config;
        enabled_ = true;
    }
    void shutdown() { enabled_ = false; }
    bool isEnabled() const { return enabled_.load(); }

    static Features detect() {
        Features f;
#if defined(NEXUS_SIMD_X86)
        std::uint32_t regs[4] = {0, 0, 0, 0};
        cpuid(1, 0, regs);
        f.sse    = (regs[3] & (1u << 25)) != 0;
        f.sse2   = (regs[3] & (1u << 26)) != 0;
        f.sse3   = (regs[2] & (1u << 0))  != 0;
        f.ssse3  = (regs[2] & (1u << 9))  != 0;
        f.sse4_1 = (regs[2] & (1u << 19)) != 0;
        f.sse4_2 = (regs[2] & (1u << 20)) != 0;
        f.fma    = (regs[2] & (1u << 12)) != 0;
        f.avx    = (regs[2] & (1u << 28)) != 0;
        std::uint32_t regs7[4] = {0, 0, 0, 0};
        cpuid(7, 0, regs7);
        f.avx2    = (regs7[1] & (1u << 5))  != 0;
        f.avx512f = (regs7[1] & (1u << 16)) != 0;
#elif defined(__ARM_NEON) || defined(__aarch64__)
        f.neon = true;
#endif
        return f;
    }

    Features features() const {
        std::lock_guard<std::mutex> lk(mutex_);
        return features_;
    }

    bool hasSSE() const { return features().sse; }
    bool hasAVX() const { return features().avx; }
    bool hasAVX2() const { return features().avx2; }
    bool hasAVX512() const { return features().avx512f; }
    bool hasNEON() const { return features().neon; }

    /// Highest available x86 vector width in bytes (0 for none).
    std::size_t maxVectorWidth() const {
        Features f = features();
        if (f.avx512f) return 64;
        if (f.avx || f.avx2) return 32;
        if (f.sse || f.sse2) return 16;
        if (f.neon) return 16;
        return 0;
    }

    void reset() {
        std::lock_guard<std::mutex> lk(mutex_);
        features_ = detect();
    }

private:
    CpuFeatureDetector() : features_(detect()) {}
    ~CpuFeatureDetector() = default;
    CpuFeatureDetector(const CpuFeatureDetector&) = delete;
    CpuFeatureDetector& operator=(const CpuFeatureDetector&) = delete;

#if defined(NEXUS_SIMD_X86)
    static void cpuid(std::uint32_t leaf, std::uint32_t subleaf, std::uint32_t out[4]) {
#if defined(_MSC_VER)
        int regs[4];
        __cpuidex(regs, static_cast<int>(leaf), static_cast<int>(subleaf));
        out[0] = regs[0]; out[1] = regs[1]; out[2] = regs[2]; out[3] = regs[3];
#else
        __cpuid_count(leaf, subleaf, out[0], out[1], out[2], out[3]);
#endif
    }
#endif

    mutable std::mutex mutex_;
    std::atomic<bool> enabled_{false};
    std::string config_;
    Features features_;
};

} // namespace nexus::utility::simd
