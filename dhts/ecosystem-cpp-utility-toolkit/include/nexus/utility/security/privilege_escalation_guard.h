#pragma once
#include <string>
#include <vector>
#include <chrono>
#include <functional>

namespace nexus::utility::security {

class PrivilegeEscalationGuard {
public:
    struct PrivilegeChange { std::string fromUser; std::string toUser; bool escalated; std::chrono::system_clock::time_point timestamp; };
    using EscalationCallback = std::function<void(const PrivilegeChange&)>;

    static PrivilegeEscalationGuard& instance();
    void initialize();
    void shutdown();
    bool isEnabled() const;

    bool recordUserSwitch(const std::string& from, const std::string& to);
    void onEscalation(EscalationCallback cb);
    std::vector<PrivilegeChange> getHistory() const;
    size_t getEscalationCount() const;
    bool hasEscalated() const;
    void clear();

private:
    PrivilegeEscalationGuard() = default;
    ~PrivilegeEscalationGuard() = default;
    bool enabled_ = false;
    std::vector<PrivilegeChange> history_;
    std::vector<EscalationCallback> callbacks_;
    size_t escalationCount_ = 0;
};
} // namespace nexus::utility::security
