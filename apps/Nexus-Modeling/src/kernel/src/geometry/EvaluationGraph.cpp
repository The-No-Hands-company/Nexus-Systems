#include <nexus/geometry/EvaluationGraph.h>

#include <nexus/geometry/BevelChamfer.h>
#include <nexus/geometry/ExtrudeOperation.h>
#include <nexus/geometry/MeshDecimator.h>
#include <nexus/geometry/SubdivisionSurface.h>

#include <algorithm>
#include <unordered_set>

namespace nexus::geometry {

void EvaluationGraph::setBaseMesh(const Mesh& mesh) {
    m_baseMesh = mesh;
    m_baseMeshDirty = true;
    for (auto& n : m_nodes) n.dirty = true;
}

EvalNodeId EvaluationGraph::addNode(EvalNodeKind kind, EvalNodeId parent, const std::string& name) {
    EvalNode node;
    node.id = EvalNodeId{static_cast<uint32_t>(m_nodes.size() + 1)};
    node.parent = parent;
    node.kind = kind;
    node.name = name;
    m_nodes.push_back(node);
    return node.id;
}

bool EvaluationGraph::removeNode(EvalNodeId id) {
    auto* n = findNode(id);
    if (!n) return false;

    for (auto& other : m_nodes) {
        if (other.parent == id) {
            other.parent = kInvalidEvalNodeId;
            other.dirty = true;
        }
    }

    n->dirty = true;
    n->enabled = false;
    return true;
}

bool EvaluationGraph::setParent(EvalNodeId child, EvalNodeId newParent) {
    if (wouldCreateCycle(child, newParent)) return false;
    auto* n = findNode(child);
    if (!n) return false;
    n->parent = newParent;
    n->dirty = true;
    return true;
}

void EvaluationGraph::setParam(EvalNodeId id, float extrudeDistance) {
    auto* n = findNode(id);
    if (!n) return;
    n->params.extrudeDistance = extrudeDistance;
    markDirty(id);
}

void EvaluationGraph::setParamBevel(EvalNodeId id, float bevelDistance) {
    auto* n = findNode(id);
    if (!n) return;
    n->params.bevelDistance = bevelDistance;
    markDirty(id);
}

void EvaluationGraph::setParamSubdiv(EvalNodeId id, uint32_t iterations) {
    auto* n = findNode(id);
    if (!n) return;
    n->params.subdivIterations = iterations;
    markDirty(id);
}

void EvaluationGraph::setParamDecimate(EvalNodeId id, float ratio) {
    auto* n = findNode(id);
    if (!n) return;
    n->params.decimateRatio = ratio;
    markDirty(id);
}

void EvaluationGraph::setEnabled(EvalNodeId id, bool enabled) {
    auto* n = findNode(id);
    if (!n) return;
    n->enabled = enabled;
    n->dirty = true;
}

const Mesh& EvaluationGraph::evaluate() {
    if (m_nodes.empty()) return m_baseMesh;

    for (auto& node : m_nodes) {
        if (node.parent == kInvalidEvalNodeId && m_baseMeshDirty) {
            node.dirty = true;
        }
    }
    m_baseMeshDirty = false;

    // Topological sort: evaluate parents before children
    std::vector<EvalNode*> order;
    std::unordered_set<uint32_t> evaluated;
    bool changed = true;
    while (changed) {
        changed = false;
        for (auto& node : m_nodes) {
            if (evaluated.count(node.id.value)) continue;
            if (node.parent == kInvalidEvalNodeId) {
                order.push_back(&node);
                evaluated.insert(node.id.value);
                changed = true;
            } else if (evaluated.count(node.parent.value)) {
                order.push_back(&node);
                evaluated.insert(node.id.value);
                changed = true;
            }
        }
    }

    for (auto* node : order) {
        if (!node->enabled) {
            if (node->parent == kInvalidEvalNodeId) {
                node->cachedOutput = m_baseMesh;
            }
            continue;
        }
        if (!node->dirty) continue;
        evaluateNode(*node);
    }

    if (order.empty()) return m_baseMesh;

    // Walk DAG to find leaf node with valid output
    for (auto it = order.rbegin(); it != order.rend(); ++it) {
        auto* n = *it;
        if (n->enabled && n->cachedOutput.isValid()) {
            return n->cachedOutput;
        }
    }
    return m_baseMesh;
}

const EvalNode* EvaluationGraph::node(EvalNodeId id) const {
    return const_cast<EvaluationGraph*>(this)->findNode(id);
}

const std::vector<EvalNode>& EvaluationGraph::nodes() const noexcept {
    return m_nodes;
}

void EvaluationGraph::markDirty(EvalNodeId id) {
    auto* n = findNode(id);
    if (!n) return;
    n->dirty = true;
    invalidateDownstream(id);
}

void EvaluationGraph::invalidateDownstream(EvalNodeId id) {
    for (auto& node : m_nodes) {
        if (node.parent == id) {
            node.dirty = true;
            invalidateDownstream(node.id);
        }
    }
}

bool EvaluationGraph::hasCycles() const {
    for (const auto& node : m_nodes) {
        if (wouldCreateCycle(node.id, node.parent)) return true;
    }
    return false;
}

EvalNode* EvaluationGraph::findNode(EvalNodeId id) {
    for (auto& n : m_nodes) {
        if (n.id == id) return &n;
    }
    return nullptr;
}

void EvaluationGraph::evaluateNode(EvalNode& node) {
    const Mesh* input = &m_baseMesh;
    if (node.parent != kInvalidEvalNodeId) {
        auto* parentNode = findNode(node.parent);
        if (parentNode && parentNode->cachedOutput.isValid()) {
            input = &parentNode->cachedOutput;
        }
    }

    if (!input->isValid()) {
        node.cachedOutput = *input;
        node.dirty = false;
        return;
    }

    switch (node.kind) {
        case EvalNodeKind::BaseMesh:
            node.cachedOutput = *input;
            break;
        case EvalNodeKind::Extrude: {
            ExtrudeDesc desc;
            desc.distance = node.params.extrudeDistance;
            (void)ExtrudeOperation::applyToAllFaces(*input, desc, node.cachedOutput);
            break;
        }
        case EvalNodeKind::Bevel: {
            BevelChamferDesc desc;
            desc.distance = node.params.bevelDistance;
            desc.mode = BevelChamferMode::Bevel;
            (void)BevelChamferOperation::apply(*input, desc, node.cachedOutput);
            break;
        }
        case EvalNodeKind::Subdivide: {
            auto hem = HalfEdgeMesh::fromMesh(*input);
            if (hem) {
                SubdivisionOptions subdOpts;
                subdOpts.levels = node.params.subdivIterations > 0 ? node.params.subdivIterations : 1;
                auto subdivided = SubdivisionSurface::catmullClark(*hem, subdOpts);
                if (subdivided) {
                    node.cachedOutput = subdivided->toMesh();
                } else {
                    node.cachedOutput = *input;
                }
            } else {
                node.cachedOutput = *input;
            }
            break;
        }
        case EvalNodeKind::Decimate: {
            auto hem = HalfEdgeMesh::fromMesh(*input);
            if (hem) {
                DecimationOptions opts;
                opts.targetFaceCount = static_cast<uint32_t>(
                    static_cast<float>(input->topology().faceCount()) * node.params.decimateRatio);
                if (opts.targetFaceCount < 3) opts.targetFaceCount = 3;
                auto result = MeshDecimator::decimate(*hem, opts);
                if (result) {
                    node.cachedOutput = result->first.toMesh();
                } else {
                    node.cachedOutput = *input;
                }
            } else {
                node.cachedOutput = *input;
            }
            break;
        }
        default:
            node.cachedOutput = *input;
            break;
    }
    node.dirty = false;
}

bool EvaluationGraph::wouldCreateCycle(EvalNodeId child, EvalNodeId parent) const {
    if (child == parent) return true;

    std::unordered_set<uint32_t> visited;
    EvalNodeId current = parent;
    while (current != kInvalidEvalNodeId) {
        if (current == child) return true;
        if (visited.count(current.value)) return true;
        visited.insert(current.value);

        const EvalNode* n = nullptr;
        for (const auto& node : m_nodes) {
            if (node.id == current) { n = &node; break; }
        }
        if (!n) break;
        current = n->parent;
    }
    return false;
}

} // namespace nexus::geometry
