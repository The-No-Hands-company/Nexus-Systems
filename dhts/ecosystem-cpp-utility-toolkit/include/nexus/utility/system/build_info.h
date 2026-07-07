#pragma once

#include <string>
#include <chrono>
#include <ostream>
#include <iostream>

namespace nexus::utility::system {

/// @brief Build information embedded at compile time
class BuildInfo {
public:
    static std::string getBuildDate();
    static std::string getBuildTime();
    static std::string getCompiler();
    static std::string getVersion();
    static std::string getGitHash();
    static std::string getPlatform();

    /// Print all build info
    static void printAll(std::ostream& os = std::cout);
};

} // namespace nexus::utility::system

// Macros to embed build information
#define NEXUS_BUILD_DATE __DATE__
#define NEXUS_BUILD_TIME __TIME__

#if defined(__GNUC__)
    #define NEXUS_COMPILER "GCC " __VERSION__
#elif defined(__clang__)
    #define NEXUS_COMPILER "Clang " __clang_version__
#elif defined(_MSC_VER)
    #define NEXUS_COMPILER "MSVC " _MSC_FULL_VER
#else
    #define NEXUS_COMPILER "Unknown"
#endif

#if defined(__linux__)
    #define NEXUS_PLATFORM "Linux"
#elif defined(_WIN32)
    #define NEXUS_PLATFORM "Windows"
#elif defined(__APPLE__)
    #define NEXUS_PLATFORM "macOS"
#else
    #define NEXUS_PLATFORM "Unknown"
#endif
