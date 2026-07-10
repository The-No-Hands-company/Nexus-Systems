// ─────────────────────────────────────────────────────────────────────────────
//  Nexus Agent Session — Headless CAD Session Implementation
// ─────────────────────────────────────────────────────────────────────────────
#include <nexus/agent/AgentSession.h>

#include <nexus/cad/CadCommand.h>
#include <nexus/cad/CadOperations.h>
#include <nexus/geometry/BooleanOperation.h>
#include <nexus/geometry/DirectModeling.h>
#include <nexus/geometry/ExtrudeOperation.h>
#include <nexus/geometry/MeshBoolean.h>
#include <nexus/geometry/ModelingShell.h>
#include <nexus/geometry/SurfacePrimitives.h>
#include <nexus/geometry/ProfileRevolve.h>
#include <nexus/geometry/HalfEdgeMesh.h>
#include <nexus/parametric/ParametricSketchProfile.h>
#include <nexus/parametric/FeatureHistory.h>

#include <algorithm>
#include <charconv>
#include <cmath>
#include <sstream>
#include <string>
#include <string_view>

using nexus::parametric::FeatureId;
using nexus::parametric::kInvalidFeatureId;
using nexus::geometry::BooleanOp;

namespace nexus::agent {

//using namespace nexus::cad;
//using namespace nexus::geometry;
//using namespace nexus::parametric;

namespace {

std::optional<float> parseFloatParam(const std::map<std::string, std::string>& params, const std::string& key)
{
    auto it = params.find(key);
    if (it == params.end()) return std::nullopt;
    float value = 0.0f;
    const auto& s = it->second;
    auto result = ::std::from_chars(s.data(), s.data() + s.size(), value);
    if (result.ec != std::errc{}) return std::nullopt;
    return value;
}

std::optional<uint32_t> parseUint32Param(const std::map<std::string, std::string>& params, const std::string& key)
{
    auto it = params.find(key);
    if (it == params.end()) return std::nullopt;
    uint32_t value = 0;
    const auto& s = it->second;
    auto result = ::std::from_chars(s.data(), s.data() + s.size(), value);
    if (result.ec != std::errc{}) return std::nullopt;
    return value;
}

} // namespace

AgentSession::AgentSession() = default;
AgentSession::~AgentSession() = default;

AgentResponse AgentSession::execute(const AgentCommand& command)
{
    switch (command.operation) {
        case AgentOperation::CreateBox:       return handleCreateBox(command);
        case AgentOperation::CreateCylinder:  return handleCreateCylinder(command);
        case AgentOperation::CreateSphere:    return handleCreateSphere(command);
        case AgentOperation::CreateCone:       return handleCreateCone(command);
        case AgentOperation::Extrude:         return handleExtrude(command);
        case AgentOperation::Revolve:         return handleRevolve(command);
        case AgentOperation::Fillet:          return handleFillet(command);
        case AgentOperation::Chamfer:         return handleChamfer(command);
        case AgentOperation::BooleanUnion:    return handleBoolean(command, AgentOperation::BooleanUnion);
        case AgentOperation::BooleanDifference: return handleBoolean(command, AgentOperation::BooleanDifference);
        case AgentOperation::BooleanIntersection: return handleBoolean(command, AgentOperation::BooleanIntersection);
        case AgentOperation::Transform:       return handleTransform(command);
        case AgentOperation::Delete:          return handleDelete(command);
        case AgentOperation::Query:           return handleQuery(command);
        case AgentOperation::AddConstraint:   return handleAddConstraint(command);
        case AgentOperation::Undo:            return undo();
        case AgentOperation::Redo:            return redo();
        case AgentOperation::Select:          return handleSelect(command);
        case AgentOperation::Deselect:        return handleDeselect(command);
        default: {
            AgentResponse resp;
            resp.success = false;
            resp.errorMessage = "Unsupported operation: ";
            resp.errorMessage += operationName(command.operation);
            return resp;
        }
    }
}

AgentResponse AgentSession::undo()
{
    AgentResponse resp;
    resp.success = m_document.undo();
    if (!resp.success) resp.errorMessage = "Nothing to undo";
    m_notifier.notifyCommandExecuted(AgentCommand{AgentOperation::Undo, {}}, resp);
    return resp;
}

AgentResponse AgentSession::redo()
{
    AgentResponse resp;
    resp.success = m_document.redo();
    if (!resp.success) resp.errorMessage = "Nothing to redo";
    m_notifier.notifyCommandExecuted(AgentCommand{AgentOperation::Redo, {}}, resp);
    return resp;
}

std::string AgentSession::serializeState() const
{
    auto data = m_document.serialize();
    return std::string(reinterpret_cast<const char*>(data.data()), data.size());
}

bool AgentSession::deserializeState(const std::string& data)
{
    return m_document.deserialize(reinterpret_cast<const uint8_t*>(data.data()), data.size());
}

AgentResponse AgentSession::handleCreateBox(const AgentCommand& command)
{
    float w = parseFloatParam(command.parameters, "width").value_or(1.0f);
    float h = parseFloatParam(command.parameters, "height").value_or(1.0f);
    float d = parseFloatParam(command.parameters, "depth").value_or(1.0f);
    auto mesh = nexus::geometry::primitives::makeBox(w, h, d);
    auto sk = nexus::parametric::ParametricSketchFactory::createSketch();
    auto fid = m_document.addSketch(sk);
    auto* node = m_document.history().node(fid);
    if (node) { node->mesh.emplace(std::move(mesh)); node->dirty = false; }
    m_document.markModified();
    AgentResponse resp;
    resp.success = true;
    resp.stateReports.push_back(AgentStateReport{true, {}, {"box created id=" + std::to_string(fid)}});
    m_notifier.notifyCommandExecuted(command, resp);
    return resp;
}

AgentResponse AgentSession::handleCreateCylinder(const AgentCommand& command)
{
    float r = parseFloatParam(command.parameters, "radius").value_or(1.0f);
    float h = parseFloatParam(command.parameters, "height").value_or(2.0f);
    uint32_t segs = parseUint32Param(command.parameters, "segments").value_or(32);
    auto mesh = nexus::geometry::primitives::makeCylinder(r, h, segs);
    auto sk = nexus::parametric::ParametricSketchFactory::createSketch();
    auto fid = m_document.addSketch(sk);
    auto* node = m_document.history().node(fid);
    if (node) { node->mesh.emplace(std::move(mesh)); node->dirty = false; }
    m_document.markModified();
    AgentResponse resp;
    resp.success = true;
    resp.stateReports.push_back(AgentStateReport{true, {}, {"cylinder created id=" + std::to_string(fid)}});
    m_notifier.notifyCommandExecuted(command, resp);
    return resp;
}

AgentResponse AgentSession::handleCreateSphere(const AgentCommand& command)
{
    float r = parseFloatParam(command.parameters, "radius").value_or(1.0f);
    uint32_t segs = parseUint32Param(command.parameters, "segments").value_or(32);
    auto mesh = nexus::geometry::primitives::makeSphere(r, segs, segs);
    auto sk = nexus::parametric::ParametricSketchFactory::createSketch();
    auto fid = m_document.addSketch(sk);
    auto* node = m_document.history().node(fid);
    if (node) { node->mesh.emplace(std::move(mesh)); node->dirty = false; }
    m_document.markModified();
    AgentResponse resp;
    resp.success = true;
    resp.stateReports.push_back(AgentStateReport{true, {}, {"sphere created id=" + std::to_string(fid)}});
    m_notifier.notifyCommandExecuted(command, resp);
    return resp;
}

AgentResponse AgentSession::handleCreateCone(const AgentCommand& command)
{
    float r = parseFloatParam(command.parameters, "radius").value_or(1.0f);
    float h = parseFloatParam(command.parameters, "height").value_or(2.0f);
    uint32_t segs = parseUint32Param(command.parameters, "segments").value_or(32);
    auto mesh = nexus::geometry::primitives::makeCone(r, h, segs);
    auto sk = nexus::parametric::ParametricSketchFactory::createSketch();
    auto fid = m_document.addSketch(sk);
    auto* node = m_document.history().node(fid);
    if (node) { node->mesh.emplace(std::move(mesh)); node->dirty = false; }
    m_document.markModified();
    AgentResponse resp;
    resp.success = true;
    resp.stateReports.push_back(AgentStateReport{true, {}, {"cone created id=" + std::to_string(fid)}});
    m_notifier.notifyCommandExecuted(command, resp);
    return resp;
}

AgentResponse AgentSession::handleExtrude(const AgentCommand& command)
{
    auto featureId = parseUint32Param(command.parameters, "feature_id");
    auto distance = parseFloatParam(command.parameters, "distance").value_or(1.0f);
    if (!featureId) {
        AgentResponse resp;
        resp.success = false;
        resp.errorMessage = "extrude requires feature_id=";
        return resp;
    }
    nexus::geometry::CurveExtrudeDesc desc;
    desc.height = distance;
    auto fid = m_document.addExtrude(*featureId, desc);
    m_document.markModified();
    AgentResponse resp;
    resp.success = (fid != nexus::parametric::kInvalidFeatureId);
    if (resp.success)
        resp.stateReports.push_back(AgentStateReport{true, {}, {"extruded feature " + std::to_string(fid)}});
    else
        resp.errorMessage = "extrude failed";
    m_notifier.notifyCommandExecuted(command, resp);
    return resp;
}

AgentResponse AgentSession::handleRevolve(const AgentCommand& command)
{
    auto featureId = parseUint32Param(command.parameters, "feature_id");
    auto angle = parseFloatParam(command.parameters, "angle").value_or(360.0f);
    if (!featureId) {
        AgentResponse resp;
        resp.success = false;
        resp.errorMessage = "revolve requires feature_id=";
        return resp;
    }
    nexus::geometry::RevolveDesc desc;
    desc.endAngleDeg = angle;
    auto fid = m_document.addRevolve(*featureId, desc);
    m_document.markModified();
    AgentResponse resp;
    resp.success = (fid != nexus::parametric::kInvalidFeatureId);
    if (resp.success)
        resp.stateReports.push_back(AgentStateReport{true, {}, {"revolved feature " + std::to_string(fid)}});
    else
        resp.errorMessage = "revolve failed";
    m_notifier.notifyCommandExecuted(command, resp);
    return resp;
}

AgentResponse AgentSession::handleFillet(const AgentCommand& command)
{
    auto featureId = parseUint32Param(command.parameters, "feature_id");
    auto radius = parseFloatParam(command.parameters, "radius").value_or(0.5f);
    if (!featureId) {
        AgentResponse resp;
        resp.success = false;
        resp.errorMessage = "fillet requires feature_id=";
        return resp;
    }
    auto* node = m_document.history().node(*featureId);
    if (!node || !node->mesh) {
        AgentResponse resp;
        resp.success = false;
        resp.errorMessage = "feature not found or has no mesh";
        return resp;
    }
    std::vector<uint32_t> edges;
    auto edgeStr = command.parameters.count("edges") ? command.parameters.at("edges") : "";
    if (!edgeStr.empty()) {
        std::istringstream iss(edgeStr);
        std::string token;
        while (std::getline(iss, token, ',')) {
            auto val = parseUint32Param({{"v", token}}, "v");
            if (val) edges.push_back(*val);
        }
    }
    auto cmd = std::make_unique<nexus::cad::FilletCommand>(*featureId, edges, radius);
    bool ok = m_document.executeCommand(std::move(cmd));
    m_document.markModified();
    AgentResponse resp;
    resp.success = ok;
    if (!ok) resp.errorMessage = "fillet failed";
    else resp.stateReports.push_back(AgentStateReport{true, {}, {"filleted feature " + std::to_string(*featureId)}});
    m_notifier.notifyCommandExecuted(command, resp);
    return resp;
}

AgentResponse AgentSession::handleChamfer(const AgentCommand& command)
{
    auto featureId = parseUint32Param(command.parameters, "feature_id");
    auto distance = parseFloatParam(command.parameters, "distance").value_or(0.5f);
    if (!featureId) {
        AgentResponse resp;
        resp.success = false;
        resp.errorMessage = "chamfer requires feature_id=";
        return resp;
    }
    std::vector<uint32_t> edges;
    auto edgeStr = command.parameters.count("edges") ? command.parameters.at("edges") : "";
    if (!edgeStr.empty()) {
        std::istringstream iss(edgeStr);
        std::string token;
        while (std::getline(iss, token, ',')) {
            auto val = parseUint32Param({{"v", token}}, "v");
            if (val) edges.push_back(*val);
        }
    }
    auto cmd = std::make_unique<nexus::cad::ChamferCommand>(*featureId, edges, distance);
    bool ok = m_document.executeCommand(std::move(cmd));
    m_document.markModified();
    AgentResponse resp;
    resp.success = ok;
    if (!ok) resp.errorMessage = "chamfer failed";
    else resp.stateReports.push_back(AgentStateReport{true, {}, {"chamfered feature " + std::to_string(*featureId)}});
    m_notifier.notifyCommandExecuted(command, resp);
    return resp;
}

AgentResponse AgentSession::handleBoolean(const AgentCommand& command, AgentOperation op)
{
    auto idA = parseUint32Param(command.parameters, "feature_a");
    auto idB = parseUint32Param(command.parameters, "feature_b");
    if (!idA || !idB) {
        AgentResponse resp;
        resp.success = false;
        resp.errorMessage = "boolean requires feature_a= and feature_b=";
        return resp;
    }
    auto* nodeA = m_document.history().node(*idA);
    auto* nodeB = m_document.history().node(*idB);
    if (!nodeA || !nodeA->mesh || !nodeB || !nodeB->mesh) {
        AgentResponse resp;
        resp.success = false;
        resp.errorMessage = "boolean requires two valid features with meshes";
        return resp;
    }
nexus::geometry::BooleanOp bop = (op == AgentOperation::BooleanUnion) ? nexus::geometry::BooleanOp::Union :
                (op == AgentOperation::BooleanDifference) ? nexus::geometry::BooleanOp::Difference :
                nexus::geometry::BooleanOp::Intersection;
    auto result = nexus::geometry::MeshBoolean::compute(*nodeA->mesh, *nodeB->mesh, bop);
    if (!result.ok) {
        AgentResponse resp;
        resp.success = false;
        resp.errorMessage = result.error;
        return resp;
    }
    auto sk = nexus::parametric::ParametricSketchFactory::createSketch();
    auto fid = m_document.addSketch(sk);
    auto* node = m_document.history().node(fid);
    if (node) { node->mesh.emplace(std::move(result.result)); node->dirty = false; }
    m_document.markModified();
    AgentResponse resp;
    resp.success = true;
    resp.stateReports.push_back(AgentStateReport{true, {}, {"boolean result id=" + std::to_string(fid)}});
    m_notifier.notifyCommandExecuted(command, resp);
    return resp;
}

AgentResponse AgentSession::handleTransform(const AgentCommand& command)
{
    auto featureId = parseUint32Param(command.parameters, "feature_id");
    auto tx = parseFloatParam(command.parameters, "tx").value_or(0.0f);
    auto ty = parseFloatParam(command.parameters, "ty").value_or(0.0f);
    auto tz = parseFloatParam(command.parameters, "tz").value_or(0.0f);
    auto sx = parseFloatParam(command.parameters, "sx").value_or(1.0f);
    auto sy = parseFloatParam(command.parameters, "sy").value_or(1.0f);
    auto sz = parseFloatParam(command.parameters, "sz").value_or(1.0f);
    if (!featureId) {
        AgentResponse resp;
        resp.success = false;
        resp.errorMessage = "transform requires feature_id=";
        return resp;
    }
    auto* node = m_document.history().node(*featureId);
    if (!node || !node->mesh) {
        AgentResponse resp;
        resp.success = false;
        resp.errorMessage = "feature not found";
        return resp;
    }
    auto saved = *node->mesh;
    auto pos = node->mesh->attributes().positions();
    for (auto& v : pos) {
        v.x = v.x * sx + tx;
        v.y = v.y * sy + ty;
        v.z = v.z * sz + tz;
    }
    node->mesh->attributes().setPositions(std::move(pos));
    auto cmd = std::make_unique<nexus::cad::TransformCommand>(*featureId, std::move(saved));
    m_document.executeCommand(std::move(cmd));
    m_document.markModified();
    AgentResponse resp;
    resp.success = true;
    resp.stateReports.push_back(AgentStateReport{true, {}, {"transformed feature " + std::to_string(*featureId)}});
    m_notifier.notifyCommandExecuted(command, resp);
    return resp;
}

AgentResponse AgentSession::handleDelete(const AgentCommand& command)
{
    auto featureId = parseUint32Param(command.parameters, "feature_id");
    if (!featureId) {
        AgentResponse resp;
        resp.success = false;
        resp.errorMessage = "delete requires feature_id=";
        return resp;
    }
    bool ok = m_document.deleteFeature(*featureId);
    m_document.markModified();
    AgentResponse resp;
    resp.success = ok;
    if (!ok) resp.errorMessage = "delete failed";
    else resp.stateReports.push_back(AgentStateReport{true, {}, {"deleted feature " + std::to_string(*featureId)}});
    m_notifier.notifyCommandExecuted(command, resp);
    return resp;
}

AgentResponse AgentSession::handleQuery(const AgentCommand& command)
{
    AgentResponse resp;
    resp.success = true;
    AgentStateReport report;
    report.valid = true;
    std::ostringstream oss;
    oss << "feature_count=" << m_document.history().featureCount();
    report.messages.push_back(oss.str());
    for (nexus::parametric::FeatureId i = 1; i <= static_cast<nexus::parametric::FeatureId>(m_document.history().featureCount()); ++i) {
        auto* node = m_document.history().node(i);
        if (!node || node->deleted) continue;
        std::ostringstream oss2;
        oss2 << "feature=" << node->id << " deleted=" << (node->deleted ? "true" : "false")
             << " hidden=" << (node->hidden ? "true" : "false");
        if (node->mesh) {
            oss2 << " vertices=" << node->mesh->attributes().vertexCount()
                 << " faces=" << node->mesh->topology().faceCount();
        }
        report.messages.push_back(oss2.str());
    }
    resp.stateReports.push_back(std::move(report));
    m_notifier.notifyCommandExecuted(command, resp);
    return resp;
}

AgentResponse AgentSession::handleAddConstraint(const AgentCommand& command)
{
    (void)command;
    AgentResponse resp;
    resp.success = false;
    resp.errorMessage = "add_constraint requires a constraint solver session (not yet wired)";
    return resp;
}

AgentResponse AgentSession::handleSelect(const AgentCommand& command)
{
    auto featureId = parseUint32Param(command.parameters, "feature_id");
    if (featureId) {
        m_document.selection().addFace(*featureId);
    }
    AgentResponse resp;
    resp.success = true;
    m_notifier.notifyCommandExecuted(command, resp);
    return resp;
}

AgentResponse AgentSession::handleDeselect(const AgentCommand& command)
{
    (void)command;
    m_document.selection().clear();
    AgentResponse resp;
    resp.success = true;
    m_notifier.notifyCommandExecuted(command, resp);
    return resp;
}

AgentStateReport AgentSession::computeStateReport(nexus::geometry::Mesh* before, nexus::geometry::Mesh* after)
{
    AgentStateReport report;
    report.valid = true;
    if (before && after) {
        auto beforeFaces = before->topology().faceCount();
        auto afterFaces = after->topology().faceCount();
        if (afterFaces > beforeFaces) {
            TopologyChange change;
            change.type = TopologyChange::ChangeType::Created;
            change.elementType = "Face";
            change.indices.push_back(static_cast<uint32_t>(afterFaces - beforeFaces));
            report.topologyChanges.push_back(std::move(change));
        } else if (afterFaces < beforeFaces) {
            TopologyChange change;
            change.type = TopologyChange::ChangeType::Destroyed;
            change.elementType = "Face";
            change.indices.push_back(static_cast<uint32_t>(beforeFaces - afterFaces));
            report.topologyChanges.push_back(std::move(change));
        }
        if (afterFaces != beforeFaces) {
            report.messages.push_back("topology changed: " + std::to_string(beforeFaces) +
                                      " -> " + std::to_string(afterFaces) + " faces");
        }
    }
    return report;
}

} // namespace nexus::agent
