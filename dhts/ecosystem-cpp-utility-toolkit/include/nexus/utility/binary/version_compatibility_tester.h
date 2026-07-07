#pragma once

#include <string>
#include <vector>
#include <sstream>
#include <atomic>
#include <mutex>
#include <cstdint>

namespace nexus::utility::binary {

/**
 * @brief Test whether binary format versions fall within compatible ranges.
 */
class VersionCompatibilityTester {
public:
    struct Version {
        int major = 0;
        int minor = 0;
        int patch = 0;

        bool operator<(const Version& o) const {
            if (major != o.major) return major < o.major;
            if (minor != o.minor) return minor < o.minor;
            return patch < o.patch;
        }
        bool operator<=(const Version& o) const { return !(o < *this); }
        bool operator==(const Version& o) const {
            return major == o.major && minor == o.minor && patch == o.patch;
        }
        std::string str() const {
            std::ostringstream os; os << major << "." << minor << "." << patch;
            return os.str();
        }
    };

    static VersionCompatibilityTester& instance() {
        static VersionCompatibilityTester inst;
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
        Version v;
        char dot;
        std::istringstream iss(s);
        iss >> v.major >> dot >> v.minor >> dot >> v.patch;
        return v;
    }

    /// A version is compatible if min <= version <= max.
    bool inRange(const Version& v, const Version& min, const Version& max) const {
        return min <= v && v <= max;
    }

    /// Semantic-versioning compatibility: same major, and version >= required.
    bool isCompatible(const Version& provided, const Version& required) {
        bool ok = provided.major == required.major && required <= provided;
        std::lock_guard<std::mutex> lk(mutex_);
        ++checks_;
        if (!ok) ++incompatible_;
        return ok;
    }

    std::size_t checks() const { return checks_.load(); }
    std::size_t incompatible() const { return incompatible_.load(); }

    void reset() { checks_ = 0; incompatible_ = 0; }

private:
    VersionCompatibilityTester() = default;
    ~VersionCompatibilityTester() = default;
    VersionCompatibilityTester(const VersionCompatibilityTester&) = delete;
    VersionCompatibilityTester& operator=(const VersionCompatibilityTester&) = delete;

    mutable std::mutex mutex_;
    std::atomic<bool> enabled_{false};
    std::string config_;
    std::atomic<std::size_t> checks_{0};
    std::atomic<std::size_t> incompatible_{0};
};

} // namespace nexus::utility::binary
