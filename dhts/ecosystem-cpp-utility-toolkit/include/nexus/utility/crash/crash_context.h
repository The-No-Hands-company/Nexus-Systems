#pragma once

#include <nexus/utility/crash/unwind_helper.h>
#include <string>
#include <vector>
#include <csignal>
#include <unistd.h>
#include <sys/utsname.h>
#include <chrono>
#include <sstream>
#include <fstream>

namespace nexus::utility::crash {

/**
 * @brief Comprehensive crash context capture.
 */
class CrashContext {
public:
    struct Context {
        // Process info
        pid_t process_id;
        std::string executable_path;
        
        // System info
        std::string hostname;
        std::string os_name;
        std::string os_version;
        
        // Crash info
        int signal_number;
        std::string signal_name;
        std::chrono::system_clock::time_point crash_time;
        
        // Stack trace
        std::vector<UnwindHelper::StackFrame> stack_frames;
        
        // Memory info
        size_t virtual_memory_size;
        size_t resident_set_size;
        
        // Thread info
        size_t thread_count;
    };

    /**
     * @brief Captures full crash context.
     */
    static Context capture(int signal = 0) {
        Context ctx;
        
        // Process info
        ctx.process_id = getpid();
        ctx.executable_path = getExecutablePath();
        
        // System info
        struct utsname sys_info;
        if (uname(&sys_info) == 0) {
            ctx.hostname = sys_info.nodename;
            ctx.os_name = sys_info.sysname;
            ctx.os_version = sys_info.release;
        }
        
        // Crash info
        ctx.signal_number = signal;
        ctx.signal_name = getSignalName(signal);
        ctx.crash_time = std::chrono::system_clock::now();
        
        // Stack trace
        ctx.stack_frames = UnwindHelper::captureStack(1);
        
        // Memory info
        auto [vsize, rss] = getMemoryInfo();
        ctx.virtual_memory_size = vsize;
        ctx.resident_set_size = rss;
        
        // Thread count
        ctx.thread_count = getThreadCount();
        
        return ctx;
    }

    /**
     * @brief Formats context as string.
     */
    static std::string format(const Context& ctx) {
        std::ostringstream oss;
        
        oss << "=== Crash Context ===\n";
        oss << "Process ID: " << ctx.process_id << "\n";
        oss << "Executable: " << ctx.executable_path << "\n";
        oss << "Hostname: " << ctx.hostname << "\n";
        oss << "OS: " << ctx.os_name << " " << ctx.os_version << "\n";
        oss << "Signal: " << ctx.signal_number << " (" << ctx.signal_name << ")\n";
        oss << "Virtual Memory: " << ctx.virtual_memory_size << " bytes\n";
        oss << "RSS: " << ctx.resident_set_size << " bytes\n";
        oss << "Threads: " << ctx.thread_count << "\n\n";
        
        oss << "=== Stack Trace ===\n";
        oss << UnwindHelper::formatStack(ctx.stack_frames);
        
        return oss.str();
    }

private:
    static std::string getExecutablePath() {
        char path[1024];
        ssize_t len = readlink("/proc/self/exe", path, sizeof(path) - 1);
        if (len != -1) {
            path[len] = '\0';
            return std::string(path);
        }
        return "unknown";
    }

    static std::string getSignalName(int signal) {
        switch (signal) {
            case SIGSEGV: return "SIGSEGV";
            case SIGABRT: return "SIGABRT";
            case SIGFPE: return "SIGFPE";
            case SIGILL: return "SIGILL";
            case SIGBUS: return "SIGBUS";
            default: return "UNKNOWN";
        }
    }

    static std::pair<size_t, size_t> getMemoryInfo() {
        std::ifstream status("/proc/self/status");
        std::string line;
        size_t vsize = 0, rss = 0;
        
        while (std::getline(status, line)) {
            if (line.starts_with("VmSize:")) {
                std::istringstream(line.substr(7)) >> vsize;
                vsize *= 1024; // Convert KB to bytes
            } else if (line.starts_with("VmRSS:")) {
                std::istringstream(line.substr(6)) >> rss;
                rss *= 1024;
            }
        }
        
        return {vsize, rss};
    }

    static size_t getThreadCount() {
        std::ifstream status("/proc/self/status");
        std::string line;
        
        while (std::getline(status, line)) {
            if (line.starts_with("Threads:")) {
                size_t count;
                std::istringstream(line.substr(8)) >> count;
                return count;
            }
        }
        
        return 1;
    }
};

} // namespace nexus::utility::crash
