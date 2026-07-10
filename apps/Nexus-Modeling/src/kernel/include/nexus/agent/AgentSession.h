#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  Nexus Agent Session — Headless CAD Session for External Agents
//
//  Wraps a CadDocument with a command-oriented interface. Every mutation
//  produces an AgentStateReport describing what topology changed.
//  Designed for headless operation — no window, no GLFW, no Vulkan.
// ─────────────────────────────────────────────────────────────────────────────

#include <nexus/agent/AgentObserver.h>
#include <nexus/agent/AgentProtocol.h>
#include <nexus/cad/CadDocument.h>
#include <nexus/geometry/Mesh.h>

#include <memory>
#include <string>
#include <vector>

namespace nexus::agent {

class AgentSession {
public:
    AgentSession();
    ~AgentSession();

    AgentSession(const AgentSession&) = delete;
    AgentSession& operator=(const AgentSession&) = delete;
    AgentSession(AgentSession&&) noexcept = default;
    AgentSession& operator=(AgentSession&&) noexcept = default;

    [[nodiscard]] AgentResponse execute(const AgentCommand& command);
    [[nodiscard]] AgentResponse undo();
    [[nodiscard]] AgentResponse redo();

    [[nodiscard]] nexus::cad::CadDocument& document() noexcept { return m_document; }
    [[nodiscard]] const nexus::cad::CadDocument& document() const noexcept { return m_document; }

    [[nodiscard]] AgentMutationNotifier& notifier() noexcept { return m_notifier; }

    [[nodiscard]] std::string serializeState() const;
    bool deserializeState(const std::string& data);

private:
    AgentResponse handleCreateBox(const AgentCommand& command);
    AgentResponse handleCreateCylinder(const AgentCommand& command);
    AgentResponse handleCreateSphere(const AgentCommand& command);
    AgentResponse handleCreateCone(const AgentCommand& command);
    AgentResponse handleExtrude(const AgentCommand& command);
    AgentResponse handleRevolve(const AgentCommand& command);
    AgentResponse handleFillet(const AgentCommand& command);
    AgentResponse handleChamfer(const AgentCommand& command);
    AgentResponse handleBoolean(const AgentCommand& command, AgentOperation op);
    AgentResponse handleTransform(const AgentCommand& command);
    AgentResponse handleDelete(const AgentCommand& command);
    AgentResponse handleQuery(const AgentCommand& command);
    AgentResponse handleAddConstraint(const AgentCommand& command);
    AgentResponse handleSelect(const AgentCommand& command);
    AgentResponse handleDeselect(const AgentCommand& command);

    [[nodiscard]] AgentStateReport computeStateReport(nexus::geometry::Mesh* before, nexus::geometry::Mesh* after);

    nexus::cad::CadDocument m_document;
    AgentMutationNotifier m_notifier;
    std::unique_ptr<nexus::geometry::Mesh> m_preMutationSnapshot;
};

} // namespace nexus::agent
