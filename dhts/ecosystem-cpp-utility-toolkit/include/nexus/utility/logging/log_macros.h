#pragma once

#include <source_location>
#include <format>
#include <string>
#include <iostream>
#include <nexus/utility/logging/logger.h>

namespace nexus::utility::logging {

/// @brief Lightweight static logger for use outside of singleton Logger
class LogMacros {
public:
    [[nodiscard]] static const char* getLevelString(LogLevel level) noexcept {
        switch (level) {
            case LogLevel::Trace:    return "TRACE";
            case LogLevel::Debug:    return "DEBUG";
            case LogLevel::Info:     return "INFO";
            case LogLevel::Warning:  return "WARN";
            case LogLevel::Error:    return "ERROR";
            case LogLevel::Critical: return "CRIT";
            default:                 return "UNKNOWN";
        }
    }
};

// ── Formatted Logging Macros ────────────────────────────────────────────────
#define LOG_DEBUG(fmt, ...) \
    nexus::utility::logging::Logger::instance().log(nexus::utility::logging::LogLevel::Debug, std::format(fmt, ##__VA_ARGS__))

#define LOG_INFO(fmt, ...) \
    nexus::utility::logging::Logger::instance().log(nexus::utility::logging::LogLevel::Info, std::format(fmt, ##__VA_ARGS__))

#define LOG_WARN(fmt, ...) \
    nexus::utility::logging::Logger::instance().log(nexus::utility::logging::LogLevel::Warning, std::format(fmt, ##__VA_ARGS__))

#define LOG_ERROR(fmt, ...) \
    nexus::utility::logging::Logger::instance().log(nexus::utility::logging::LogLevel::Error, std::format(fmt, ##__VA_ARGS__))

#define LOG_FATAL(fmt, ...) \
    nexus::utility::logging::Logger::instance().log(nexus::utility::logging::LogLevel::Critical, std::format(fmt, ##__VA_ARGS__))

// ── Conditional Logging Macros ──────────────────────────────────────────────
#define LOG_IF(condition, level, fmt, ...) \
    do { if (condition) nexus::utility::logging::Logger::instance().log(level, std::format(fmt, ##__VA_ARGS__)); } while (0)

#define LOG_DEBUG_IF(condition, fmt, ...) \
    LOG_IF(condition, nexus::utility::logging::LogLevel::Debug, fmt, ##__VA_ARGS__)
#define LOG_INFO_IF(condition, fmt, ...) \
    LOG_IF(condition, nexus::utility::logging::LogLevel::Info, fmt, ##__VA_ARGS__)
#define LOG_WARN_IF(condition, fmt, ...) \
    LOG_IF(condition, nexus::utility::logging::LogLevel::Warning, fmt, ##__VA_ARGS__)
#define LOG_ERROR_IF(condition, fmt, ...) \
    LOG_IF(condition, nexus::utility::logging::LogLevel::Error, fmt, ##__VA_ARGS__)

} // namespace nexus::utility::logging
