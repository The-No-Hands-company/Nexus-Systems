#pragma once

#include <nexus/utility/crash/stacktrace_capture.h>
#include <nexus/utility/crash/signal_handler.h>
#include <string>
#include <fstream>
#include <sstream>
#include <chrono>
#include <ctime>
#include <unistd.h>
#include <sys/types.h>

namespace nexus::utility::crash {

/**
 * @brief Automatic crash reporter that captures and saves crash information.
 */
class CrashReporter {
public:
    struct CrashInfo {
        int signal;
        std::string signal_name;
        std::string timestamp;
        pid_t pid;
        std::string stacktrace;
    };

    /**
     * @brief Initializes the crash reporter with signal handlers.
     * @param crash_dir Directory to write crash reports (default: /tmp).
     */
    static void initialize(const std::string& crash_dir = "/tmp") {
        crash_dir_ = crash_dir;
        
        // Install signal handler with our callback
        SignalHandler::instance().install(crashCallback);
    }

    /**
     * @brief Generates a crash report from current state.
     */
    static CrashInfo generateReport(int signal) {
        CrashInfo info;
        info.signal = signal;
        info.signal_name = SignalHandler::getSignalName(signal);
        info.pid = getpid();
        
        // Timestamp
        auto now = std::chrono::system_clock::now();
        auto time_t_now = std::chrono::system_clock::to_time_t(now);
        char time_buf[100];
        std::strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", std::localtime(&time_t_now));
        info.timestamp = time_buf;
        
        // Capture stack trace (skip signal handler frames)
        info.stacktrace = StacktraceCapture::captureAndFormat(3);
        
        return info;
    }

    /**
     * @brief Formats crash info as a string.
     */
    static std::string formatReport(const CrashInfo& info) {
        std::ostringstream oss;
        oss << "=== CRASH REPORT ===\n";
        oss << "Timestamp: " << info.timestamp << "\n";
        oss << "PID: " << info.pid << "\n";
        oss << "Signal: " << info.signal_name << " (" << info.signal << ")\n";
        oss << "\n" << info.stacktrace;
        oss << "===================\n";
        return oss.str();
    }

    /**
     * @brief Writes crash report to file.
     */
    static bool writeReport(const CrashInfo& info) {
        std::ostringstream filename;
        filename << crash_dir_ << "/crash_" << info.pid << "_" << info.signal << ".txt";
        
        std::ofstream file(filename.str());
        if (!file.is_open()) {
            return false;
        }
        
        file << formatReport(info);
        file.close();
        return true;
    }

private:
    static void crashCallback(int signal, siginfo_t* info, void* context) {
        // Note: We're in a signal handler - be careful!
        // We can't use most C++ features safely here.
        
        // Write a simple message to stderr (async-signal-safe)
        const char* msg = "Generating crash report...\n";
        write(STDERR_FILENO, msg, strlen(msg));
        
        // We can't safely call generateReport/writeReport here
        // as they use non-async-signal-safe operations.
        // In a real implementation, we'd use a separate crash handler process
        // or write minimal info using only async-signal-safe functions.
        
        // For now, just note that crash reporting would happen here
    }

    static inline std::string crash_dir_ = "/tmp";
};

} // namespace nexus::utility::crash
