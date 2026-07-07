#pragma once

#include <cstddef>

namespace nexus::utility::resource {

/// @brief Utility to detect and report leaked resources
class ResourceLeakDetector {
public:
    /// @brief Check for leaks and report them via ErrorReporter/Logger
    /// @return Number of detected leaks
    static size_t reportLeaks();
};

} // namespace nexus::utility::resource
