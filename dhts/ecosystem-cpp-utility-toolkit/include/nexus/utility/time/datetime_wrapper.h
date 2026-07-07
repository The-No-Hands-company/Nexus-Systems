#pragma once

#include <chrono>
#include <string>
#include <format>
#include <optional>
#include <sstream>

namespace nexus::utility::time {

/**
 * @brief Utilities for simplified DateTime handling using C++20 chrono.
 */
class DateTimeWrapper {
public:
    using SystemClock = std::chrono::system_clock;
    using TimePoint = std::chrono::time_point<SystemClock>;

    /**
     * @brief Gets current UTC time point.
     */
    static TimePoint now() {
        return SystemClock::now();
    }

    /**
     * @brief Converts time point to ISO 8601 string (UTC).
     * Example: 2023-10-05 14:30:00
     */
    static std::string toString(const TimePoint& tp) {
        // C++20 formatting
        return std::format("{:%Y-%m-%d %H:%M:%S}", tp);
    }
    
    /**
     * @brief Converts time point to string with custom format.
     */
    static std::string toString(const TimePoint& tp, std::string_view fmt) {
        return std::vformat(fmt, std::make_format_args(tp));
    }

    /**
     * @brief Gets Unix epoch timestamp (seconds).
     */
    static int64_t toEpoch(const TimePoint& tp) {
        return std::chrono::duration_cast<std::chrono::seconds>(tp.time_since_epoch()).count();
    }

    /**
     * @brief Creates time point from Unix epoch (seconds).
     */
    static TimePoint fromEpoch(int64_t seconds) {
        return TimePoint(std::chrono::seconds(seconds));
    }

    // Parsing is notoriously hard in C++ pre-20 without external libs (like date.h)
    // C++20 adds std::chrono::parse but compiler support varies (GCC 13+ usually okay).
    // We will attempt a stream-based parse or provide a simple one.
    
    /**
     * @brief Simple ISO8601 parser.
     * Expects: YYYY-mm-dd HH:MM:SS
     */
    static std::optional<TimePoint> parse(const std::string& str) {
        std::istringstream ss(str);
        std::chrono::sys_seconds tp;
        ss >> std::chrono::parse("%Y-%m-%d %H:%M:%S", tp);
        if (ss.fail()) return std::nullopt;
        return tp;
    }
};

} // namespace nexus::utility::time
