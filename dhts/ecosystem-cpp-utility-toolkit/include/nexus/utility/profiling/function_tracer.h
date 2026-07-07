#pragma once

#include <unordered_map>
#include <string>
#include <mutex>
#include <chrono>
#include <sstream>
#include <vector>

namespace nexus::utility::profiling {

/**
 * @brief Automatic function instrumentation and profiling.
 */
class FunctionTracer {
public:
    struct FunctionStats {
        size_t call_count = 0;
        std::chrono::nanoseconds total_time{0};
        std::chrono::nanoseconds min_time{std::chrono::nanoseconds::max()};
        std::chrono::nanoseconds max_time{0};
    };

    /**
     * @brief Records function call.
     */
    static void recordCall(const std::string& function_name, std::chrono::nanoseconds duration) {
        std::lock_guard lock(mutex_);
        
        auto& stats = function_stats_[function_name];
        stats.call_count++;
        stats.total_time += duration;
        
        if (duration < stats.min_time) stats.min_time = duration;
        if (duration > stats.max_time) stats.max_time = duration;
    }

    /**
     * @brief Gets statistics for a function.
     */
    static FunctionStats getStats(const std::string& function_name) {
        std::lock_guard lock(mutex_);
        return function_stats_[function_name];
    }

    /**
     * @brief Gets all function statistics.
     */
    static std::unordered_map<std::string, FunctionStats> getAllStats() {
        std::lock_guard lock(mutex_);
        return function_stats_;
    }

    /**
     * @brief Identifies hot paths.
     */
    static std::vector<std::pair<std::string, FunctionStats>> getHotFunctions(size_t top_n = 10) {
        std::lock_guard lock(mutex_);
        
        std::vector<std::pair<std::string, FunctionStats>> functions;
        for (const auto& [name, stats] : function_stats_) {
            functions.emplace_back(name, stats);
        }
        
        // Sort by total time
        std::sort(functions.begin(), functions.end(),
            [](const auto& a, const auto& b) {
                return a.second.total_time > b.second.total_time;
            });
        
        if (functions.size() > top_n) {
            functions.resize(top_n);
        }
        
        return functions;
    }

    /**
     * @brief Generates profiling report.
     */
    static std::string generateReport() {
        std::lock_guard lock(mutex_);
        std::ostringstream report;
        
        report << "=== Function Profiling Report ===\n";
        report << "Total Functions: " << function_stats_.size() << "\n\n";
        
        auto hot_functions = getHotFunctions(20);
        report << "=== Top 20 Functions by Time ===\n";
        
        for (const auto& [name, stats] : hot_functions) {
            auto avg_time = stats.total_time / stats.call_count;
            report << name << ":\n";
            report << "  Calls: " << stats.call_count << "\n";
            report << "  Total: " << stats.total_time.count() << " ns\n";
            report << "  Avg: " << avg_time.count() << " ns\n";
            report << "  Min: " << stats.min_time.count() << " ns\n";
            report << "  Max: " << stats.max_time.count() << " ns\n";
        }
        
        return report.str();
    }

    /**
     * @brief Resets all statistics.
     */
    static void reset() {
        std::lock_guard lock(mutex_);
        function_stats_.clear();
    }

    /**
     * @brief RAII function timer.
     */
    class ScopedFunctionTimer {
    public:
        explicit ScopedFunctionTimer(const std::string& function_name)
            : function_name_(function_name),
              start_time_(std::chrono::steady_clock::now()) {}

        ~ScopedFunctionTimer() {
            auto end_time = std::chrono::steady_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(
                end_time - start_time_);
            recordCall(function_name_, duration);
        }

    private:
        std::string function_name_;
        std::chrono::steady_clock::time_point start_time_;
    };

private:
    static inline std::mutex mutex_;
    static inline std::unordered_map<std::string, FunctionStats> function_stats_;
};

// ── Convenience Macros ──────────────────────────────────────────────────────
#define NEXUS_PROFILE_FUNCTION() \
    nexus::utility::profiling::FunctionTracer::ScopedFunctionTimer \
    __function_timer_##__LINE__(__FUNCTION__)

#define NEXUS_PROFILE_SCOPE(name) \
    nexus::utility::profiling::FunctionTracer::ScopedFunctionTimer \
    __function_timer_##__LINE__(name)

} // namespace nexus::utility::profiling
