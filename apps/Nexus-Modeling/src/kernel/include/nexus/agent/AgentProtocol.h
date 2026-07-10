#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  Nexus Agent Protocol — External-Agent Command & Response Wire Format
//
//  Provides a serializable command protocol and deterministic state-report
//  mechanism for AI/scripting agents to drive the geometry kernel safely.
// ─────────────────────────────────────────────────────────────────────────────

#include <cstdint>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace nexus::agent {

enum class AgentOperation : uint8_t {
    NoOp,
    CreateBox,
    CreateCylinder,
    CreateSphere,
    CreateCone,
    Extrude,
    Revolve,
    Fillet,
    Chamfer,
    BooleanUnion,
    BooleanDifference,
    BooleanIntersection,
    Shell,
    Draft,
    PatternLinear,
    PatternCircular,
    Mirror,
    Transform,
    Delete,
    Query,
    AddConstraint,
    SolveConstraints,
    Select,
    Deselect,
    Undo,
    Redo,
    Count,
};

struct AgentCommand {
    AgentOperation operation = AgentOperation::NoOp;
    std::map<std::string, std::string> parameters;
};

struct TopologyChange {
    enum class ChangeType : uint8_t { Created, Destroyed, Modified };
    ChangeType type = ChangeType::Created;
    std::string elementType;  // "Face", "Edge", "Vertex"
    std::vector<uint32_t> indices;
};

struct AgentStateReport {
    bool valid = true;
    std::vector<TopologyChange> topologyChanges;
    std::vector<std::string> messages;
};

struct AgentResponse {
    bool success = false;
    std::string errorMessage;
    std::vector<AgentStateReport> stateReports;
};

AgentOperation parseOperation(std::string_view name) noexcept;
const char* operationName(AgentOperation op) noexcept;
AgentCommand parseCommand(std::string_view text);
std::string serializeCommand(const AgentCommand& command);
std::string serializeResponse(const AgentResponse& response);
AgentResponse parseResponse(std::string_view text);

} // namespace nexus::agent
