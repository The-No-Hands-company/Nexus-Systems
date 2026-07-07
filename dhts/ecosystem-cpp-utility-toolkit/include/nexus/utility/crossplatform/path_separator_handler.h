#pragma once

#include <string>
#include <atomic>
#include <mutex>

namespace nexus::utility::crossplatform {

/**
 * @brief Normalize and convert path separators across platforms.
 */
class PathSeparatorHandler {
public:
    static PathSeparatorHandler& instance() {
        static PathSeparatorHandler inst;
        return inst;
    }

    void initialize(const std::string& config = "") {
        std::lock_guard<std::mutex> lk(mutex_);
        config_ = config;
        enabled_ = true;
    }
    void shutdown() { enabled_ = false; }
    bool isEnabled() const { return enabled_.load(); }

    static char nativeSeparator() {
#if defined(_WIN32)
        return '\\';
#else
        return '/';
#endif
    }

    /// Convert all separators to forward slashes (POSIX style).
    static std::string toPosix(const std::string& path) {
        std::string out = path;
        for (char& c : out) if (c == '\\') c = '/';
        return out;
    }

    /// Convert all separators to backslashes (Windows style).
    static std::string toWindows(const std::string& path) {
        std::string out = path;
        for (char& c : out) if (c == '/') c = '\\';
        return out;
    }

    /// Convert to the native separator and collapse duplicate separators.
    std::string toNative(const std::string& path) {
        std::lock_guard<std::mutex> lk(mutex_);
        ++conversions_;
        char sep = nativeSeparator();
        std::string out;
        out.reserve(path.size());
        char prev = 0;
        for (char c : path) {
            char n = (c == '/' || c == '\\') ? sep : c;
            if (n == sep && prev == sep) continue;
            out += n;
            prev = n;
        }
        return out;
    }

    /// Join two path segments with the native separator.
    std::string join(const std::string& a, const std::string& b) {
        if (a.empty()) return b;
        if (b.empty()) return a;
        char sep = nativeSeparator();
        std::string out = a;
        if (out.back() != '/' && out.back() != '\\') out += sep;
        std::string bb = b;
        while (!bb.empty() && (bb.front() == '/' || bb.front() == '\\')) bb.erase(bb.begin());
        return out + bb;
    }

    std::size_t conversions() const { return conversions_.load(); }
    void reset() { conversions_ = 0; }

private:
    PathSeparatorHandler() = default;
    ~PathSeparatorHandler() = default;
    PathSeparatorHandler(const PathSeparatorHandler&) = delete;
    PathSeparatorHandler& operator=(const PathSeparatorHandler&) = delete;

    mutable std::mutex mutex_;
    std::atomic<bool> enabled_{false};
    std::string config_;
    std::atomic<std::size_t> conversions_{0};
};

} // namespace nexus::utility::crossplatform
