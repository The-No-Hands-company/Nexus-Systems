#pragma once

#include <string>
#include <vector>
#include <sstream>
#include <atomic>
#include <mutex>

namespace nexus::utility::plugin {

/**
 * @brief Check plugin version compatibility against a host requirement.
 */
class PluginVersionChecker {
public:
    struct Version {
        int major = 0, minor = 0, patch = 0;
        bool operator<(const Version& o) const {
            if (major != o.major) return major < o.major;
            if (minor != o.minor) return minor < o.minor;
            return patch < o.patch;
        }
        bool operator<=(const Version& o) const { return !(o < *this); }
        std::string str() const {
            std::ostringstream os; os << major << "." << minor << "." << patch;
            return os.str();
        }
    };

    static PluginVersionChecker& instance() {
        static PluginVersionChecker inst;
        return inst;
    }

    void initialize(const std::string& config = "") {
        std::lock_guard<std::mutex> lk(mutex_);
        config_ = config;
        enabled_ = true;
    }
    void shutdown() { enabled_ = false; }
    bool isEnabled() const { return enabled_.load(); }

    static Version parse(const std::string& s) {
        Version v; char dot;
        std::istringstream iss(s);
        iss >> v.major >> dot >> v.minor >> dot >> v.patch;
        return v;
    }

    void setHostVersion(const Version& v) {
        std::lock_guard<std::mutex> lk(mutex_);
        host_ = v;
    }

    /// Compatible if same major version and plugin requires <= host version.
    bool isCompatible(const Version& pluginApiVersion) {
        std::lock_guard<std::mutex> lk(mutex_);
        bool ok = pluginApiVersion.major == host_.major && pluginApiVersion <= host_;
        ++checks_;
        if (!ok) ++incompatible_;
        return ok;
    }

    bool inRange(const Version& v, const Version& min, const Version& max) const {
        return min <= v && v <= max;
    }

    std::size_t checks() const { return checks_.load(); }
    std::size_t incompatible() const { return incompatible_.load(); }

    void reset() { checks_ = 0; incompatible_ = 0; }

private:
    PluginVersionChecker() = default;
    ~PluginVersionChecker() = default;
    PluginVersionChecker(const PluginVersionChecker&) = delete;
    PluginVersionChecker& operator=(const PluginVersionChecker&) = delete;

    mutable std::mutex mutex_;
    std::atomic<bool> enabled_{false};
    std::string config_;
    Version host_{1, 0, 0};
    std::atomic<std::size_t> checks_{0};
    std::atomic<std::size_t> incompatible_{0};
};

} // namespace nexus::utility::plugin
