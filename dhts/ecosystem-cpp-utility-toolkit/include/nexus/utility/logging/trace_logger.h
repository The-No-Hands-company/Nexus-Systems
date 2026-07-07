#pragma once

#include <string>
#include <vector>
#include <mutex>
#include <chrono>
#include <sstream>
#include <source_location>

namespace nexus::utility::logging {

/**
 * @brief Function entry/exit tracing and call graph generation.
 */
class TraceLogger {
public:
    struct TraceEvent {
        std::string function_name;
        std::string file;
        size_t line;
        std::chrono::steady_clock::time_point timestamp;
        bool is_entry;
        size_t depth;
    };

    /**
     * @brief Records function entry.
     */
    static void recordEntry(const std::source_location& loc = std::source_location::current()) {
        std::lock_guard lock(mutex_);
        
        TraceEvent event;
        event.function_name = loc.function_name();
        event.file = loc.file_name();
        event.line = loc.line();
        event.timestamp = std::chrono::steady_clock::now();
        event.is_entry = true;
        event.depth = current_depth_++;
        
        trace_.push_back(event);
    }

    /**
     * @brief Records function exit.
     */
    static void recordExit(const std::source_location& loc = std::source_location::current()) {
        std::lock_guard lock(mutex_);
        
        if (current_depth_ > 0) current_depth_--;
        
        TraceEvent event;
        event.function_name = loc.function_name();
        event.file = loc.file_name();
        event.line = loc.line();
        event.timestamp = std::chrono::steady_clock::now();
        event.is_entry = false;
        event.depth = current_depth_;
        
        trace_.push_back(event);
    }

    /**
     * @brief Gets full trace.
     */
    static std::vector<TraceEvent> getTrace() {
        std::lock_guard lock(mutex_);
        return trace_;
    }

    /**
     * @brief Generates call graph.
     */
    static std::string generateCallGraph() {
        std::lock_guard lock(mutex_);
        std::ostringstream graph;
        
        graph << "=== Call Graph ===\n";
        for (const auto& event : trace_) {
            std::string indent(event.depth * 2, ' ');
            graph << indent;
            graph << (event.is_entry ? "→ " : "← ");
            graph << event.function_name << "\n";
        }
        
        return graph.str();
    }

    /**
     * @brief Resets trace.
     */
    static void reset() {
        std::lock_guard lock(mutex_);
        trace_.clear();
        current_depth_ = 0;
    }

    /**
     * @brief RAII function tracer.
     */
    class ScopedTrace {
    public:
        explicit ScopedTrace(const std::source_location& loc = std::source_location::current()) 
            : loc_(loc) {
            recordEntry(loc_);
        }

        ~ScopedTrace() {
            recordExit(loc_);
        }

    private:
        std::source_location loc_;
    };

private:
    static inline std::mutex mutex_;
    static inline std::vector<TraceEvent> trace_;
    static inline size_t current_depth_ = 0;
};

// Convenience macro
#define NEXUS_TRACE_FUNCTION() \
    nexus::utility::logging::TraceLogger::ScopedTrace __trace_##__LINE__

} // namespace nexus::utility::logging
