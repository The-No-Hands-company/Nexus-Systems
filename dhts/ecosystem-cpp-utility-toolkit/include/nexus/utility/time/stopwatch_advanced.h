#pragma once

#include <chrono>
#include <vector>

namespace nexus::utility::time {

/**
 * @brief High-precision stopwatch with lap support.
 */
class StopwatchAdvanced {
public:
    using Clock = std::chrono::high_resolution_clock;
    using Duration = Clock::duration;
    using TimePoint = Clock::time_point;

    StopwatchAdvanced() {
        start();
    }

    void start() {
        if (!running_) {
            start_time_ = Clock::now();
            running_ = true;
        }
    }

    void stop() {
        if (running_) {
            accumulated_ += (Clock::now() - start_time_);
            running_ = false;
        }
    }

    void reset() {
        running_ = false;
        accumulated_ = Duration::zero();
        laps_.clear();
    }

    void restart() {
        reset();
        start();
    }

    Duration elapsed() const {
        if (running_) {
            return accumulated_ + (Clock::now() - start_time_);
        }
        return accumulated_;
    }

    int64_t elapsedMs() const {
        return std::chrono::duration_cast<std::chrono::milliseconds>(elapsed()).count();
    }
    
    int64_t elapsedUs() const {
        return std::chrono::duration_cast<std::chrono::microseconds>(elapsed()).count();
    }

    /**
     * @brief Records current elapsed time as a lap and returns it.
     */
    Duration lap() {
        Duration current = elapsed();
        Duration lap_time;
        if (laps_.empty()) {
            lap_time = current;
        } else {
            lap_time = current - laps_.back().total_time;
        }
        laps_.push_back({current, lap_time});
        return lap_time;
    }

    struct LapInfo {
        Duration total_time;
        Duration lap_duration;
    };
    
    const std::vector<LapInfo>& getLaps() const {
        return laps_;
    }

private:
    TimePoint start_time_;
    Duration accumulated_ = Duration::zero();
    bool running_ = false;
    std::vector<LapInfo> laps_;
};

} // namespace nexus::utility::time
