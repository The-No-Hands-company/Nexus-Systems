#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  Nexus Agent Observer — Mutation Notification System
//
//  Emits structured topology-change events whenever the kernel mutates geometry.
//  AI/scripting agents subscribe to receive state-delta reports after each op.
// ─────────────────────────────────────────────────────────────────────────────

#include <nexus/agent/AgentProtocol.h>

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace nexus::agent {

class AgentMutationListener {
public:
    virtual ~AgentMutationListener() = default;
    virtual void onTopologyChanged(const AgentStateReport& report) = 0;
    virtual void onCommandExecuted(const AgentCommand& command, const AgentResponse& response) = 0;
};

class AgentMutationNotifier {
public:
    void addListener(AgentMutationListener* listener);
    void removeListener(AgentMutationListener* listener);

    void notifyTopologyChanged(const AgentStateReport& report);
    void notifyCommandExecuted(const AgentCommand& command, const AgentResponse& response);

    [[nodiscard]] bool hasListeners() const noexcept { return !m_listeners.empty(); }
    [[nodiscard]] size_t listenerCount() const noexcept { return m_listeners.size(); }

private:
    std::vector<AgentMutationListener*> m_listeners;
};

} // namespace nexus::agent
