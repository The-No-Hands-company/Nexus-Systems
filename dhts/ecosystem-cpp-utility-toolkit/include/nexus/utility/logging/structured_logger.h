#pragma once

#include <string>
#include <vector>
#include <sstream>
#include <variant>
#include <map>
#include <mutex>
#include <nexus/utility/logging/logger.h>
#include <source_location>

namespace nexus::utility::logging {

/// @brief Helper to build JSON object for structured logging
class LogBuilder {
public:
    LogBuilder& add(const std::string& key, const std::string& value);
    LogBuilder& add(const std::string& key, const char* value); // Fix for string literal -> bool
    LogBuilder& add(const std::string& key, int value);
    LogBuilder& add(const std::string& key, double value);
    LogBuilder& add(const std::string& key, bool value);

    std::string toJSON() const;

private:
    std::vector<std::pair<std::string, std::string>> fields_;
};

/**
 * @brief Structured logger that outputs logs in JSON format
 */
class StructuredLogger {
public:
    static StructuredLogger& instance();

    void log(LogLevel level, const std::string& event, const LogBuilder& data,
             std::source_location location = std::source_location::current());
             
    // Direct map support
    void log(LogLevel level, const std::string& event, 
             const std::map<std::string, std::string>& data,
             std::source_location location = std::source_location::current());

private:
    StructuredLogger() = default;
};

} // namespace nexus::utility::logging
