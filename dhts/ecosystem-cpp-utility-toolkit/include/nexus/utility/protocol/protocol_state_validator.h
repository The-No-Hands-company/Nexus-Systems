#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <atomic>
#include <mutex>

namespace nexus::utility::protocol {

/**
 * @brief Validate protocol state machine transitions.
 */
class ProtocolStateValidator {
public:
    struct Transition {
        std::string from;
        std::string to;
        bool valid = true;
    };

    static ProtocolStateValidator& instance() {
        static ProtocolStateValidator inst;
        return inst;
    }

    void initialize(const std::string& config = "") {
        std::lock_guard<std::mutex> lk(mutex_);
        config_ = config;
        enabled_ = true;
    }
    void shutdown() { enabled_ = false; }
    bool isEnabled() const { return enabled_.load(); }

    void setInitialState(const std::string& state) {
        std::lock_guard<std::mutex> lk(mutex_);
        current_ = state;
        initial_ = state;
    }

    void addTransition(const std::string& from, const std::string& to) {
        std::lock_guard<std::mutex> lk(mutex_);
        allowed_[from].insert(to);
    }

    bool canTransition(const std::string& from, const std::string& to) const {
        std::lock_guard<std::mutex> lk(mutex_);
        auto it = allowed_.find(from);
        return it != allowed_.end() && it->second.count(to) > 0;
    }

    /// Attempt a transition to `to` from the current state; records validity.
    bool transition(const std::string& to) {
        std::lock_guard<std::mutex> lk(mutex_);
        bool ok = allowed_.count(current_) && allowed_[current_].count(to);
        history_.push_back({current_, to, ok});
        if (ok) current_ = to;
        else ++violations_;
        return ok;
    }

    std::string currentState() const {
        std::lock_guard<std::mutex> lk(mutex_);
        return current_;
    }

    std::size_t violations() const { return violations_.load(); }

    std::vector<Transition> history() const {
        std::lock_guard<std::mutex> lk(mutex_);
        return history_;
    }

    void reset() {
        std::lock_guard<std::mutex> lk(mutex_);
        current_ = initial_;
        history_.clear();
        violations_ = 0;
    }

private:
    ProtocolStateValidator() = default;
    ~ProtocolStateValidator() = default;
    ProtocolStateValidator(const ProtocolStateValidator&) = delete;
    ProtocolStateValidator& operator=(const ProtocolStateValidator&) = delete;

    mutable std::mutex mutex_;
    std::atomic<bool> enabled_{false};
    std::string config_;
    std::string current_;
    std::string initial_;
    std::unordered_map<std::string, std::unordered_set<std::string>> allowed_;
    std::vector<Transition> history_;
    std::atomic<std::size_t> violations_{0};
};

} // namespace nexus::utility::protocol
