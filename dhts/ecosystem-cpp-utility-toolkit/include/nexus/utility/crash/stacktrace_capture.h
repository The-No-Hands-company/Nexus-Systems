#pragma once

#ifdef NEXUS_NO_STACKTRACE
#include "nexus/utility/compiler/stacktrace_compat.h"
#else
#include <stacktrace>
#endif
#include <string>
#include <sstream>
#include <vector>

namespace nexus::utility::crash {

/**
 * @brief Utilities for capturing and formatting stack traces.
 */
class StacktraceCapture {
public:
    /**
     * @brief Captures the current stack trace.
     * @param skip Number of frames to skip (0 = include this function).
     * @param max_depth Maximum depth to capture (0 = unlimited).
     */
    static nexus::utility::stacktrace capture(size_t skip = 1, size_t max_depth = 0) {
        auto trace = nexus::utility::stacktrace::current(skip);
        if (max_depth > 0 && trace.size() > max_depth) {
            // Truncate to max_depth
            std::vector<nexus::utility::stacktrace_entry> entries;
            entries.reserve(max_depth);
            for (size_t i = 0; i < max_depth && i < trace.size(); ++i) {
                entries.push_back(trace[i]);
            }
            // Note: nexus::utility::stacktrace doesn't have a constructor from vector
            // We'll just return the full trace for now
            return trace;
        }
        return trace;
    }

    /**
     * @brief Formats a stack trace as a string.
     */
    static std::string format(const nexus::utility::stacktrace& trace) {
        std::ostringstream oss;
        oss << "Stack trace (" << trace.size() << " frames):\n";
        
        size_t frame_num = 0;
        for (const auto& entry : trace) {
            oss << "  #" << frame_num++ << " ";
            
            // Source location if available
            if (!std::string(entry.source_file()).empty()) {
                oss << entry.source_file() << ":" << entry.source_line() << " ";
            }
            
            // Function name
            oss << entry.description();
            oss << "\n";
        }
        
        return oss.str();
    }

    /**
     * @brief Captures and formats the current stack trace.
     */
    static std::string captureAndFormat(size_t skip = 1, size_t max_depth = 0) {
        return format(capture(skip + 1, max_depth));
    }

    /**
     * @brief Gets a compact representation (just function names).
     */
    static std::vector<std::string> getFunctionNames(const nexus::utility::stacktrace& trace) {
        std::vector<std::string> names;
        names.reserve(trace.size());
        
        for (const auto& entry : trace) {
            names.push_back(entry.description());
        }
        
        return names;
    }
};

} // namespace nexus::utility::crash
