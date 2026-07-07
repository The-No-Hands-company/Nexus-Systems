# Voltron 3D Utility Toolkit

## Overview

Professional-grade diagnostic and development utilities for Voltron 3D. This comprehensive toolkit provides robust debugging, profiling, error handling, and system monitoring capabilities essential for enterprise-level C++ development.

## Categories

### 1. Memory Debugging (`include/voltron/utility/memory/`)

- **memory_tracker.h** - Track all allocations/deallocations with stack traces
- **smart_ptr_debug.h** - Enhanced smart pointers with lifecycle logging
- **allocation_guard.h** - RAII-based allocation scope tracking
- **buffer_overflow_detector.h** - Canary-based buffer overflow detection
- **double_free_detector.h** - Catch double-free errors

**Usage Example:**
```cpp
#include <voltron/utility/memory/memory_tracker.h>

void myFunction() {
    // Track memory allocations in this scope
    using namespace voltron::utility::memory;
    
    MemoryTracker::instance().recordAllocation(ptr, size, "MyClass");
    
    // At program exit, check for leaks
    auto leaks = MemoryTracker::instance().detectLeaks();
    if (!leaks.empty()) {
        MemoryTracker::instance().printReport(std::cerr);
    }
}
```

### 2. Crash Handling (`include/voltron/utility/crash/`)

- **stacktrace_capture.h** - C++23 stack trace capture and formatting
- **signal_handler.h** - Handle SIGSEGV, SIGABRT, SIGFPE with diagnostics

**Usage Example:**
```cpp
#include <voltron/utility/crash/signal_handler.h>

int main() {
    using namespace voltron::utility::crash;
    
    // Install crash handlers
    SignalHandler::instance().installHandlers();
    
    // Set callback for crash reporting
    SignalHandler::instance().setCrashCallback([](int sig, const std::string& info) {
        std::cerr << "Application crashed! Signal: " << info << "\n";
        // Save crash dump, notify user, etc.
    });
    
    // Your application code...
}
```

### 3. Logging Infrastructure (`include/voltron/utility/logging/`)

- **logger.h** - Multi-level, thread-safe logging
- **log_macros.h** - Convenient macros with automatic source location

**Usage Example:**
```cpp
#include <voltron/utility/logging/log_macros.h>

void processData() {
    VOLTRON_LOG_INFO("Starting data processing");
    VOLTRON_LOG_DEBUG_FMT("Processing " << count << " items");
    
    if (error) {
        VOLTRON_LOG_ERROR("Failed to process data");
    }
}

// Configure logger
voltron::utility::logging::Logger::instance().setLogLevel(LogLevel::Debug);
voltron::utility::logging::Logger::instance().addFileOutput("voltron.log");
```

### 4. Profiling (`include/voltron/utility/profiling/`)

- **scoped_timer.h** - RAII-based timing blocks

**Usage Example:**
```cpp
#include <voltron/utility/profiling/scoped_timer.h>

void expensiveOperation() {
    VOLTRON_PROFILE_FUNCTION();  // Automatically times function
    
    {
        VOLTRON_PROFILE_SCOPE("Inner loop");
        // ... code to profile
    }
}
```

### 5. Assertions & Contracts (`include/voltron/utility/assertions/`)

- **assert_enhanced.h** - Assertions with stack traces
- **precondition.h** - Function precondition validation

**Usage Example:**
```cpp
#include <voltron/utility/assertions/assert_enhanced.h>
#include <voltron/utility/assertions/precondition.h>

float divide(float a, float b) {
    VOLTRON_PRECONDITION(b != 0.0f, "Divisor cannot be zero");
    return a / b;
}

void processVector(const Vector3& v) {
    VOLTRON_ASSERT(v.length() > 0, "Vector must be non-zero");
    // ...
}
```

### 6. Concurrency Debugging (`include/voltron/utility/concurrency/`)

- **thread_tracker.h** - Track thread creation/destruction
- **mutex_wrapper.h** - Timed locks with contention reporting

**Usage Example:**
```cpp
#include <voltron/utility/concurrency/thread_tracker.h>
#include <voltron/utility/concurrency/mutex_wrapper.h>

// Track threads
TrackedThread worker("WorkerThread", []() {
    // Thread work...
});

// Monitor mutex contention
MutexWrapper<std::mutex> myMutex("DataMutex");
std::lock_guard<MutexWrapper<std::mutex>> lock(myMutex);
```

### 7. Error Handling (`include/voltron/utility/error/`)

- **expected_debug.h** - Enhanced std::expected with diagnostics
- **exception_context.h** - Attach context data to exceptions

**Usage Example:**
```cpp
#include <voltron/utility/error/expected_debug.h>
#include <voltron/utility/error/exception_context.h>

ExpectedDebug<int, std::string> parseValue(const std::string& str) {
    if (str.empty()) {
        return ExpectedDebug<int, std::string>("Empty string");
    }
    return std::stoi(str);
}

// Contextual exceptions
throw ContextualException("Failed to load file")
    .context().addContext("filename", filename)
    .context().addContext("line_number", line);
```

### 8. Debugging Helpers (`include/voltron/utility/debug/`)

- **debugger_detection.h** - Detect if running under debugger
- **hex_dumper.h** - Hexadecimal memory dumps

**Usage Example:**
```cpp
#include <voltron/utility/debug/debugger_detection.h>
#include <voltron/utility/debug/hex_dumper.h>

if (DebuggerDetection::isDebuggerPresent()) {
    VOLTRON_DEBUGBREAK();  // Break into debugger
}

// Dump memory contents
HexDumper::dumpWithAscii(buffer, size, std::cout);
```

### 9. System Integration (`include/voltron/utility/system/`)

- **build_info.h** - Embed build metadata

**Usage Example:**
```cpp
#include <voltron/utility/system/build_info.h>

BuildInfo::printAll(std::cout);
// Output:
// === Voltron 3D Build Information ===
// Version:   0.1.0-dev
// Built:     Dec  5 2025 14:30:00
// Compiler:  GCC 13.2.0
// Platform:  Linux
// =====================================
```

## Build Integration

Add to your project's `CMakeLists.txt`:

```cmake
# Add utility module
add_subdirectory(src/utility)

# Link to your target
target_link_libraries(your_target PRIVATE voltron::utility)
```

## Debug Macros

The utility module defines several debug macros that are automatically enabled in Debug builds:

- `VOLTRON_ENABLE_ASSERTS` - Enable enhanced assertions
- `VOLTRON_ENABLE_CONTRACTS` - Enable precondition/postcondition checks
- `VOLTRON_DEBUG_SMART_PTRS` - Log smart pointer lifecycle
- `VOLTRON_DEBUG_ALLOCATIONS` - Track allocation scopes
- `VOLTRON_DEBUG_MUTEX_CONTENTION` - Report mutex contention
- `VOLTRON_ENABLE_DEBUG_BREAKS` - Enable debugger breakpoints

## Philosophy

This utility toolkit follows Voltron 3D's core principles:

1. **Quality over Speed** - Comprehensive diagnostics prevent bugs
2. **Solo Developer Support** - Easy-to-use tools for debugging alone
3. **Professional-Grade** - Enterprise-level robustness
4. **Modular Architecture** - Clean dependencies, no coupling

## Recommended Usage During Development

### Phase 0-1 (Setup & Math Foundation)
- Use `VOLTRON_ASSERT` extensively in math code
- Enable `MemoryTracker` to catch leaks early
- Use `ScopedTimer` to profile math operations

### Phase 2-5 (Scene Graph & Geometry)
- Use `VOLTRON_PRECONDITION` for mesh operations
- Enable `ThreadTracker` when adding concurrency
- Use `BufferOverflowDetector` for dynamic arrays

### Phase 6+ (Gizmos & UI)
- Use `SignalHandler` to catch crashes during interaction
- Enable logging with `VOLTRON_LOG_*` macros
- Use `DebuggerDetection` for conditional breakpoints

## Performance Impact

- **Debug builds**: Full diagnostics enabled, ~10-20% overhead
- **Release builds**: Most utilities compiled out, <1% overhead
- **Memory tracking**: ~100 bytes per allocation
- **Logging**: Async mode available for minimal impact

## Future Additions

See the comprehensive list in the original specification for 80+ planned utilities across 18 categories. Current implementation provides the essential foundation for MVP development.

---

**Remember**: These utilities are tools to help you build Voltron 3D reliably. Use them liberally during development, profile performance later (Phase 10).
