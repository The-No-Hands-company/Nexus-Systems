#pragma once

#include <unordered_map>
#include <vector>
#include <string>
#include <mutex>
#include <chrono>
#include <sstream>

namespace nexus::utility::profiling {

/**
 * @brief Tracks allocation patterns and generates reports.
 */
class AllocationProfiler {
public:
    struct AllocationInfo {
        size_t size;
        std::chrono::steady_clock::time_point timestamp;
        std::string stack_trace; // Simplified - would use actual stacktrace
        bool freed = false;
    };

    struct AllocationStats {
        size_t total_allocations;
        size_t total_deallocations;
        size_t current_allocations;
        size_t peak_allocations;
        size_t total_bytes_allocated;
        size_t current_bytes;
        size_t peak_bytes;
        AllocationStats() : total_allocations(0), total_deallocations(0),
            current_allocations(0), peak_allocations(0),
            total_bytes_allocated(0), current_bytes(0), peak_bytes(0) {}
    };

    /**
     * @brief Records an allocation.
     */
    static void recordAllocation(void* ptr, size_t size, const std::string& location = "") {
        std::lock_guard lock(mutex_);
        
        AllocationInfo info;
        info.size = size;
        info.timestamp = std::chrono::steady_clock::now();
        info.stack_trace = location;
        
        allocations_[ptr] = info;
        
        stats_.total_allocations++;
        stats_.current_allocations++;
        stats_.total_bytes_allocated += size;
        stats_.current_bytes += size;
        
        if (stats_.current_allocations > stats_.peak_allocations) {
            stats_.peak_allocations = stats_.current_allocations;
        }
        if (stats_.current_bytes > stats_.peak_bytes) {
            stats_.peak_bytes = stats_.current_bytes;
        }
    }

    /**
     * @brief Records a deallocation.
     */
    static void recordDeallocation(void* ptr) {
        std::lock_guard lock(mutex_);
        
        auto it = allocations_.find(ptr);
        if (it != allocations_.end()) {
            it->second.freed = true;
            stats_.total_deallocations++;
            stats_.current_allocations--;
            stats_.current_bytes -= it->second.size;
        }
    }

    /**
     * @brief Gets current statistics.
     */
    static AllocationStats getStats() {
        std::lock_guard lock(mutex_);
        return stats_;
    }

    /**
     * @brief Generates a report of current allocations.
     */
    static std::string generateReport() {
        std::lock_guard lock(mutex_);
        std::ostringstream report;
        
        report << "=== Allocation Profiler Report ===\n";
        report << "Total Allocations: " << stats_.total_allocations << "\n";
        report << "Total Deallocations: " << stats_.total_deallocations << "\n";
        report << "Current Allocations: " << stats_.current_allocations << "\n";
        report << "Peak Allocations: " << stats_.peak_allocations << "\n";
        report << "Total Bytes Allocated: " << stats_.total_bytes_allocated << "\n";
        report << "Current Bytes: " << stats_.current_bytes << "\n";
        report << "Peak Bytes: " << stats_.peak_bytes << "\n";
        
        // List active allocations
        report << "\n=== Active Allocations ===\n";
        for (const auto& [ptr, info] : allocations_) {
            if (!info.freed) {
                report << "Ptr: " << ptr << ", Size: " << info.size << " bytes";
                if (!info.stack_trace.empty()) {
                    report << ", Location: " << info.stack_trace;
                }
                report << "\n";
            }
        }
        
        return report.str();
    }

    /**
     * @brief Detects memory leaks.
     */
    static std::vector<void*> detectLeaks() {
        std::lock_guard lock(mutex_);
        std::vector<void*> leaks;
        
        for (const auto& [ptr, info] : allocations_) {
            if (!info.freed) {
                leaks.push_back(ptr);
            }
        }
        
        return leaks;
    }

    /**
     * @brief Resets all statistics.
     */
    static void reset() {
        std::lock_guard lock(mutex_);
        allocations_.clear();
        stats_ = AllocationStats{};
    }

    /**
     * @brief Gets allocation size histogram.
     */
    static std::unordered_map<size_t, size_t> getSizeHistogram() {
        std::lock_guard lock(mutex_);
        std::unordered_map<size_t, size_t> histogram;
        
        for (const auto& [ptr, info] : allocations_) {
            if (!info.freed) {
                // Round to nearest power of 2 for bucketing
                size_t bucket = 1;
                while (bucket < info.size) bucket <<= 1;
                histogram[bucket]++;
            }
        }
        
        return histogram;
    }

private:
    static inline std::mutex mutex_;
    static inline std::unordered_map<void*, AllocationInfo> allocations_;
    static inline AllocationStats stats_;
};

} // namespace nexus::utility::profiling
