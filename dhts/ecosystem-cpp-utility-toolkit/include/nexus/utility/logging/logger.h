#pragma once

#include <string>
#include <mutex>
#include <vector>
#include <memory>
#include <chrono>
#include <sstream>
#include <source_location>
#include <format>
#include <nexus/utility/logging/log_sinks.h>

namespace nexus::utility::logging {

enum class LogLevel {
    Trace,
    Debug,
    Info,
    Warning,
    Error,
    Critical
};

/// @brief Singleton logger with pluggable sinks
class Logger {
public:
    [[nodiscard]] static Logger& instance();

    void log(LogLevel level, const std::string& message,
             std::source_location location = std::source_location::current());

    void addSink(std::shared_ptr<LogSink> sink);
    void clearSinks();

    void addConsoleSink();
    void addFileSink(const std::string& filename);
    void addRotatingFileSink(const std::string& filename, size_t max_size, int max_files);

    void setLogLevel(LogLevel level) noexcept;
    [[nodiscard]] LogLevel getLogLevel() const noexcept;

    void setConsoleOutput(bool enabled);
    [[nodiscard]] bool isConsoleOutput() const noexcept;

    void setTimestamps(bool enabled);
    [[nodiscard]] bool isTimestamps() const noexcept;

    void flush();

private:
    Logger();
    ~Logger();

    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    [[nodiscard]] std::string formatMessage(LogLevel level, const std::string& message,
                                            const std::source_location& location) const;
    [[nodiscard]] static const char* levelToString(LogLevel level) noexcept;

    mutable std::mutex mutex_;
    LogLevel min_level_ = LogLevel::Info;
    bool timestamps_ = true;
    bool console_output_ = true;
    std::vector<std::shared_ptr<LogSink>> sinks_;
    std::shared_ptr<ConsoleSink> default_console_sink_;
};

// ── Convenience Macros ──────────────────────────────────────────────────────
#define NEXUS_LOG_TRACE(msg) \
    nexus::utility::logging::Logger::instance().log(nexus::utility::logging::LogLevel::Trace, msg)
#define NEXUS_LOG_DEBUG(msg) \
    nexus::utility::logging::Logger::instance().log(nexus::utility::logging::LogLevel::Debug, msg)
#define NEXUS_LOG_INFO(msg) \
    nexus::utility::logging::Logger::instance().log(nexus::utility::logging::LogLevel::Info, msg)
#define NEXUS_LOG_WARN(msg) \
    nexus::utility::logging::Logger::instance().log(nexus::utility::logging::LogLevel::Warning, msg)
#define NEXUS_LOG_ERROR(msg) \
    nexus::utility::logging::Logger::instance().log(nexus::utility::logging::LogLevel::Error, msg)
#define NEXUS_LOG_CRIT(msg) \
    nexus::utility::logging::Logger::instance().log(nexus::utility::logging::LogLevel::Critical, msg)

#define NEXUS_LOG_INFO_FMT(fmt, ...) \
    do { \
        std::ostringstream _nexus_oss; \
        _nexus_oss << fmt; \
        nexus::utility::logging::Logger::instance().log(nexus::utility::logging::LogLevel::Info, _nexus_oss.str()); \
    } while (0)

#define NEXUS_LOG_DEBUG_FMT(fmt, ...) \
    do { \
        std::ostringstream _nexus_oss; \
        _nexus_oss << fmt; \
        nexus::utility::logging::Logger::instance().log(nexus::utility::logging::LogLevel::Debug, _nexus_oss.str()); \
    } while (0)

#define NEXUS_LOG_ERROR_FMT(fmt, ...) \
    do { \
        std::ostringstream _nexus_oss; \
        _nexus_oss << fmt; \
        nexus::utility::logging::Logger::instance().log(nexus::utility::logging::LogLevel::Error, _nexus_oss.str()); \
    } while (0)

} // namespace nexus::utility::logging
