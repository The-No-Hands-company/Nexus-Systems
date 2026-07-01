#pragma once
// ─── Nexus Geometry ── EvaluationGraph ─────────────────────────────────────
//  Directed Acyclic Graph (DAG) modifier chain.
//  Base mesh passes through modifier nodes; output is a viewport render mesh.
//  Dirty propagation cascades downstream on parameter change.

#include <nexus/geometry/Mesh.h>
#include <nexus/geometry/HalfEdgeMesh.h>

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace nexus::geometry {

struct EvalNodeId {
    uint32_t value = 0;
    bool operator==(const EvalNodeId& o) const noexcept { return value == o.value; }
    bool operator!=(const EvalNodeId& o) const noexcept { return value != o.value; }
};
static constexpr EvalNodeId kInvalidEvalNodeId{0xFFFFFFFF};

enum class EvalNodeKind : uint8_t {
    BaseMesh,
    Extrude,
    Bevel,
    Subdivide,
    Decimate,
    Boolean,
    Mirror,
    Array,
    Deform,
    Custom,
};

struct EvalNode {
    EvalNodeId id;
    EvalNodeId parent = kInvalidEvalNodeId;
    EvalNodeKind kind = EvalNodeKind::BaseMesh;
    std::string name;
    bool enabled = true;
    bool dirty = true;

    struct Parameters {
        float extrudeDistance = 0.0f;
        float bevelDistance = 0.0f;
        uint32_t subdivIterations = 0;
        float decimateRatio = 1.0f;
    } params;

    Mesh cachedOutput;
};

class EvaluationGraph {
public:
    void setBaseMesh(const Mesh& mesh);

    [[nodiscard]] EvalNodeId addNode(EvalNodeKind kind,
                                      EvalNodeId parent = kInvalidEvalNodeId,
                                      const std::string& name = "");

    bool removeNode(EvalNodeId id);
    bool setParent(EvalNodeId child, EvalNodeId newParent);

    void setParam(EvalNodeId id, float extrudeDistance);
    void setParamBevel(EvalNodeId id, float bevelDistance);
    void setParamSubdiv(EvalNodeId id, uint32_t iterations);
    void setParamDecimate(EvalNodeId id, float ratio);

    void setEnabled(EvalNodeId id, bool enabled);

    [[nodiscard]] const Mesh& evaluate();

    [[nodiscard]] const EvalNode* node(EvalNodeId id) const;
    [[nodiscard]] const std::vector<EvalNode>& nodes() const noexcept;

    void markDirty(EvalNodeId id);
    void invalidateDownstream(EvalNodeId id);

    [[nodiscard]] bool hasCycles() const;

private:
    std::vector<EvalNode> m_nodes;
    Mesh m_baseMesh;
    bool m_baseMeshDirty = true;

    [[nodiscard]] EvalNode* findNode(EvalNodeId id);
    void evaluateNode(EvalNode& node);
    bool wouldCreateCycle(EvalNodeId child, EvalNodeId parent) const;
};

} // namespace nexus::geometry
