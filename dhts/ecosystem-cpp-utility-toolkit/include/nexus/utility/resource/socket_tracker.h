#pragma once

#include <string>
#include <source_location>
#include <nexus/utility/resource/resource_tracker.h>

// Forward declarations if POSIX headers not available or to avoid pollution
// Instead we'll use int for socket handle
// On Windows standard is different but int usually works for FD or need SOCKET type.
// We'll stick to 'int' for POSIX/Linux focus of this environment.

namespace nexus::utility::resource {

/// @brief Tracks socket descriptor usage
class SocketTracker {
public:
    /// @brief Track an opened socket
    static void trackSocket(int sockfd, int domain, int type, int protocol,
                          std::source_location loc = std::source_location::current()) {
        if (sockfd >= 0) {
            std::string desc = "Socket (fd=" + std::to_string(sockfd) + ")"; 
            // Could decode domain/type strings but that requires large switches
            ResourceTracker::instance().registerResource(reinterpret_cast<void*>(static_cast<uintptr_t>(sockfd)), "Socket", desc, loc);
        }
    }

    /// @brief Track a closed socket
    static void trackClose(int sockfd) {
        if (sockfd >= 0) {
            ResourceTracker::instance().unregisterResource(reinterpret_cast<void*>(static_cast<uintptr_t>(sockfd)));
        }
    }
};

} // namespace nexus::utility::resource
