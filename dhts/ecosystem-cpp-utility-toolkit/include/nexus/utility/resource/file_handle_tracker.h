#pragma once

#include <cstdio>
#include <string>
#include <source_location>
#include <nexus/utility/resource/resource_tracker.h>

namespace nexus::utility::resource {

/// @brief Tracks FILE* or file descriptor usage
class FileHandleTracker {
public:
    /// @brief Track an opened file
    static void trackOpen(FILE* file, const std::string& path, 
                         std::source_location loc = std::source_location::current()) {
        if (file) {
            ResourceTracker::instance().registerResource(file, "FILE*", "File: " + path, loc);
        }
    }

    /// @brief Track a closed file
    static void trackClose(FILE* file) {
        if (file) {
            ResourceTracker::instance().unregisterResource(file);
        }
    }
};

/// @brief RAII wrapper for tracking generic resources if not using smart pointers
/// Usage: TrackedFile f(fopen("foo", "r"), "foo");
class TrackedFile {
public:
    TrackedFile(FILE* f, const std::string& path, 
               std::source_location loc = std::source_location::current())
        : file_(f) 
    {
        FileHandleTracker::trackOpen(file_, path, loc);
    }

    ~TrackedFile() {
        if (file_) {
            FileHandleTracker::trackClose(file_);
            std::fclose(file_); // Auto-close. Or should we? Yes, RAII usually owns.
        }
    }

    // Move-only
    TrackedFile(const TrackedFile&) = delete;
    TrackedFile& operator=(const TrackedFile&) = delete;
    
    TrackedFile(TrackedFile&& other) noexcept : file_(other.file_) {
        other.file_ = nullptr;
    }
    
    TrackedFile& operator=(TrackedFile&& other) noexcept {
        if (this != &other) {
            if (file_) {
                FileHandleTracker::trackClose(file_);
                std::fclose(file_);
            }
            file_ = other.file_;
            other.file_ = nullptr;
        }
        return *this;
    }

    FILE* get() const { return file_; }
    operator FILE*() const { return file_; }

private:
    FILE* file_ = nullptr;
};

} // namespace nexus::utility::resource
