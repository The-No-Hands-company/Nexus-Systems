#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <atomic>
#include <mutex>
#include <cstdint>

namespace nexus::utility::plugin {

/**
 * @brief Validate plugin ABI compatibility with the host application.
 */
class AbiValidator {
public:
    struct AbiInfo {
        std::uint32_t version = 0;
        std::size_t pointerSize = sizeof(void*);
        std::string compiler;
        std::string stdlib;
        bool exceptionsEnabled = true;
        bool rttiEnabled = true;
    };

    static AbiValidator& instance() {
        static AbiValidator inst;
        return inst;
    }

    void initialize(const std::string& config = "") {
        std::lock_guard<std::mutex> lk(mutex_);
        host_ = hostAbi();
        config_ = config;
        enabled_ = true;
    }
    void shutdown() { enabled_ = false; }
    bool isEnabled() const { return enabled_.load(); }

    static AbiInfo hostAbi() {
        AbiInfo a;
        a.version = 1;
        a.pointerSize = sizeof(void*);
#if defined(__clang__)
        a.compiler = "clang";
#elif defined(__GNUC__)
        a.compiler = "gcc";
#elif defined(_MSC_VER)
        a.compiler = "msvc";
#else
        a.compiler = "unknown";
#endif
        return a;
    }

    void setHostAbi(const AbiInfo& info) {
        std::lock_guard<std::mutex> lk(mutex_);
        host_ = info;
    }

    /// A plugin is compatible if version, pointer size and compiler match.
    bool isCompatible(const AbiInfo& plugin) {
        std::lock_guard<std::mutex> lk(mutex_);
        bool ok = plugin.version == host_.version &&
                  plugin.pointerSize == host_.pointerSize &&
                  plugin.compiler == host_.compiler &&
                  plugin.exceptionsEnabled == host_.exceptionsEnabled &&
                  plugin.rttiEnabled == host_.rttiEnabled;
        ++checks_;
        if (!ok) ++rejected_;
        return ok;
    }

    AbiInfo host() const {
        std::lock_guard<std::mutex> lk(mutex_);
        return host_;
    }

    std::size_t checks() const { return checks_.load(); }
    std::size_t rejected() const { return rejected_.load(); }

    void reset() { checks_ = 0; rejected_ = 0; }

private:
    AbiValidator() : host_(hostAbi()) {}
    ~AbiValidator() = default;
    AbiValidator(const AbiValidator&) = delete;
    AbiValidator& operator=(const AbiValidator&) = delete;

    mutable std::mutex mutex_;
    std::atomic<bool> enabled_{false};
    std::string config_;
    AbiInfo host_;
    std::atomic<std::size_t> checks_{0};
    std::atomic<std::size_t> rejected_{0};
};

} // namespace nexus::utility::plugin
