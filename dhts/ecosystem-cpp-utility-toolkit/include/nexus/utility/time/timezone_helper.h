#pragma once

#include <chrono>
#include <string>
#include <optional>
#include <nexus/utility/time/datetime_wrapper.h>
#include <sstream>

namespace nexus::utility::time {

class TimezoneHelper {
public:
    using TimePoint = DateTimeWrapper::TimePoint; // sys_time

    /**
     * @brief Gets current timezone offset in seconds from UTC.
     */
    static std::chrono::seconds getOffset() {
        auto now = std::chrono::system_clock::now();
        auto local_zone = std::chrono::current_zone();
        auto local_info = local_zone->get_info(now);
        return local_info.offset;
    }

    /**
     * @brief Converts UTC time point to Local time string representation.
     * C++20 chrono stores system_clock as UTC usually.
     * To represent "local time", we usually convert to zoned_time.
     */
    static std::string toLocalString(const TimePoint& utc_tp) {
        auto zt = std::chrono::zoned_time{std::chrono::current_zone(), utc_tp};
        return std::format("{:%Y-%m-%d %H:%M:%S}", zt);
    }

    /**
     * @brief Converts a specific TimeZone time to UTC.
     * @param local_str "2023-10-05 14:30:00"
     * @param zone_name "America/New_York"
     */
    static std::optional<TimePoint> toUTC(const std::string& local_str, std::string_view zone_name) {
        try {
            // Parse into a local_seconds (time without zone)
            std::istringstream ss(local_str);
            std::chrono::local_seconds tp;
            ss >> std::chrono::parse("%Y-%m-%d %H:%M:%S", tp);
            
            if (ss.fail()) return std::nullopt;

            auto zone = std::chrono::locate_zone(zone_name);
            // Convert local time in that zone to sys_time (UTC)
            return std::chrono::zoned_time{zone, tp}.get_sys_time();
        } catch (...) {
            return std::nullopt;
        }
    }
    
    static std::chrono::time_zone const* getLocalZone() {
        return std::chrono::current_zone();
    }
};

} // namespace nexus::utility::time
