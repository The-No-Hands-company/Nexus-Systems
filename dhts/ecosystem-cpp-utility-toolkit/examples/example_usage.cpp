/**
 * @file example_usage.cpp
 * @brief Example demonstrating Nexus Systems utility toolkit usage
 *
 * This file shows how to use the various diagnostic utilities
 * during development of Nexus Systems.
 */

#include "nexus/utility/nexus_utility_toolkit.h"
#include <iostream>
#include <vector>

using namespace nexus::utility;

// Example 1: Memory tracking
class Mesh {
public:
    Mesh(size_t vertex_count) {
        memory::MemoryTracker::instance().recordAllocation(this, sizeof(Mesh), "Mesh");
        vertices_.resize(vertex_count);
        NEXUS_LOG_INFO_FMT("Created mesh with " << vertex_count << " vertices");
    }

    ~Mesh() {
        memory::MemoryTracker::instance().recordDeallocation(this);
        NEXUS_LOG_INFO("Mesh destroyed");
    }

private:
    std::vector<float> vertices_;
};

// Example 2: Assertions and preconditions
float safeDiv(float a, float b) {
    NEXUS_PRECONDITION(b != 0.0f, "Division by zero");
    return a / b;
}

// Example 3: Profiled function
void expensiveCalculation() {
    NEXUS_PROFILE_FUNCTION();

    {
        NEXUS_PROFILE_SCOPE("Inner loop");
        for (int i = 0; i < 1000000; ++i) {
            volatile double result = std::sin(i) * std::cos(i);
            (void)result;
        }
    }

    NEXUS_LOG_DEBUG("Expensive calculation complete");
}

// Example 4: Error handling with context
error::ExpectedDebug<int, std::string> parseInt(const std::string& str) {
    if (str.empty()) {
        return error::ExpectedDebug<int, std::string>("Empty string provided");
    }

    try {
        return std::stoi(str);
    } catch (const std::exception& e) {
        return error::ExpectedDebug<int, std::string>(
            std::string("Parse error: ") + e.what()
        );
    }
}

// Example 5: Thread tracking
void workerThread() {
    NEXUS_LOG_INFO("Worker thread started");

    // Simulate work
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    NEXUS_LOG_INFO("Worker thread finished");
}

// Example 6: Crash recovery
void demonstrateCrashHandler() {
    crash::SignalHandler::instance().setCrashCallback([](int sig, const std::string& info) {
        std::cerr << "CRASH DETECTED: " << info << "\n";
        std::cerr << "Saving emergency data...\n";
        // Save important state, notify user, etc.
    });

    // Uncomment to test crash handling (will terminate program)
    // int* bad_ptr = nullptr;
    // *bad_ptr = 42;  // SIGSEGV
}

int main() {
    std::cout << "Nexus Systems Utility Toolkit - Example Usage\n";
    std::cout << "===========================================\n\n";

    // Initialize utility system
    nexus::utility::initialize();

    // Example 1: Memory tracking
    {
        NEXUS_LOG_INFO("=== Memory Tracking Example ===");
        Mesh* mesh = new Mesh(1000);
        delete mesh;

        auto stats = memory::MemoryTracker::instance().getStats();
        NEXUS_LOG_INFO_FMT("Total allocations: " << stats.total_allocations);
    }

    // Example 2: Assertions
    {
        NEXUS_LOG_INFO("\n=== Assertion Example ===");
        float result = safeDiv(10.0f, 2.0f);
        NEXUS_LOG_INFO_FMT("10 / 2 = " << result);

        // This would throw:
        // safeDiv(10.0f, 0.0f);
    }

    // Example 3: Profiling
    {
        NEXUS_LOG_INFO("\n=== Profiling Example ===");
        expensiveCalculation();
    }

    // Example 4: Error handling
    {
        NEXUS_LOG_INFO("\n=== Error Handling Example ===");
        auto result1 = parseInt("42");
        if (result1.has_value()) {
            NEXUS_LOG_INFO_FMT("Parsed: " << result1.value());
        }

        auto result2 = parseInt("");
        if (!result2.has_value()) {
            NEXUS_LOG_ERROR_FMT("Parse failed: " << result2.error().value());
            error::printError(result2, std::cerr);
        }
    }

    // Example 5: Thread tracking
    {
        NEXUS_LOG_INFO("\n=== Thread Tracking Example ===");
        concurrency::TrackedThread worker("WorkerThread", workerThread);
        worker.join();

        NEXUS_LOG_INFO_FMT("Active threads: "
            << concurrency::ThreadTracker::instance().getActiveThreadCount());
    }

    // Example 6: Debug helpers
    {
        NEXUS_LOG_INFO("\n=== Debug Helpers Example ===");

        uint8_t buffer[] = {0x48, 0x65, 0x6C, 0x6C, 0x6F, 0x20, 0x56, 0x6F,
                           0x6C, 0x74, 0x72, 0x6F, 0x6E};  // "Hello Nexus Systems"

        std::cout << "Memory dump:\n";
        debug::HexDumper::dumpWithAscii(buffer, sizeof(buffer), std::cout);

        debug::DebuggerDetection::printStatus(std::cout);
    }

    // Example 7: Build info
    {
        NEXUS_LOG_INFO("\n=== Build Information ===");
        system::BuildInfo::printAll(std::cout);
    }

    // Print full diagnostic report
    std::cout << "\n";
    nexus::utility::printDiagnosticReport(std::cout);

    // Shutdown utility system
    nexus::utility::shutdown();

    std::cout << "\nExample completed successfully!\n";
    return 0;
}
