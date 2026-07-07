#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <atomic>
#include <mutex>

namespace nexus::utility::interop {

/**
 * @brief Validate callback signatures registered across an FFI boundary.
 */
class CallbackValidator {
public:
    struct Signature {
        std::string name;
        std::string returnType;
        std::vector<std::string> paramTypes;
        std::string callingConvention = "cdecl";

        bool matches(const Signature& o) const {
            return returnType == o.returnType &&
                   paramTypes == o.paramTypes &&
                   callingConvention == o.callingConvention;
        }
    };

    static CallbackValidator& instance() {
        static CallbackValidator inst;
        return inst;
    }

    void initialize(const std::string& config = "") {
        std::lock_guard<std::mutex> lk(mutex_);
        config_ = config;
        enabled_ = true;
    }
    void shutdown() { enabled_ = false; }
    bool isEnabled() const { return enabled_.load(); }

    /// Register the expected signature for a callback slot.
    void expect(const std::string& name, const Signature& sig) {
        std::lock_guard<std::mutex> lk(mutex_);
        expected_[name] = sig;
    }

    /// Validate a provided callback signature against the expectation.
    bool validate(const std::string& name, const Signature& provided) {
        std::lock_guard<std::mutex> lk(mutex_);
        ++validations_;
        auto it = expected_.find(name);
        if (it == expected_.end()) { ++mismatches_; return false; }
        bool ok = it->second.matches(provided);
        if (!ok) ++mismatches_;
        return ok;
    }

    bool hasExpectation(const std::string& name) const {
        std::lock_guard<std::mutex> lk(mutex_);
        return expected_.count(name) > 0;
    }

    std::size_t validations() const { return validations_.load(); }
    std::size_t mismatches() const { return mismatches_.load(); }

    void reset() {
        std::lock_guard<std::mutex> lk(mutex_);
        expected_.clear();
        validations_ = 0;
        mismatches_ = 0;
    }

private:
    CallbackValidator() = default;
    ~CallbackValidator() = default;
    CallbackValidator(const CallbackValidator&) = delete;
    CallbackValidator& operator=(const CallbackValidator&) = delete;

    mutable std::mutex mutex_;
    std::atomic<bool> enabled_{false};
    std::string config_;
    std::unordered_map<std::string, Signature> expected_;
    std::atomic<std::size_t> validations_{0};
    std::atomic<std::size_t> mismatches_{0};
};

} // namespace nexus::utility::interop
