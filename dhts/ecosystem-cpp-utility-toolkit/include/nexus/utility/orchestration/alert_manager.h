#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <atomic>
#include <mutex>
#include <chrono>
#include <cstdint>

namespace nexus::utility::orchestration {

/**
 * @brief Manage diagnostic alerts with severity, acknowledgement and resolution.
 */
class AlertManager {
public:
    enum class Severity { Info, Warning, Error, Critical };
    enum class State { Active, Acknowledged, Resolved };

    struct Alert {
        std::uint64_t id = 0;
        std::string source;
        std::string message;
        Severity severity = Severity::Info;
        State state = State::Active;
        std::chrono::system_clock::time_point raised;
    };

    static AlertManager& instance() {
        static AlertManager inst;
        return inst;
    }

    void initialize(const std::string& config = "") {
        std::lock_guard<std::mutex> lk(mutex_);
        config_ = config;
        enabled_ = true;
    }
    void shutdown() { enabled_ = false; }
    bool isEnabled() const { return enabled_.load(); }

    std::uint64_t raise(const std::string& source, const std::string& message, Severity sev) {
        std::lock_guard<std::mutex> lk(mutex_);
        std::uint64_t id = ++nextId_;
        alerts_[id] = Alert{id, source, message, sev, State::Active,
                            std::chrono::system_clock::now()};
        return id;
    }

    bool acknowledge(std::uint64_t id) {
        std::lock_guard<std::mutex> lk(mutex_);
        auto it = alerts_.find(id);
        if (it == alerts_.end() || it->second.state == State::Resolved) return false;
        it->second.state = State::Acknowledged;
        return true;
    }

    bool resolve(std::uint64_t id) {
        std::lock_guard<std::mutex> lk(mutex_);
        auto it = alerts_.find(id);
        if (it == alerts_.end()) return false;
        it->second.state = State::Resolved;
        return true;
    }

    std::vector<Alert> activeAlerts() const {
        std::lock_guard<std::mutex> lk(mutex_);
        std::vector<Alert> out;
        for (const auto& [id, a] : alerts_)
            if (a.state != State::Resolved) out.push_back(a);
        return out;
    }

    std::size_t countBySeverity(Severity sev) const {
        std::lock_guard<std::mutex> lk(mutex_);
        std::size_t n = 0;
        for (const auto& [id, a] : alerts_)
            if (a.severity == sev && a.state != State::Resolved) ++n;
        return n;
    }

    void reset() {
        std::lock_guard<std::mutex> lk(mutex_);
        alerts_.clear();
        nextId_ = 0;
    }

private:
    AlertManager() = default;
    ~AlertManager() = default;
    AlertManager(const AlertManager&) = delete;
    AlertManager& operator=(const AlertManager&) = delete;

    mutable std::mutex mutex_;
    std::atomic<bool> enabled_{false};
    std::string config_;
    std::unordered_map<std::uint64_t, Alert> alerts_;
    std::uint64_t nextId_ = 0;
};

} // namespace nexus::utility::orchestration
