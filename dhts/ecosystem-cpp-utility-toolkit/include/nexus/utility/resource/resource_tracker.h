#pragma once

#include <string>
#include <unordered_map>
#include <mutex>
#include <shared_mutex>
#include <source_location>
#include <vector>
#include <any>

namespace nexus::utility::resource {

struct ResourceInfo {
    std::string type;
    std::string description;
    std::source_location location;
    // Potentially store stack trace? Expensive but useful for leaks.
};

/// @brief Generic registry for tracking system resources to detect leaks
class ResourceTracker {
public:
    static ResourceTracker& instance();

    /// @brief Register a resource
    /// @param handle Unique identifier for the resource (e.g. pointer address or file descriptor cast to void*)
    /// @param type Resource type name (e.g. "FILE*", "Socket")
    /// @param description User-friendly description or path
    void registerResource(const void* handle, const std::string& type, const std::string& description,
                         std::source_location loc = std::source_location::current());

    /// @brief Unregister a resource (it has been properly closed/freed)
    void unregisterResource(const void* handle);

    struct LeakedResource {
        const void* handle;
        ResourceInfo info;
    };

    /// @brief Get list of currently active resources
    std::vector<LeakedResource> getActiveResources() const;

private:
    ResourceTracker() = default;
    ~ResourceTracker() = default;

    mutable std::shared_mutex mutex_;
    std::unordered_map<const void*, ResourceInfo> resources_;
};

} // namespace nexus::utility::resource
