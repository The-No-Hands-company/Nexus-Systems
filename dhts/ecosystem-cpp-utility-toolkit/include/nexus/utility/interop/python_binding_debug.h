#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <atomic>
#include <mutex>

namespace nexus::utility::interop {

/**
 * @brief Track Python binding calls and reference-count balance (GIL/refcount).
 */
class PythonBindingDebug {
public:
    struct FunctionStats {
        std::string function;
        std::size_t calls = 0;
        std::size_t exceptions = 0;
    };

    static PythonBindingDebug& instance() {
        static PythonBindingDebug inst;
        return inst;
    }

    void initialize(const std::string& config = "") {
        std::lock_guard<std::mutex> lk(mutex_);
        config_ = config;
        enabled_ = true;
    }
    void shutdown() { enabled_ = false; }
    bool isEnabled() const { return enabled_.load(); }

    void recordCall(const std::string& function, bool raised = false) {
        std::lock_guard<std::mutex> lk(mutex_);
        auto& s = functions_[function];
        s.function = function;
        ++s.calls;
        if (raised) ++s.exceptions;
        ++totalCalls_;
    }

    void incref() { ++increfs_; }
    void decref() { ++decrefs_; }

    /// Net reference count delta; nonzero at teardown suggests a leak/over-free.
    long refCountBalance() const {
        return static_cast<long>(increfs_.load()) - static_cast<long>(decrefs_.load());
    }
    bool refCountBalanced() const { return refCountBalance() == 0; }

    void acquireGil() { ++gilAcquires_; }
    void releaseGil() { ++gilReleases_; }
    bool gilBalanced() const { return gilAcquires_.load() == gilReleases_.load(); }

    FunctionStats stats(const std::string& function) const {
        std::lock_guard<std::mutex> lk(mutex_);
        auto it = functions_.find(function);
        return it == functions_.end() ? FunctionStats{function, 0, 0} : it->second;
    }

    std::size_t totalCalls() const { return totalCalls_.load(); }

    void reset() {
        std::lock_guard<std::mutex> lk(mutex_);
        functions_.clear();
        totalCalls_ = 0;
        increfs_ = 0; decrefs_ = 0;
        gilAcquires_ = 0; gilReleases_ = 0;
    }

private:
    PythonBindingDebug() = default;
    ~PythonBindingDebug() = default;
    PythonBindingDebug(const PythonBindingDebug&) = delete;
    PythonBindingDebug& operator=(const PythonBindingDebug&) = delete;

    mutable std::mutex mutex_;
    std::atomic<bool> enabled_{false};
    std::string config_;
    std::unordered_map<std::string, FunctionStats> functions_;
    std::atomic<std::size_t> totalCalls_{0};
    std::atomic<std::size_t> increfs_{0};
    std::atomic<std::size_t> decrefs_{0};
    std::atomic<std::size_t> gilAcquires_{0};
    std::atomic<std::size_t> gilReleases_{0};
};

} // namespace nexus::utility::interop
