#pragma once

#include <cstdint>
#include <chrono>
#include <string>

// Platform-specific includes
#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
    #include <x86intrin.h>
    #define NEXUS_HAS_RDTSC 1
#endif

#if defined(__linux__)
    #include <sys/resource.h>
    #include <unistd.h>
#endif

namespace nexus::utility::profiling {

/**
 * @brief High-precision performance counter for CPU cycles and system metrics.
 */
class PerformanceCounter {
public:
    struct Metrics {
        uint64_t cpu_cycles = 0;
        int64_t wall_time_ns = 0;
        int64_t user_time_us = 0;
        int64_t system_time_us = 0;
        size_t memory_kb = 0;
    };

    PerformanceCounter() {
        reset();
    }

    void reset() {
        start_cycles_ = readCycles();
        start_time_ = std::chrono::high_resolution_clock::now();
        
#if defined(__linux__)
        getrusage(RUSAGE_SELF, &start_usage_);
#endif
    }

    Metrics measure() const {
        Metrics m;
        
        // CPU cycles
        m.cpu_cycles = readCycles() - start_cycles_;
        
        // Wall time
        auto now = std::chrono::high_resolution_clock::now();
        m.wall_time_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
            now - start_time_
        ).count();
        
#if defined(__linux__)
        struct rusage current_usage;
        getrusage(RUSAGE_SELF, &current_usage);
        
        // User time
        m.user_time_us = 
            (current_usage.ru_utime.tv_sec - start_usage_.ru_utime.tv_sec) * 1000000 +
            (current_usage.ru_utime.tv_usec - start_usage_.ru_utime.tv_usec);
        
        // System time
        m.system_time_us = 
            (current_usage.ru_stime.tv_sec - start_usage_.ru_stime.tv_sec) * 1000000 +
            (current_usage.ru_stime.tv_usec - start_usage_.ru_stime.tv_usec);
        
        // Memory (RSS in KB)
        m.memory_kb = current_usage.ru_maxrss;
#endif
        
        return m;
    }

    /**
     * @brief Reads CPU cycle counter (platform-specific).
     */
    static uint64_t readCycles() {
#if defined(NEXUS_HAS_RDTSC)
        return __rdtsc();
#else
        // Fallback: use high-resolution clock as approximation
        auto now = std::chrono::high_resolution_clock::now();
        return static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                now.time_since_epoch()
            ).count()
        );
#endif
    }

private:
    uint64_t start_cycles_ = 0;
    std::chrono::high_resolution_clock::time_point start_time_;
    
#if defined(__linux__)
    struct rusage start_usage_;
#endif
};

} // namespace nexus::utility::profiling
