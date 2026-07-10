// ─────────────────────────────────────────────────────────────────────────────
//  Nexus Agent Observer — Mutation Notification Implementation
// ─────────────────────────────────────────────────────────────────────────────
#include <nexus/agent/AgentObserver.h>

#include <algorithm>

namespace nexus::agent {

void AgentMutationNotifier::addListener(AgentMutationListener* listener)
{
    if (!listener) return;
    auto it = std::find(m_listeners.begin(), m_listeners.end(), listener);
    if (it == m_listeners.end()) m_listeners.push_back(listener);
}

void AgentMutationNotifier::removeListener(AgentMutationListener* listener)
{
    auto it = std::find(m_listeners.begin(), m_listeners.end(), listener);
    if (it != m_listeners.end()) m_listeners.erase(it);
}

void AgentMutationNotifier::notifyTopologyChanged(const AgentStateReport& report)
{
    for (auto* listener : m_listeners) listener->onTopologyChanged(report);
}

void AgentMutationNotifier::notifyCommandExecuted(const AgentCommand& command, const AgentResponse& response)
{
    for (auto* listener : m_listeners) listener->onCommandExecuted(command, response);
}

} // namespace nexus::agent
