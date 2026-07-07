#pragma once

#include <set>
#include <mutex>
#include <string>

namespace nexus::utility::container {

class SeccompValidator {
public:
    static SeccompValidator& instance() {
        static SeccompValidator inst;
        return inst;
    }

    void initialize(const std::string& config = "") { std::lock_guard<std::mutex> lk(mutex_); enabled_ = true; config_ = config; }
    void shutdown() { std::lock_guard<std::mutex> lk(mutex_); enabled_ = false; }
    bool isEnabled() const { return enabled_; }
    void enable() { std::lock_guard<std::mutex> lk(mutex_); enabled_ = true; }
    void disable() { std::lock_guard<std::mutex> lk(mutex_); enabled_ = false; }
    void reset() { std::lock_guard<std::mutex> lk(mutex_); allowed_.clear(); denied_.clear(); }

    void addAllowedSyscall(int syscall_num) {
        std::lock_guard<std::mutex> lk(mutex_);
        allowed_.insert(syscall_num);
    }

    void addDeniedSyscall(int syscall_num) {
        std::lock_guard<std::mutex> lk(mutex_);
        denied_.insert(syscall_num);
    }

    bool isAllowed(int syscall_num) const {
        std::lock_guard<std::mutex> lk(mutex_);
        if (denied_.find(syscall_num) != denied_.end()) return false;
        if (!allowed_.empty() && allowed_.find(syscall_num) == allowed_.end()) return false;
        return true;
    }

    size_t getSyscallCount() const {
        std::lock_guard<std::mutex> lk(mutex_);
        return allowed_.size() + denied_.size();
    }

private:
    SeccompValidator() = default;
    ~SeccompValidator() = default;
    SeccompValidator(const SeccompValidator&) = delete;
    SeccompValidator& operator=(const SeccompValidator&) = delete;
    SeccompValidator(SeccompValidator&&) = delete;
    SeccompValidator& operator=(SeccompValidator&&) = delete;

    mutable std::mutex mutex_;
    bool enabled_ = false;
    std::string config_;
    std::set<int> allowed_;
    std::set<int> denied_;
};

} // namespace nexus::utility::container
