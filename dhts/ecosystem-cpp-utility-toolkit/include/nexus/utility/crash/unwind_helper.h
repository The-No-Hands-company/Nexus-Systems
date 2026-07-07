#pragma once

#ifdef NEXUS_NO_STACKTRACE
#include "nexus/utility/compiler/stacktrace_compat.h"
#else
#include <stacktrace>
#endif
#include <string>
#include <vector>
#include <sstream>

namespace nexus::utility::crash {

/**
 * @brief Stack unwinding and frame inspection utilities.
 */
class UnwindHelper {
public:
    struct StackFrame {
        std::string function_name;
        std::string source_file;
        size_t line_number;
        void* address;
    };

    /**
     * @brief Captures current stack trace.
     */
    static std::vector<StackFrame> captureStack(size_t skip = 0, size_t max_frames = 64) {
        std::vector<StackFrame> frames;
        
        auto trace = nexus::utility::stacktrace::current(skip, max_frames);
        
        for (const auto& entry : trace) {
            StackFrame frame;
            frame.function_name = entry.description();
            frame.source_file = entry.source_file();
            frame.line_number = entry.source_line();
            frame.address = reinterpret_cast<void*>(entry.native_handle());
            frames.push_back(frame);
        }
        
        return frames;
    }

    /**
     * @brief Formats stack frames as string.
     */
    static std::string formatStack(const std::vector<StackFrame>& frames) {
        std::ostringstream oss;
        
        for (size_t i = 0; i < frames.size(); ++i) {
            oss << "#" << i << " ";
            oss << frames[i].function_name;
            
            if (!frames[i].source_file.empty()) {
                oss << " at " << frames[i].source_file;
                if (frames[i].line_number > 0) {
                    oss << ":" << frames[i].line_number;
                }
            }
            
            oss << " [" << frames[i].address << "]";
            oss << "\n";
        }
        
        return oss.str();
    }

    /**
     * @brief Gets function name at specific depth.
     */
    static std::string getFunctionAtDepth(size_t depth) {
        auto frames = captureStack(depth, 1);
        if (!frames.empty()) {
            return frames[0].function_name;
        }
        return "unknown";
    }

    /**
     * @brief Counts stack depth.
     */
    static size_t getStackDepth() {
        return nexus::utility::stacktrace::current().size();
    }

    /**
     * @brief Checks if function is in call stack.
     */
    static bool isInCallStack(const std::string& function_name) {
        auto frames = captureStack();
        for (const auto& frame : frames) {
            if (frame.function_name.find(function_name) != std::string::npos) {
                return true;
            }
        }
        return false;
    }
};

} // namespace nexus::utility::crash
