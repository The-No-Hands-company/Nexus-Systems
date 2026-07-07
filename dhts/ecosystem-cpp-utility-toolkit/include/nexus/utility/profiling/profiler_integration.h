#pragma once

#include <string>
#include <fstream>

// Valgrind callgrind annotations
#ifdef __has_include
  #if __has_include(<valgrind/callgrind.h>)
    #include <valgrind/callgrind.h>
    #define NEXUS_HAS_VALGRIND 1
  #endif
#endif

// gperftools profiler
#ifdef __has_include
  #if __has_include(<gperftools/profiler.h>)
    #include <gperftools/profiler.h>
    #define NEXUS_HAS_GPERFTOOLS 1
  #endif
#endif

namespace nexus::utility::profiling {

/**
 * @brief Integration with external profilers.
 */
class ProfilerIntegration {
public:
    /**
     * @brief Starts Valgrind callgrind instrumentation.
     */
    static void startCallgrind() {
#ifdef NEXUS_HAS_VALGRIND
        CALLGRIND_START_INSTRUMENTATION;
#endif
    }

    /**
     * @brief Stops Valgrind callgrind instrumentation.
     */
    static void stopCallgrind() {
#ifdef NEXUS_HAS_VALGRIND
        CALLGRIND_STOP_INSTRUMENTATION;
#endif
    }

    /**
     * @brief Dumps callgrind statistics.
     */
    static void dumpCallgrind() {
#ifdef NEXUS_HAS_VALGRIND
        CALLGRIND_DUMP_STATS;
#endif
    }

    /**
     * @brief Toggles callgrind collection.
     */
    static void toggleCallgrind() {
#ifdef NEXUS_HAS_VALGRIND
        CALLGRIND_TOGGLE_COLLECT;
#endif
    }

    /**
     * @brief Starts CPU profiling (gperftools).
     */
    static bool startCPUProfile(const std::string& filename) {
#ifdef NEXUS_HAS_GPERFTOOLS
        return ProfilerStart(filename.c_str()) != 0;
#else
        (void)filename;
        return false;
#endif
    }

    /**
     * @brief Stops CPU profiling (gperftools).
     */
    static void stopCPUProfile() {
#ifdef NEXUS_HAS_GPERFTOOLS
        ProfilerStop();
#endif
    }

    /**
     * @brief Checks if running under Valgrind.
     */
    static bool isRunningUnderValgrind() {
#ifdef NEXUS_HAS_VALGRIND
        return RUNNING_ON_VALGRIND != 0;
#else
        return false;
#endif
    }

    /**
     * @brief RAII wrapper for CPU profiling.
     */
    class ScopedCPUProfile {
    public:
        explicit ScopedCPUProfile(const std::string& filename) : active_(false) {
            active_ = startCPUProfile(filename);
        }

        ~ScopedCPUProfile() {
            if (active_) {
                stopCPUProfile();
            }
        }

        bool isActive() const { return active_; }

    private:
        bool active_;
    };

    /**
     * @brief RAII wrapper for callgrind instrumentation.
     */
    class ScopedCallgrind {
    public:
        ScopedCallgrind() {
            startCallgrind();
        }

        ~ScopedCallgrind() {
            stopCallgrind();
        }
    };

    /**
     * @brief Exports profiling data to JSON.
     */
    static bool exportProfileData(const std::string& filename, 
                                  const std::string& data) {
        std::ofstream file(filename);
        if (!file.is_open()) {
            return false;
        }
        file << data;
        return true;
    }
};

// Convenience macros
#define NEXUS_PROFILE_FUNCTION() \
    nexus::utility::profiling::ProfilerIntegration::ScopedCallgrind \
    __nexus_profile_##__LINE__

#define NEXUS_PROFILE_CPU(filename) \
    nexus::utility::profiling::ProfilerIntegration::ScopedCPUProfile \
    __nexus_cpu_profile_##__LINE__(filename)

} // namespace nexus::utility::profiling
