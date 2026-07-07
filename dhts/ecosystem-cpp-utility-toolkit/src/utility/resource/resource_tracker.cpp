#include "nexus/utility/resource/resource_tracker.h"
namespace nexus::utility::resource {
ResourceTracker& ResourceTracker::instance() { static ResourceTracker t; return t; }
void ResourceTracker::registerResource(const void* handle, const std::string& type, const std::string& description, std::source_location loc) { resources_[handle] = {type, description, loc}; }
void ResourceTracker::unregisterResource(const void* handle) { resources_.erase(handle); }
} // namespace nexus::utility::resource
