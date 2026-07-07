#pragma once

#include <cstddef>
#include <string>
#include <sstream>

// Linux perf event support
#ifdef __linux__
#include <linux/perf_event.h>
#include <sys/syscall.h>
#include <unistd.h>
#endif

namespace nexus::utility::profiling {

/**
 * @brief Cache performance profiling using perf events.
 */
class CacheProfiler {
public:
    struct CacheStats {
        uint64_t cache_references = 0;
        uint64_t cache_misses = 0;
        double miss_rate = 0.0;
    };

    /**
     * @brief Starts cache profiling.
     */
    static bool start() {
#ifdef __linux__
        // This is a simplified version - full implementation would use perf_event_open
        return true;
#else
        return false;
#endif
    }

    /**
     * @brief Stops cache profiling.
     */
    static void stop() {
        // Implementation would close perf event fds
    }

    /**
     * @brief Gets cache statistics.
     */
    static CacheStats getStats() {
        CacheStats stats;
        // Implementation would read from perf events
        return stats;
    }

    /**
     * @brief Generates cache report.
     */
    static std::string generateReport() {
        auto stats = getStats();
        std::ostringstream report;
        
        report << "=== Cache Profile Report ===\n";
        report << "Cache References: " << stats.cache_references << "\n";
        report << "Cache Misses: " << stats.cache_misses << "\n";
        report << "Miss Rate: " << (stats.miss_rate * 100.0) << "%\n";
        
        return report.str();
    }

    /**
     * @brief RAII cache profiler.
     */
    class ScopedCacheProfile {
    public:
        ScopedCacheProfile() {
            start();
        }

        ~ScopedCacheProfile() {
            stop();
        }

        CacheStats getStats() const {
            return CacheProfiler::getStats();
        }
    };
};

} // namespace nexus::utility::profiling
