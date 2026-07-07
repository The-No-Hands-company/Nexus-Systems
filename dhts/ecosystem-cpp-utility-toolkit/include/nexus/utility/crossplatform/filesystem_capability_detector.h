#pragma once

#include <string>
#include <atomic>
#include <mutex>

namespace nexus::utility::crossplatform {

/**
 * @brief Detect filesystem capabilities (case sensitivity, symlinks, max path).
 */
class FilesystemCapabilityDetector {
public:
    struct Capabilities {
        bool caseSensitive = true;
        bool supportsSymlinks = true;
        bool supportsHardlinks = true;
        bool supportsUnicode = true;
        std::size_t maxPathLength = 4096;
        std::size_t maxFilenameLength = 255;
        char preferredSeparator = '/';
    };

    static FilesystemCapabilityDetector& instance() {
        static FilesystemCapabilityDetector inst;
        return inst;
    }

    void initialize(const std::string& config = "") {
        std::lock_guard<std::mutex> lk(mutex_);
        caps_ = detectDefaults();
        config_ = config;
        enabled_ = true;
    }
    void shutdown() { enabled_ = false; }
    bool isEnabled() const { return enabled_.load(); }

    /// Compile-time-informed defaults for the current platform.
    static Capabilities detectDefaults() {
        Capabilities c;
#if defined(_WIN32)
        c.caseSensitive = false;
        c.supportsSymlinks = true;
        c.maxPathLength = 260;
        c.preferredSeparator = '\\';
#elif defined(__APPLE__)
        c.caseSensitive = false;   // default HFS+/APFS volumes
        c.maxPathLength = 1024;
        c.preferredSeparator = '/';
#else
        c.caseSensitive = true;
        c.maxPathLength = 4096;
        c.preferredSeparator = '/';
#endif
        return c;
    }

    Capabilities capabilities() const {
        std::lock_guard<std::mutex> lk(mutex_);
        return caps_;
    }

    void overrideCapabilities(const Capabilities& caps) {
        std::lock_guard<std::mutex> lk(mutex_);
        caps_ = caps;
    }

    bool pathFits(const std::string& path) const {
        std::lock_guard<std::mutex> lk(mutex_);
        return path.size() <= caps_.maxPathLength;
    }

    void reset() {
        std::lock_guard<std::mutex> lk(mutex_);
        caps_ = detectDefaults();
    }

private:
    FilesystemCapabilityDetector() : caps_(detectDefaults()) {}
    ~FilesystemCapabilityDetector() = default;
    FilesystemCapabilityDetector(const FilesystemCapabilityDetector&) = delete;
    FilesystemCapabilityDetector& operator=(const FilesystemCapabilityDetector&) = delete;

    mutable std::mutex mutex_;
    std::atomic<bool> enabled_{false};
    std::string config_;
    Capabilities caps_;
};

} // namespace nexus::utility::crossplatform
