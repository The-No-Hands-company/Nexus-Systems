#pragma once

#include <string>
#include <functional>
#include <source_location>
#include <mutex>
#include <shared_mutex>
#include <nexus/utility/logging/logger.h>

namespace nexus::utility::error {

/// @brief Centralized reporter for non-fatal errors
class ErrorReporter {
public:
    static ErrorReporter& instance();

    using ErrorCallback = std::function<void(const std::string&, const std::source_location&)>;

    /// @brief Report a generic error
    void reportError(const std::string& message, 
                    std::source_location loc = std::source_location::current());

    /// @brief Report a warning
    void reportWarning(const std::string& message, 
                      std::source_location loc = std::source_location::current());
    
    /// @brief Set a custom callback for errors (e.g. show GUI dialog)
    void setErrorCallback(ErrorCallback callback);
    
    /// @brief Set a custom callback for warnings
    void setWarningCallback(ErrorCallback callback);

private:
    ErrorReporter() = default;
    ~ErrorReporter() = default;

    mutable std::shared_mutex mutex_;
    ErrorCallback error_callback_;
    ErrorCallback warning_callback_;
};

} // namespace nexus::utility::error
