#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <atomic>
#include <mutex>

namespace nexus::utility::orchestration {

/**
 * @brief Register and trigger automated remediation actions for alert types.
 */
class AutomatedRemediation {
public:
    using Action = std::function<bool(const std::string& context)>;

    struct RemediationRecord {
        std::string alertType;
        std::string context;
        bool success = false;
    };

    static AutomatedRemediation& instance() {
        static AutomatedRemediation inst;
        return inst;
    }

    void initialize(const std::string& config = "") {
        std::lock_guard<std::mutex> lk(mutex_);
        config_ = config;
        enabled_ = true;
    }
    void shutdown() { enabled_ = false; }
    bool isEnabled() const { return enabled_.load(); }

    void registerAction(const std::string& alertType, Action action) {
        std::lock_guard<std::mutex> lk(mutex_);
        actions_[alertType] = std::move(action);
    }

    bool hasAction(const std::string& alertType) const {
        std::lock_guard<std::mutex> lk(mutex_);
        return actions_.count(alertType) > 0;
    }

    bool remediate(const std::string& alertType, const std::string& context = "") {
        Action action;
        {
            std::lock_guard<std::mutex> lk(mutex_);
            auto it = actions_.find(alertType);
            if (it == actions_.end()) return false;
            action = it->second;
        }
        bool ok = action ? action(context) : false;
        {
            std::lock_guard<std::mutex> lk(mutex_);
            history_.push_back({alertType, context, ok});
            if (ok) ++succeeded_; else ++failed_;
        }
        return ok;
    }

    std::vector<RemediationRecord> history() const {
        std::lock_guard<std::mutex> lk(mutex_);
        return history_;
    }

    std::size_t succeeded() const { return succeeded_.load(); }
    std::size_t failed() const { return failed_.load(); }

    void reset() {
        std::lock_guard<std::mutex> lk(mutex_);
        history_.clear();
        succeeded_ = 0;
        failed_ = 0;
    }

private:
    AutomatedRemediation() = default;
    ~AutomatedRemediation() = default;
    AutomatedRemediation(const AutomatedRemediation&) = delete;
    AutomatedRemediation& operator=(const AutomatedRemediation&) = delete;

    mutable std::mutex mutex_;
    std::atomic<bool> enabled_{false};
    std::string config_;
    std::unordered_map<std::string, Action> actions_;
    std::vector<RemediationRecord> history_;
    std::atomic<std::size_t> succeeded_{0};
    std::atomic<std::size_t> failed_{0};
};

} // namespace nexus::utility::orchestration
