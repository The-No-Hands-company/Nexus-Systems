#pragma once

/// @file nexus_utility_toolkit.h
/// @brief Master include header for Nexus Systems utility toolkit
/// Include this single header to access all core utility categories

// ── Memory Debugging ────────────────────────────────────────────────────────
#include "nexus/utility/memory/memory_tracker.h"
#include "nexus/utility/memory/smart_ptr_debug.h"
#include "nexus/utility/memory/allocation_guard.h"
#include "nexus/utility/memory/buffer_overflow_detector.h"
#include "nexus/utility/memory/double_free_detector.h"

// ── Crash Handling ──────────────────────────────────────────────────────────
#include "nexus/utility/crash/stacktrace_capture.h"
#include "nexus/utility/crash/signal_handler.h"

// ── Logging ─────────────────────────────────────────────────────────────────
#include "nexus/utility/logging/logger.h"
#include "nexus/utility/logging/log_macros.h"

// ── Profiling ───────────────────────────────────────────────────────────────
#include "nexus/utility/profiling/scoped_timer.h"
#include "nexus/utility/profiling/function_tracer.h"

// ── Assertions & Contracts ──────────────────────────────────────────────────
#include "nexus/utility/assertions/assert_enhanced.h"
#include "nexus/utility/assertions/precondition.h"

// ── Concurrency ─────────────────────────────────────────────────────────────
#include "nexus/utility/concurrency/thread_tracker.h"
#include "nexus/utility/concurrency/mutex_wrapper.h"

// ── Error Handling ──────────────────────────────────────────────────────────
#include "nexus/utility/error/expected_debug.h"
#include "nexus/utility/error/exception_context.h"

// ── Debugging ───────────────────────────────────────────────────────────────
#include "nexus/utility/debug/debugger_detection.h"
#include "nexus/utility/debug/hex_dumper.h"

// ── System ──────────────────────────────────────────────────────────────────
#include "nexus/utility/system/build_info.h"

#include <ostream>
#include <iostream>

namespace nexus::utility {

/// @brief Initialize all utility systems (call at startup)
inline void initialize() {
    crash::SignalHandler::instance().installHandlers();

    logging::Logger::instance().setLogLevel(logging::LogLevel::Info);
    logging::Logger::instance().setConsoleOutput(true);
    logging::Logger::instance().setTimestamps(true);

    NEXUS_LOG_INFO("Nexus Systems Utility System initialized");
}

/// @brief Shutdown utility systems (call before exit)
inline void shutdown() {
    NEXUS_LOG_INFO("Shutting down Nexus Systems Utility System");

    auto leaks = memory::MemoryTracker::instance().detectLeaks();
    if (!leaks.empty()) {
        NEXUS_LOG_ERROR("Memory leaks detected!");
        memory::MemoryTracker::instance().printReport(std::cerr);
    }

    concurrency::ThreadTracker::instance().printReport(std::cout);

    logging::Logger::instance().flush();

    crash::SignalHandler::instance().restoreDefaultHandlers();
}

/// @brief Print full diagnostic report
inline void printDiagnosticReport(std::ostream& os = std::cout) {
    os << "\n";
    os << "╔════════════════════════════════════════╗\n";
    os << "║   NEXUS SYSTEMS DIAGNOSTIC REPORT        ║\n";
    os << "╚════════════════════════════════════════╝\n\n";

    system::BuildInfo::printAll(os);
    os << "\n";

    memory::MemoryTracker::instance().printReport(os);
    os << "\n";

    concurrency::ThreadTracker::instance().printReport(os);
    os << "\n";

    debug::DebuggerDetection::printStatus(os);
    os << "\n";
}

} // namespace nexus::utility
