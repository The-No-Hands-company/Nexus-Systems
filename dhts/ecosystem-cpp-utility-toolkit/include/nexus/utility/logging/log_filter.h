#pragma once

#include <nexus/utility/logging/log_macros.h>
#include <string>
#include <functional>
#include <regex>
#include <vector>

namespace nexus::utility::logging {

/**
 * @brief Filtering system for log messages.
 */
class LogFilter {
public:
    using FilterFunc = std::function<bool(LogLevel, const std::string&)>;

    /**
     * @brief Creates a filter that passes messages at or above minimum level.
     */
    static FilterFunc byLevel(LogLevel min_level) {
        return [min_level](LogLevel level, const std::string&) {
            return static_cast<int>(level) >= static_cast<int>(min_level);
        };
    }

    /**
     * @brief Creates a filter that passes messages containing a substring.
     */
    static FilterFunc bySubstring(const std::string& substring) {
        return [substring](LogLevel, const std::string& message) {
            return message.find(substring) != std::string::npos;
        };
    }

    /**
     * @brief Creates a filter that passes messages matching a regex pattern.
     */
    static FilterFunc byPattern(const std::string& pattern) {
        std::regex regex(pattern);
        return [regex](LogLevel, const std::string& message) {
            return std::regex_search(message, regex);
        };
    }

    /**
     * @brief Creates a filter that passes messages from specific categories.
     */
    static FilterFunc byCategory(const std::vector<std::string>& categories) {
        return [categories](LogLevel, const std::string& message) {
            for (const auto& cat : categories) {
                if (message.find(cat) != std::string::npos) {
                    return true;
                }
            }
            return false;
        };
    }

    /**
     * @brief Combines filters with AND logic.
     */
    static FilterFunc andFilter(const std::vector<FilterFunc>& filters) {
        return [filters](LogLevel level, const std::string& message) {
            for (const auto& filter : filters) {
                if (!filter(level, message)) {
                    return false;
                }
            }
            return true;
        };
    }

    /**
     * @brief Combines filters with OR logic.
     */
    static FilterFunc orFilter(const std::vector<FilterFunc>& filters) {
        return [filters](LogLevel level, const std::string& message) {
            for (const auto& filter : filters) {
                if (filter(level, message)) {
                    return true;
                }
            }
            return false;
        };
    }

    /**
     * @brief Negates a filter.
     */
    static FilterFunc notFilter(FilterFunc filter) {
        return [filter](LogLevel level, const std::string& message) {
            return !filter(level, message);
        };
    }

    /**
     * @brief Tests if a message passes the filter.
     */
    static bool test(const FilterFunc& filter, LogLevel level, const std::string& message) {
        return filter(level, message);
    }
};

} // namespace nexus::utility::logging
