#pragma once

#include <nexus/eval/EvalGraph.h>

#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace nexus {

/// Unique identifier for a node inside a NodeScene.
/// Same underlying type as EvalGraph NodeId; kept as an alias for readability.
using SceneNodeId = NodeId;
inline constexpr SceneNodeId kInvalidSceneNodeId = kInvalidNodeId;

/// Procedural evaluation scene built on top of EvalGraph.
///
/// NodeScene maps human-readable string names to EvalGraph node IDs,
/// provides a parent-agnostic dependency API (connect/disconnect), and
/// exposes a typed asset slot per node backed by EvalGraph payload slots.
///
/// All evaluation semantics (dirty propagation, topological ordering,
/// cycle detection, compute callbacks) are delegated to the internal EvalGraph.
///
/// Thread-safety: none — external locking required for concurrent access.
class NodeScene {
public:
    NodeScene();
    ~NodeScene();

    // ── Node management ─────────────────────────────────────────────────────

    /// Add a named node of the given kind.
    /// Returns kInvalidSceneNodeId when a node with the same name already exists.
    [[nodiscard]] SceneNodeId addNode(std::string name, NodeKind kind);

    /// Remove a node by id. Returns false for unknown ids.
    bool removeNode(SceneNodeId id) noexcept;

    /// Look up a node id by exact name. Returns kInvalidSceneNodeId when absent.
    [[nodiscard]] SceneNodeId nodeByName(const std::string& name) const noexcept;

    [[nodiscard]] bool        hasNode(SceneNodeId id)   const noexcept;
    [[nodiscard]] std::string nodeName(SceneNodeId id)  const;          ///< "" for unknown id.
    [[nodiscard]] NodeKind    nodeKind(SceneNodeId id)  const noexcept; ///< Constant for unknown id.
    [[nodiscard]] std::size_t nodeCount()               const noexcept;

    // ── Dependency edges ────────────────────────────────────────────────────

    /// Register srcNode → dstNode data-flow dependency.
    /// Returns false when either id is unknown, identical, or the edge exists.
    [[nodiscard]] bool connect(SceneNodeId srcNode, SceneNodeId dstNode);

    [[nodiscard]] bool disconnect(SceneNodeId srcNode, SceneNodeId dstNode) noexcept;
    [[nodiscard]] bool isConnected(SceneNodeId srcNode, SceneNodeId dstNode) const noexcept;

    // ── Hierarchy ───────────────────────────────────────────────────────────

    /// Establish a parent–child tree relationship and register the parent→child
    /// dependency edge in the underlying EvalGraph.
    ///
    /// If child already has a parent, the old relationship and its edge are cleared
    /// before the new one is established.
    ///
    /// Returns false when either id is unknown, when child == parent, or when the
    /// operation would introduce a cycle in the scene hierarchy.
    [[nodiscard]] bool setParent(SceneNodeId child, SceneNodeId parent);

    /// Remove the parent–child relationship and disconnect the corresponding
    /// dependency edge. Returns false when child has no parent or is unknown.
    bool clearParent(SceneNodeId child) noexcept;

    /// Return the direct parent of child, or kInvalidSceneNodeId for roots.
    [[nodiscard]] SceneNodeId parent(SceneNodeId child) const noexcept;

    /// Return the ordered direct children of the given node.
    /// Returns an empty vector for leaf nodes or unknown ids.
    [[nodiscard]] std::vector<SceneNodeId> children(SceneNodeId id) const;

    /// Resolve a slash-separated path to a scene node.
    ///
    /// Each segment must match the exact name of a node in the ancestor chain.
    /// The first segment must be a root node (no parent). Returns
    /// kInvalidSceneNodeId for empty paths, unknown segments, or ancestry
    /// mismatches.
    [[nodiscard]] SceneNodeId nodeByPath(std::string_view path) const;

    /// Return the slash-separated path from the root ancestor down to this node.
    /// Returns "" for unknown ids.
    [[nodiscard]] std::string nodePath(SceneNodeId id) const;

    // ── Asset slots ─────────────────────────────────────────────────────────

    /// Set the typed asset attached to a node's output slot.
    /// Returns false for unknown ids.
    [[nodiscard]] bool setAsset(SceneNodeId id, NodePayload payload);

    /// Reset asset to monostate (NodePayloadType::None). Returns false for unknown ids.
    [[nodiscard]] bool clearAsset(SceneNodeId id) noexcept;

    /// Read-only access to the asset slot. Returns nullptr for unknown ids.
    [[nodiscard]] const NodePayload* asset(SceneNodeId id) const noexcept;

    /// Set reconstruction quality metrics on a node's output slot.
    /// Returns false for unknown ids.
    [[nodiscard]] bool setReconstructionDiagnostic(
        SceneNodeId id,
        NodePayload::ReconstructionDiagnostic diagnostic);

    /// Read-only access to reconstruction quality metrics.
    /// Returns nullptr when id is unknown or payload type is not ReconstructionDiagnostic.
    [[nodiscard]] const NodePayload::ReconstructionDiagnostic* reconstructionDiagnostic(SceneNodeId id) const noexcept;

    // ── Cache invalidation ──────────────────────────────────────────────────

    void markDirty(SceneNodeId id);
    [[nodiscard]] bool isDirty(SceneNodeId id) const noexcept;

    // ── Evaluation ──────────────────────────────────────────────────────────

    /// Evaluate the scene in dependency-first topological order.
    /// Forwards to the internal EvalGraph::evaluate().
    [[nodiscard]] EvalReport evaluate();

    /// Register a compute callback. Forwarded to the internal EvalGraph.
    void setComputeCallback(EvalGraph::NodeComputeFn callback);
    void setComputeCallback(EvalGraph::LegacyNodeComputeFn callback);

    // ── Lifetime ────────────────────────────────────────────────────────────

    /// Remove all nodes, edges, assets, and name mappings.
    void clear() noexcept;

    // ── Internal graph access ───────────────────────────────────────────────

    /// Read-only access to the underlying EvalGraph for advanced queries.
    [[nodiscard]] const EvalGraph& evalGraph() const noexcept;

private:
    EvalGraph m_graph;
    std::unordered_map<std::string, SceneNodeId>              m_nameToId;
    std::unordered_map<SceneNodeId, std::string>              m_idToName;
    std::unordered_map<SceneNodeId, SceneNodeId>              m_parentOf;   // child → parent
    std::unordered_map<SceneNodeId, std::vector<SceneNodeId>> m_childrenOf; // parent → ordered children
};

} // namespace nexus
