#include "nexus/scene/NodeScene.h"

#include <algorithm>
#include <iomanip>
#include <locale>
#include <sstream>
#include <utility>
#include <string_view>

namespace nexus {

NodeScene::NodeScene()  = default;
NodeScene::~NodeScene() = default;

// ── Node management ───────────────────────────────────────────────────────────

SceneNodeId NodeScene::addNode(std::string name, NodeKind kind) {
    if (m_nameToId.count(name)) {
        return kInvalidSceneNodeId;
    }
    const SceneNodeId id = m_graph.addNode(kind, name);
    m_nameToId.emplace(name, id);
    m_idToName.emplace(id, std::move(name));
    return id;
}

bool NodeScene::removeNode(SceneNodeId id) noexcept {
    // Unlink id from its parent's children list (edge cleanup delegated to graph below).
    auto parentIt = m_parentOf.find(id);
    if (parentIt != m_parentOf.end()) {
        auto& siblings = m_childrenOf[parentIt->second];
        siblings.erase(std::remove(siblings.begin(), siblings.end(), id), siblings.end());
        m_parentOf.erase(parentIt);
    }
    // Orphan id's children: clear their parent pointers (edges are removed by the graph).
    auto childrenIt = m_childrenOf.find(id);
    if (childrenIt != m_childrenOf.end()) {
        for (SceneNodeId ch : childrenIt->second) {
            m_parentOf.erase(ch);
        }
        m_childrenOf.erase(childrenIt);
    }
    // Remove node from graph (also purges all its edges).
    if (!m_graph.removeNode(id)) {
        return false;
    }
    auto nameIt = m_idToName.find(id);
    if (nameIt != m_idToName.end()) {
        m_nameToId.erase(nameIt->second);
        m_idToName.erase(nameIt);
    }
    return true;
}

SceneNodeId NodeScene::nodeByName(const std::string& name) const noexcept {
    auto it = m_nameToId.find(name);
    return it != m_nameToId.end() ? it->second : kInvalidSceneNodeId;
}

bool NodeScene::hasNode(SceneNodeId id) const noexcept {
    return m_graph.hasNode(id);
}

std::string NodeScene::nodeName(SceneNodeId id) const {
    auto it = m_idToName.find(id);
    return it != m_idToName.end() ? it->second : "";
}

NodeKind NodeScene::nodeKind(SceneNodeId id) const noexcept {
    return m_graph.nodeKind(id);
}

std::size_t NodeScene::nodeCount() const noexcept {
    return m_graph.nodeCount();
}

// ── Dependency edges ──────────────────────────────────────────────────────────

bool NodeScene::connect(SceneNodeId srcNode, SceneNodeId dstNode) {
    return m_graph.connect(srcNode, dstNode);
}

bool NodeScene::disconnect(SceneNodeId srcNode, SceneNodeId dstNode) noexcept {
    return m_graph.disconnect(srcNode, dstNode);
}

bool NodeScene::isConnected(SceneNodeId srcNode, SceneNodeId dstNode) const noexcept {
    return m_graph.isConnected(srcNode, dstNode);
}

// ── Asset slots ───────────────────────────────────────────────────────────────

bool NodeScene::setAsset(SceneNodeId id, NodePayload payload) {
    return m_graph.setNodeOutputPayload(id, std::move(payload));
}

bool NodeScene::clearAsset(SceneNodeId id) noexcept {
    return m_graph.clearNodeOutputPayload(id);
}

const NodePayload* NodeScene::asset(SceneNodeId id) const noexcept {
    return m_graph.nodeOutputPayload(id);
}

bool NodeScene::setReconstructionDiagnostic(
    SceneNodeId id,
    NodePayload::ReconstructionDiagnostic diagnostic) {
    NodePayload payload;
    payload.value = diagnostic;
    return m_graph.setNodeOutputPayload(id, std::move(payload));
}

const NodePayload::ReconstructionDiagnostic* NodeScene::reconstructionDiagnostic(SceneNodeId id) const noexcept {
    const NodePayload* payload = m_graph.nodeOutputPayload(id);
    if (!payload) {
        return nullptr;
    }
    return payload->reconstructionDiagnostic();
}

bool NodeScene::reconstructionPassesAlpha(SceneNodeId id) const noexcept {
    return reconstructionPassesAlpha(id, m_reconstructionThresholds);
}

bool NodeScene::reconstructionPassesAlpha(
    SceneNodeId id,
    float maxResidual,
    float minConfidence) const noexcept {
    const NodePayload::ReconstructionDiagnostic* d = reconstructionDiagnostic(id);
    return d ? d->passesAlpha(maxResidual, minConfidence) : false;
}

bool NodeScene::reconstructionPassesAlpha(
    SceneNodeId id,
    const ReconstructionQualityThresholds& thresholds) const noexcept {
    return reconstructionPassesAlpha(id, thresholds.maxResidual, thresholds.minConfidence);
}

void NodeScene::setReconstructionQualityThresholds(ReconstructionQualityThresholds thresholds) noexcept {
    m_reconstructionThresholds = thresholds;
}

ReconstructionQualityThresholds NodeScene::reconstructionQualityThresholds() const noexcept {
    return m_reconstructionThresholds;
}

std::string NodeScene::reconstructionQualitySummary(SceneNodeId id) const {
    return reconstructionQualitySummary(id, m_reconstructionThresholds);
}

std::string NodeScene::reconstructionQualitySummary(
    SceneNodeId id,
    float maxResidual,
    float minConfidence) const {
    const ReconstructionAssessmentSnapshot snapshot = reconstructionAssessment(
        id,
        ReconstructionQualityThresholds{.maxResidual = maxResidual, .minConfidence = minConfidence});
    if (snapshot.state == ReconstructionQualityState::UnavailableUnknownNode) {
        return "reconstruction_status=unavailable node=" + std::to_string(id) + " reason=unknown_node";
    }
    if (snapshot.state == ReconstructionQualityState::UnavailableMissingDiagnostic) {
        return "reconstruction_status=unavailable node=" + std::to_string(id) + " reason=missing_diagnostic";
    }

    if (!snapshot.metrics) {
        return "reconstruction_status=unavailable node=" + std::to_string(id) + " reason=missing_diagnostic";
    }

    const NodePayload::ReconstructionDiagnostic& d = *snapshot.metrics;
    std::ostringstream oss;
    oss.imbue(std::locale::classic());
    oss << std::fixed << std::setprecision(3);
    oss << "reconstruction_status="
        << (snapshot.state == ReconstructionQualityState::Pass ? "pass" : "fail")
        << " node=" << id
        << " residual=" << d.residual
        << " confidence=" << d.confidence
        << " residual_threshold=" << snapshot.thresholds.maxResidual
        << " confidence_threshold=" << snapshot.thresholds.minConfidence;
    return oss.str();
}

std::string NodeScene::reconstructionQualitySummary(
    SceneNodeId id,
    const ReconstructionQualityThresholds& thresholds) const {
    return reconstructionQualitySummary(id, thresholds.maxResidual, thresholds.minConfidence);
}

ReconstructionAssessmentSnapshot NodeScene::reconstructionAssessment(SceneNodeId id) const noexcept {
    return reconstructionAssessment(id, m_reconstructionThresholds);
}

ReconstructionAssessmentSnapshot NodeScene::reconstructionAssessment(
    SceneNodeId id,
    const ReconstructionQualityThresholds& thresholds) const noexcept {
    ReconstructionAssessmentSnapshot snapshot;
    snapshot.thresholds = thresholds;
    snapshot.state = reconstructionQualityState(id, thresholds);
    if (const NodePayload::ReconstructionDiagnostic* d = reconstructionDiagnostic(id)) {
        snapshot.metrics = *d;
    }
    return snapshot;
}

std::vector<ReconstructionAssessmentEntry> NodeScene::reconstructionAssessments() const {
    return reconstructionAssessments(m_reconstructionThresholds);
}

std::vector<ReconstructionAssessmentEntry> NodeScene::reconstructionAssessments(
    const ReconstructionQualityThresholds& thresholds) const {
    std::vector<SceneNodeId> ids;
    ids.reserve(m_idToName.size());
    for (const auto& [id, _] : m_idToName) {
        if (m_graph.nodeKind(id) == NodeKind::Reconstruction) {
            ids.push_back(id);
        }
    }
    std::sort(ids.begin(), ids.end());

    std::vector<ReconstructionAssessmentEntry> rows;
    rows.reserve(ids.size());
    for (SceneNodeId id : ids) {
        rows.push_back(ReconstructionAssessmentEntry{
            .id = id,
            .name = nodeName(id),
            .snapshot = reconstructionAssessment(id, thresholds),
        });
    }
    return rows;
}

std::vector<std::string> NodeScene::reconstructionAssessmentSummaries() const {
    return reconstructionAssessmentSummaries(m_reconstructionThresholds);
}

std::vector<std::string> NodeScene::reconstructionAssessmentSummaries(
    const ReconstructionQualityThresholds& thresholds) const {
    const std::vector<ReconstructionAssessmentEntry> rows = reconstructionAssessments(thresholds);
    std::vector<std::string> lines;
    lines.reserve(rows.size());

    for (const ReconstructionAssessmentEntry& row : rows) {
        if (row.snapshot.state == ReconstructionQualityState::UnavailableUnknownNode) {
            lines.push_back(
                "reconstruction_status=unavailable node=" + std::to_string(row.id)
                + " name=" + row.name
                + " reason=unknown_node");
            continue;
        }
        if (row.snapshot.state == ReconstructionQualityState::UnavailableMissingDiagnostic || !row.snapshot.metrics) {
            lines.push_back(
                "reconstruction_status=unavailable node=" + std::to_string(row.id)
                + " name=" + row.name
                + " reason=missing_diagnostic");
            continue;
        }

        std::ostringstream oss;
        oss.imbue(std::locale::classic());
        oss << std::fixed << std::setprecision(3);
        oss << "reconstruction_status="
            << (row.snapshot.state == ReconstructionQualityState::Pass ? "pass" : "fail")
            << " node=" << row.id
            << " name=" << row.name
            << " residual=" << row.snapshot.metrics->residual
            << " confidence=" << row.snapshot.metrics->confidence
            << " residual_threshold=" << row.snapshot.thresholds.maxResidual
            << " confidence_threshold=" << row.snapshot.thresholds.minConfidence;
        lines.push_back(oss.str());
    }

    return lines;
}

ReconstructionAssessmentStats NodeScene::reconstructionAssessmentStats() const {
    return reconstructionAssessmentStats(m_reconstructionThresholds);
}

ReconstructionAssessmentStats NodeScene::reconstructionAssessmentStats(
    const ReconstructionQualityThresholds& thresholds) const {
    const std::vector<ReconstructionAssessmentEntry> rows = reconstructionAssessments(thresholds);
    ReconstructionAssessmentStats stats;
    stats.total = rows.size();

    for (const ReconstructionAssessmentEntry& row : rows) {
        if (row.snapshot.state == ReconstructionQualityState::Pass) {
            ++stats.pass;
            continue;
        }
        if (row.snapshot.state == ReconstructionQualityState::Fail) {
            ++stats.fail;
            continue;
        }
        ++stats.unavailable;
    }

    return stats;
}

ReconstructionAssessmentStatsSnapshot NodeScene::reconstructionAssessmentStatsSnapshot() const {
    return reconstructionAssessmentStatsSnapshot(m_reconstructionThresholds);
}

ReconstructionAssessmentStatsSnapshot NodeScene::reconstructionAssessmentStatsSnapshot(
    const ReconstructionQualityThresholds& thresholds) const {
    ReconstructionAssessmentStatsSnapshot snapshot;
    snapshot.stats = reconstructionAssessmentStats(thresholds);
    snapshot.passRate = snapshot.stats.total == 0u
        ? 0.0f
        : static_cast<float>(snapshot.stats.pass) / static_cast<float>(snapshot.stats.total);
    snapshot.thresholds = thresholds;
    return snapshot;
}

std::string NodeScene::reconstructionAssessmentStatsSummary() const {
    return reconstructionAssessmentStatsSummary(m_reconstructionThresholds);
}

std::string NodeScene::reconstructionAssessmentStatsSummary(
    const ReconstructionQualityThresholds& thresholds) const {
    const ReconstructionAssessmentStatsSnapshot summary = reconstructionAssessmentStatsSnapshot(thresholds);

    std::ostringstream oss;
    oss.imbue(std::locale::classic());
    oss << std::fixed << std::setprecision(3);
    oss << "reconstruction_stats"
        << " total=" << summary.stats.total
        << " pass=" << summary.stats.pass
        << " fail=" << summary.stats.fail
        << " unavailable=" << summary.stats.unavailable
        << " pass_rate=" << summary.passRate
        << " residual_threshold=" << summary.thresholds.maxResidual
        << " confidence_threshold=" << summary.thresholds.minConfidence;
    return oss.str();
}

ReconstructionQualityState NodeScene::reconstructionQualityState(SceneNodeId id) const noexcept {
    return reconstructionQualityState(id, m_reconstructionThresholds);
}

ReconstructionQualityState NodeScene::reconstructionQualityState(
    SceneNodeId id,
    float maxResidual,
    float minConfidence) const noexcept {
    if (!hasNode(id)) {
        return ReconstructionQualityState::UnavailableUnknownNode;
    }
    const NodePayload::ReconstructionDiagnostic* d = reconstructionDiagnostic(id);
    if (!d) {
        return ReconstructionQualityState::UnavailableMissingDiagnostic;
    }
    return d->passesAlpha(maxResidual, minConfidence)
        ? ReconstructionQualityState::Pass
        : ReconstructionQualityState::Fail;
}

ReconstructionQualityState NodeScene::reconstructionQualityState(
    SceneNodeId id,
    const ReconstructionQualityThresholds& thresholds) const noexcept {
    return reconstructionQualityState(id, thresholds.maxResidual, thresholds.minConfidence);
}

// ── Cache invalidation ────────────────────────────────────────────────────────

void NodeScene::markDirty(SceneNodeId id) {
    m_graph.markDirty(id);
}

bool NodeScene::isDirty(SceneNodeId id) const noexcept {
    return m_graph.isDirty(id);
}

// ── Evaluation ────────────────────────────────────────────────────────────────

EvalReport NodeScene::evaluate() {
    return m_graph.evaluate();
}

void NodeScene::setComputeCallback(EvalGraph::NodeComputeFn callback) {
    m_graph.setComputeCallback(std::move(callback));
}

void NodeScene::setComputeCallback(EvalGraph::LegacyNodeComputeFn callback) {
    m_graph.setComputeCallback(std::move(callback));
}

// ── Hierarchy ─────────────────────────────────────────────────────────────────

bool NodeScene::setParent(SceneNodeId child, SceneNodeId parent) {
    if (!m_graph.hasNode(child) || !m_graph.hasNode(parent)) return false;
    if (child == parent) return false;

    // Cycle check: walk up from parent; if we reach child the operation is cyclic.
    SceneNodeId cursor = parent;
    while (cursor != kInvalidSceneNodeId) {
        auto it = m_parentOf.find(cursor);
        if (it == m_parentOf.end()) break;
        if (it->second == child) return false;
        cursor = it->second;
    }

    // Detach from existing parent, if any.
    auto existingIt = m_parentOf.find(child);
    if (existingIt != m_parentOf.end()) {
        SceneNodeId oldParent = existingIt->second;
        auto& siblings = m_childrenOf[oldParent];
        siblings.erase(std::remove(siblings.begin(), siblings.end(), child), siblings.end());
        (void)m_graph.disconnect(oldParent, child);
        m_parentOf.erase(existingIt);
    }

    m_parentOf[child] = parent;
    m_childrenOf[parent].push_back(child);
    (void)m_graph.connect(parent, child);
    return true;
}

bool NodeScene::clearParent(SceneNodeId child) noexcept {
    auto it = m_parentOf.find(child);
    if (it == m_parentOf.end()) return false;
    SceneNodeId oldParent = it->second;
    auto& siblings = m_childrenOf[oldParent];
    siblings.erase(std::remove(siblings.begin(), siblings.end(), child), siblings.end());
    (void)m_graph.disconnect(oldParent, child);
    m_parentOf.erase(it);
    return true;
}

SceneNodeId NodeScene::parent(SceneNodeId child) const noexcept {
    auto it = m_parentOf.find(child);
    return it != m_parentOf.end() ? it->second : kInvalidSceneNodeId;
}

std::vector<SceneNodeId> NodeScene::children(SceneNodeId id) const {
    auto it = m_childrenOf.find(id);
    return it != m_childrenOf.end() ? it->second : std::vector<SceneNodeId>{};
}

SceneNodeId NodeScene::nodeByPath(std::string_view path) const {
    if (path.empty()) return kInvalidSceneNodeId;

    // Split on '/'.
    std::vector<std::string> segments;
    std::size_t start = 0;
    while (start <= path.size()) {
        std::size_t slash = path.find('/', start);
        if (slash == std::string_view::npos) {
            segments.emplace_back(path.substr(start));
            break;
        }
        segments.emplace_back(path.substr(start, slash - start));
        start = slash + 1;
    }
    if (segments.empty()) return kInvalidSceneNodeId;

    // First segment must resolve to a root node (no parent).
    SceneNodeId current = nodeByName(segments[0]);
    if (current == kInvalidSceneNodeId) return kInvalidSceneNodeId;
    if (m_parentOf.count(current)) return kInvalidSceneNodeId; // not a root

    for (std::size_t i = 1; i < segments.size(); ++i) {
        SceneNodeId next = nodeByName(segments[i]);
        if (next == kInvalidSceneNodeId) return kInvalidSceneNodeId;
        auto pit = m_parentOf.find(next);
        if (pit == m_parentOf.end() || pit->second != current) return kInvalidSceneNodeId;
        current = next;
    }
    return current;
}

std::string NodeScene::nodePath(SceneNodeId id) const {
    if (!m_graph.hasNode(id)) return "";

    std::vector<std::string> parts;
    SceneNodeId cursor = id;
    while (cursor != kInvalidSceneNodeId) {
        auto nameIt = m_idToName.find(cursor);
        if (nameIt == m_idToName.end()) return "";
        parts.push_back(nameIt->second);
        auto parentIt = m_parentOf.find(cursor);
        cursor = (parentIt != m_parentOf.end()) ? parentIt->second : kInvalidSceneNodeId;
    }

    std::reverse(parts.begin(), parts.end());
    std::string result;
    result.reserve(64);
    for (std::size_t i = 0; i < parts.size(); ++i) {
        if (i > 0) result += '/';
        result += parts[i];
    }
    return result;
}

// ── Lifetime ──────────────────────────────────────────────────────────────────

void NodeScene::clear() noexcept {
    m_graph.clear();
    m_nameToId.clear();
    m_idToName.clear();
    m_parentOf.clear();
    m_childrenOf.clear();
}

// ── Internal graph access ─────────────────────────────────────────────────────

const EvalGraph& NodeScene::evalGraph() const noexcept {
    return m_graph;
}

} // namespace nexus
