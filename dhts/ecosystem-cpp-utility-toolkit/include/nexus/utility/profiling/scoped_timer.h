#pragma once

#include <chrono>
#include <string>
#include <functional>
#include <iostream>
#include <source_location>

namespace nexus::utility::profiling {

/**
 * @brief RAII-based automatic timer that reports elapsed time on destruction.
 */
class ScopedTimer {
public:
    using Clock = std::chrono::high_resolution_clock;
    using Duration = std::chrono::microseconds;
    using Callback = std::function<void(const std::string&, int64_t)>;

    /**
     * @brief Constructs a scoped timer with a name.
     * @param name Name/description of the timed section.
     * @param callback Optional callback for custom reporting.
     * @param loc Source location for context.
     */
    explicit ScopedTimer(
        std::string name,
        Callback callback = nullptr,
        std::source_location loc = std::source_location::current()
    ) : name_(std::move(name)), 
        callback_(std::move(callback)),
        location_(loc),
        start_(Clock::now()) {}

    ~ScopedTimer() {
        auto end = Clock::now();
        auto elapsed = std::chrono::duration_cast<Duration>(end - start_).count();
        
        if (callback_) {
            callback_(name_, elapsed);
        } else {
            // Default: print to stdout
            std::cout << "[ScopedTimer] " << name_ 
                     << " took " << elapsed << " μs"
                     << " (" << location_.file_name() << ":" << location_.line() << ")\n";
        }
    }

    // Non-copyable, non-movable
    ScopedTimer(const ScopedTimer&) = delete;
    ScopedTimer& operator=(const ScopedTimer&) = delete;
    ScopedTimer(ScopedTimer&&) = delete;
    ScopedTimer& operator=(ScopedTimer&&) = delete;

    /**
     * @brief Gets elapsed time so far (without stopping the timer).
     */
    int64_t elapsed() const {
        auto now = Clock::now();
        return std::chrono::duration_cast<Duration>(now - start_).count();
    }

private:
    std::string name_;
    Callback callback_;
    std::source_location location_;
    Clock::time_point start_;
};

// Convenience macro
#define NEXUS_SCOPED_TIMER(name) \
    nexus::utility::profiling::ScopedTimer _scoped_timer_##__LINE__(name)

} // namespace nexus::utility::profiling
