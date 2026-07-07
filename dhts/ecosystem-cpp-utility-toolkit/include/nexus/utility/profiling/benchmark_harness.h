#pragma once

#include <functional>
#include <vector>
#include <chrono>
#include <algorithm>
#include <cmath>
#include <string>
#include <iostream>

namespace nexus::utility::profiling {

/**
 * @brief Statistical benchmarking harness for function performance testing.
 */
class BenchmarkHarness {
public:
    struct Result {
        std::string name;
        size_t iterations;
        int64_t min_ns;
        int64_t max_ns;
        double mean_ns;
        double stddev_ns;
        int64_t total_ns;
    };

    /**
     * @brief Runs a benchmark with the given function.
     * @param name Benchmark name.
     * @param func Function to benchmark.
     * @param iterations Number of iterations.
     * @param warmup_iterations Warmup iterations (not counted).
     */
    static Result run(
        const std::string& name,
        std::function<void()> func,
        size_t iterations = 1000,
        size_t warmup_iterations = 10
    ) {
        using Clock = std::chrono::high_resolution_clock;
        
        // Warmup
        for (size_t i = 0; i < warmup_iterations; ++i) {
            func();
        }
        
        // Benchmark
        std::vector<int64_t> timings;
        timings.reserve(iterations);
        
        for (size_t i = 0; i < iterations; ++i) {
            auto start = Clock::now();
            func();
            auto end = Clock::now();
            
            auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(
                end - start
            ).count();
            timings.push_back(elapsed);
        }
        
        // Calculate statistics
        Result result;
        result.name = name;
        result.iterations = iterations;
        result.min_ns = *std::min_element(timings.begin(), timings.end());
        result.max_ns = *std::max_element(timings.begin(), timings.end());
        
        // Mean
        int64_t sum = 0;
        for (auto t : timings) sum += t;
        result.mean_ns = static_cast<double>(sum) / iterations;
        result.total_ns = sum;
        
        // Standard deviation
        double variance = 0.0;
        for (auto t : timings) {
            double diff = t - result.mean_ns;
            variance += diff * diff;
        }
        result.stddev_ns = std::sqrt(variance / iterations);
        
        return result;
    }

    /**
     * @brief Prints benchmark result in a readable format.
     */
    static void printResult(const Result& result) {
        std::cout << "Benchmark: " << result.name << "\n";
        std::cout << "  Iterations: " << result.iterations << "\n";
        std::cout << "  Min:        " << result.min_ns << " ns\n";
        std::cout << "  Max:        " << result.max_ns << " ns\n";
        std::cout << "  Mean:       " << result.mean_ns << " ns\n";
        std::cout << "  StdDev:     " << result.stddev_ns << " ns\n";
        std::cout << "  Total:      " << result.total_ns << " ns\n";
    }
};

// Convenience macro
#define NEXUS_BENCHMARK(name, iterations, code) \
    nexus::utility::profiling::BenchmarkHarness::run(name, [&](){ code; }, iterations)

} // namespace nexus::utility::profiling
