#pragma once

#include <cstddef>
#include <string>
#include <vector>
#include <mutex>
#include <unordered_map>
#ifdef NEXUS_NO_STACKTRACE
#include "nexus/utility/compiler/stacktrace_compat.h"
#else
#include <stacktrace>
#endif
#include <iostream>
#include <ostream>

namespace nexus::utility::memory {

struct AllocationStackTrace {
    nexus::utility::stacktrace trace;
    size_t size = 0;
    void* address = nullptr;
    std::string type_name;

    AllocationStackTrace() = default;
    AllocationStackTrace(void* addr, size_t sz, const char* type = "unknown");
};

struct MemoryStats {
    size_t total_allocations = 0;
    size_t total_deallocations = 0;
    size_t current_allocations = 0;
    size_t peak_allocations = 0;
    size_t total_bytes_allocated = 0;
    size_t total_bytes_deallocated = 0;
    size_t current_bytes = 0;
    size_t peak_bytes = 0;

    void recordAllocation(size_t size);
    void recordDeallocation(size_t size);
};

class MemoryTracker {
public:
    static MemoryTracker& instance();

    struct Config {
        bool capture_stack_traces = true;
        bool track_allocations = true;
        size_t stack_trace_depth = 10;
        size_t stack_trace_skip = 1;
    };

    void configure(const Config& config);
    Config getConfig() const;

    void recordAllocation(void* ptr, size_t size, const char* type_name = "unknown");
    void recordDeallocation(void* ptr);

    MemoryStats getStats() const;
    std::vector<AllocationStackTrace> detectLeaks() const;
    void printReport(std::ostream& os) const;
    void reset();

    // Direct enable/disable (updates config.track_allocations)
    void setEnabled(bool enabled);
    bool isEnabled() const;

private:
    MemoryTracker() = default;
    ~MemoryTracker();

    MemoryTracker(const MemoryTracker&) = delete;
    MemoryTracker& operator=(const MemoryTracker&) = delete;

    mutable std::mutex mutex_;
    std::unordered_map<void*, AllocationStackTrace> allocations_;
    MemoryStats stats_;
    Config config_;
};

class MemoryTrackingScope {
public:
    explicit MemoryTrackingScope(bool enable);
    ~MemoryTrackingScope();

private:
    bool previous_state_;
};

} // namespace nexus::utility::memory
