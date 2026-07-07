#pragma once

#include <sys/resource.h>
#include <unistd.h>
#include <csignal>
#include <string>
#include <fstream>

namespace nexus::utility::crash {

/**
 * @brief Core dump configuration and generation helpers.
 */
class CoreDumpHelper {
public:
    /**
     * @brief Enables core dumps.
     */
    static bool enableCoreDumps() {
        struct rlimit limit;
        limit.rlim_cur = RLIM_INFINITY;
        limit.rlim_max = RLIM_INFINITY;
        return setrlimit(RLIMIT_CORE, &limit) == 0;
    }

    /**
     * @brief Disables core dumps.
     */
    static bool disableCoreDumps() {
        struct rlimit limit;
        limit.rlim_cur = 0;
        limit.rlim_max = 0;
        return setrlimit(RLIMIT_CORE, &limit) == 0;
    }

    /**
     * @brief Gets current core dump size limit.
     */
    static rlim_t getCoreDumpLimit() {
        struct rlimit limit;
        if (getrlimit(RLIMIT_CORE, &limit) == 0) {
            return limit.rlim_cur;
        }
        return 0;
    }

    /**
     * @brief Sets core dump size limit.
     */
    static bool setCoreDumpLimit(rlim_t bytes) {
        struct rlimit limit;
        if (getrlimit(RLIMIT_CORE, &limit) != 0) {
            return false;
        }
        
        limit.rlim_cur = bytes;
        if (bytes > limit.rlim_max) {
            limit.rlim_max = bytes;
        }
        
        return setrlimit(RLIMIT_CORE, &limit) == 0;
    }

    /**
     * @brief Triggers a core dump programmatically.
     */
    static void triggerCoreDump() {
        // Enable core dumps first
        enableCoreDumps();
        
        // Trigger SIGABRT which generates a core dump
        std::raise(SIGABRT);
    }

    /**
     * @brief Checks if core dumps are enabled.
     */
    static bool areCoreDumpsEnabled() {
        return getCoreDumpLimit() > 0;
    }

    /**
     * @brief Gets core dump file pattern.
     */
    static std::string getCoreDumpPattern() {
        // On Linux, read from /proc/sys/kernel/core_pattern
        std::ifstream file("/proc/sys/kernel/core_pattern");
        if (file.is_open()) {
            std::string pattern;
            std::getline(file, pattern);
            return pattern;
        }
        return "core";
    }

    /**
     * @brief RAII wrapper to enable core dumps in scope.
     */
    class ScopedCoreDump {
    public:
        ScopedCoreDump() : previous_limit_(getCoreDumpLimit()) {
            enableCoreDumps();
        }

        ~ScopedCoreDump() {
            setCoreDumpLimit(previous_limit_);
        }

    private:
        rlim_t previous_limit_;
    };
};

} // namespace nexus::utility::crash
