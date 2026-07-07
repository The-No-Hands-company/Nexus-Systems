#pragma once

#include <string>
#include <vector>
#include <atomic>
#include <mutex>

#if !defined(_WIN32)
#include <dlfcn.h>
#endif

namespace nexus::utility::plugin {

/**
 * @brief Wrap dlopen/dlsym with error tracking (POSIX; no-op stubs elsewhere).
 */
class DlopenWrapper {
public:
    struct OpenResult {
        void* handle = nullptr;
        bool success = false;
        std::string error;
    };

    static DlopenWrapper& instance() {
        static DlopenWrapper inst;
        return inst;
    }

    void initialize(const std::string& config = "") {
        std::lock_guard<std::mutex> lk(mutex_);
        config_ = config;
        enabled_ = true;
    }
    void shutdown() { enabled_ = false; }
    bool isEnabled() const { return enabled_.load(); }

    OpenResult open(const std::string& path, bool lazy = true) {
        OpenResult r;
#if !defined(_WIN32)
        int flags = lazy ? RTLD_LAZY : RTLD_NOW;
        r.handle = ::dlopen(path.c_str(), flags);
        if (!r.handle) {
            const char* e = ::dlerror();
            r.error = e ? e : "unknown dlopen error";
        } else {
            r.success = true;
        }
#else
        r.error = "dlopen not supported on this platform";
#endif
        std::lock_guard<std::mutex> lk(mutex_);
        ++opens_;
        if (!r.success) { ++failures_; errors_.push_back(r.error); }
        return r;
    }

    void* symbol(void* handle, const std::string& name) {
#if !defined(_WIN32)
        void* sym = handle ? ::dlsym(handle, name.c_str()) : nullptr;
        if (!sym) {
            std::lock_guard<std::mutex> lk(mutex_);
            ++symbolMisses_;
            const char* e = ::dlerror();
            if (e) errors_.push_back(e);
        }
        return sym;
#else
        (void)handle; (void)name;
        std::lock_guard<std::mutex> lk(mutex_);
        ++symbolMisses_;
        return nullptr;
#endif
    }

    bool close(void* handle) {
#if !defined(_WIN32)
        bool ok = handle ? (::dlclose(handle) == 0) : false;
#else
        (void)handle;
        bool ok = false;
#endif
        std::lock_guard<std::mutex> lk(mutex_);
        ++closes_;
        return ok;
    }

    std::size_t opens() const { return opens_.load(); }
    std::size_t failures() const { return failures_.load(); }
    std::size_t symbolMisses() const { return symbolMisses_.load(); }

    std::vector<std::string> errors() const {
        std::lock_guard<std::mutex> lk(mutex_);
        return errors_;
    }

    void reset() {
        std::lock_guard<std::mutex> lk(mutex_);
        opens_ = 0; failures_ = 0; symbolMisses_ = 0; closes_ = 0;
        errors_.clear();
    }

private:
    DlopenWrapper() = default;
    ~DlopenWrapper() = default;
    DlopenWrapper(const DlopenWrapper&) = delete;
    DlopenWrapper& operator=(const DlopenWrapper&) = delete;

    mutable std::mutex mutex_;
    std::atomic<bool> enabled_{false};
    std::string config_;
    std::atomic<std::size_t> opens_{0};
    std::atomic<std::size_t> failures_{0};
    std::atomic<std::size_t> symbolMisses_{0};
    std::atomic<std::size_t> closes_{0};
    std::vector<std::string> errors_;
};

} // namespace nexus::utility::plugin
