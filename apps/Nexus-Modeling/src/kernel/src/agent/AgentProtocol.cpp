// ─────────────────────────────────────────────────────────────────────────────
//  Nexus Agent Protocol — Serialization Implementation
// ─────────────────────────────────────────────────────────────────────────────
#include <nexus/agent/AgentProtocol.h>

#include <algorithm>
#include <charconv>
#include <cctype>

namespace nexus::agent {

namespace {

constexpr const char* operationNames[] = {
    "noop",       "create_box",     "create_cylinder", "create_sphere",
    "create_cone","extrude",        "revolve",         "fillet",
    "chamfer",    "boolean_union",  "boolean_diff",    "boolean_intersect",
    "shell",      "draft",          "pattern_linear",  "pattern_circular",
    "mirror",     "transform",      "delete",          "query",
    "add_constraint","solve_constraints","select","deselect",
    "undo",       "redo",
};

[[nodiscard]] std::string trimCopy(std::string_view text)
{
    size_t begin = 0;
    size_t end = text.size();
    while (begin < end && std::isspace(static_cast<unsigned char>(text[begin]))) ++begin;
    while (end > begin && std::isspace(static_cast<unsigned char>(text[end - 1]))) --end;
    return std::string(text.substr(begin, end - begin));
}

[[nodiscard]] std::vector<std::string> splitLines(std::string_view text)
{
    std::vector<std::string> lines;
    std::string current;
    for (char c : text) {
        if (c == '\n') { lines.push_back(trimCopy(current)); current.clear(); }
        else if (c != '\r') { current.push_back(c); }
    }
    auto trimmed = trimCopy(current);
    if (!trimmed.empty()) lines.push_back(std::move(trimmed));
    return lines;
}

} // namespace

AgentOperation parseOperation(std::string_view name) noexcept
{
    constexpr size_t count = sizeof(operationNames) / sizeof(operationNames[0]);
    for (size_t i = 0; i < count; ++i) {
        if (name == operationNames[i]) return static_cast<AgentOperation>(i);
    }
    return AgentOperation::NoOp;
}

const char* operationName(AgentOperation op) noexcept
{
    const auto idx = static_cast<size_t>(op);
    constexpr size_t count = sizeof(operationNames) / sizeof(operationNames[0]);
    if (idx >= count) return "noop";
    return operationNames[idx];
}

AgentCommand parseCommand(std::string_view text)
{
    AgentCommand cmd;
    auto lines = splitLines(text);
    if (lines.empty()) return cmd;

    cmd.operation = parseOperation(lines[0]);

    for (size_t i = 1; i < lines.size(); ++i) {
        const auto& line = lines[i];
        auto eq = line.find('=');
        if (eq == std::string::npos || eq == 0 || eq + 1 >= line.size()) continue;
        cmd.parameters[line.substr(0, eq)] = line.substr(eq + 1);
    }
    return cmd;
}

std::string serializeCommand(const AgentCommand& command)
{
    std::string out;
    out += operationName(command.operation);
    out += '\n';
    for (const auto& [key, value] : command.parameters) {
        out += key;
        out += '=';
        out += value;
        out += '\n';
    }
    return out;
}

std::string serializeResponse(const AgentResponse& response)
{
    std::string out;
    out += "success=";
    out += response.success ? "true" : "false";
    out += '\n';
    if (!response.errorMessage.empty()) {
        out += "error=";
        out += response.errorMessage;
        out += '\n';
    }
    out += "report_count=";
    out += std::to_string(response.stateReports.size());
    out += '\n';
    for (const auto& report : response.stateReports) {
        out += "report_valid=";
        out += report.valid ? "true" : "false";
        out += '\n';
        for (const auto& msg : report.messages) {
            out += "msg=";
            out += msg;
            out += '\n';
        }
        for (const auto& change : report.topologyChanges) {
            out += "change=";
            out += (change.type == TopologyChange::ChangeType::Created) ? "created" :
                    (change.type == TopologyChange::ChangeType::Destroyed) ? "destroyed" : "modified";
            out += ':';
            out += change.elementType;
            for (auto idx : change.indices) {
                out += ':';
                out += std::to_string(idx);
            }
            out += '\n';
        }
    }
    return out;
}

AgentResponse parseResponse(std::string_view text)
{
    AgentResponse response;
    response.success = false;
    auto lines = splitLines(text);
    for (const auto& line : lines) {
        auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        auto key = line.substr(0, eq);
        auto val = line.substr(eq + 1);
        if (key == "success") response.success = (val == "true");
        else if (key == "error") response.errorMessage = val;
    }
    return response;
}

} // namespace nexus::agent
