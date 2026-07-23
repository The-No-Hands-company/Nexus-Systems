#include <nexus/geometry/MeshTopologyValidation.h>

#include <algorithm>
#include <map>
#include <utility>
#include <cmath>
#include <unordered_map>
#include <unordered_set>

namespace nexus::geometry {
using Vec3 = nexus::render::Vec3;

TopologyValidityResult MeshTopologyValidation::validate(const Mesh& mesh) {
    TopologyValidityResult r;
    if (!mesh.isValid()) {
        r.valid = false;
        r.violations.push_back({TopologyViolation::DegenerateFace, "Mesh is not valid", 0});
        return r;
    }

    auto hemOpt = HalfEdgeMesh::fromMesh(mesh);
    if (!hemOpt) {
        r.valid = false;
        r.violations.push_back({TopologyViolation::NonManifoldVertex, "Failed to build half-edge structure", 0});
        return r;
    }
    r = validateHEM(*hemOpt);

    // Non-manifold edges have to be found on the FACE LIST, not through the half-edge
    // structure. A half-edge mesh cannot represent three faces meeting at one edge — the
    // third face's half-edge simply never gets a twin — so by the time the structure
    // exists the evidence is gone: the extra face reads as a boundary, no edge ever shows
    // a valence above two, and the check that looks for one can never fire. Three
    // triangles sharing an edge, the textbook non-manifold case, was reported VALID with
    // no violations at all.
    //
    // Counting how many faces use each undirected edge is exact and needs no structure.
    std::map<std::pair<uint32_t, uint32_t>, uint32_t> edgeUse;
    const auto& topo = mesh.topology();
    for (size_t fi = 0; fi < topo.faceCount(); ++fi) {
        const Face& face = topo.face(fi);
        if (face.indices.size() < 3) continue;
        for (size_t k = 0; k < face.indices.size(); ++k) {
            uint32_t a = face.indices[k];
            uint32_t b = face.indices[(k + 1) % face.indices.size()];
            if (a > b) std::swap(a, b);
            edgeUse[{a, b}]++;
        }
    }
    uint32_t edgeIndex = 0;
    for (const auto& [edge, uses] : edgeUse) {
        if (uses > 2) {
            r.valid = false;
            r.violations.push_back({TopologyViolation::NonManifoldEdge,
                                    "Edge is used by " + std::to_string(uses) + " faces",
                                    edgeIndex});
        }
        ++edgeIndex;
    }
    return r;
}

TopologyValidityResult MeshTopologyValidation::validateHEM(const HalfEdgeMesh& hem) {
    TopologyValidityResult r;
    uint32_t V = hem.vertexCount();
    uint32_t F = hem.faceCount();

    // Count each undirected edge once. Dividing the half-edge count by two assumes every
    // edge carries two half-edges, which is true only on a CLOSED surface — a boundary
    // edge has one. On an open mesh that undercounts E by half the boundary, and the Euler
    // characteristic comes out too high by exactly that much: a 4x4 plane, whose sixteen
    // boundary edges make it a disk with chi = 1, reported 9.
    //
    // Nothing caught it because every test here used a closed cube, or called the static
    // arithmetic helpers with numbers written down by hand.
    uint32_t E = 0;
    for (uint32_t ei = 0; ei < hem.edgeCount(); ++ei) {
        const uint32_t twin = hem.edge(ei).twin;
        if (twin == HalfEdgeMesh::kInvalid || ei < twin) ++E;  // boundary, or the lower of a pair
    }

    // Euler characteristic: V - E + F = 2(1 - G) + B for orientable surfaces
    r.euler = computeEulerCharacteristic(V, E, F);
    r.boundaryLoops = static_cast<uint32_t>(hem.boundaryLoops().size());
    r.genus = computeGenus(r.euler, r.boundaryLoops);

    // Check for non-manifold edges (valence > 2)
    for (uint32_t ei = 0; ei < hem.edgeCount(); ++ei) {
        uint32_t valence = hem.edgeValence(ei);
        if (valence > 2) {
            r.valid = false;
            r.violations.push_back({TopologyViolation::NonManifoldEdge,
                "Edge has " + std::to_string(valence) + " incident faces", ei});
        }
    }

    // Check for degenerate faces (valence < 3)
    for (uint32_t fi = 0; fi < hem.faceCount(); ++fi) {
        uint32_t v = hem.faceValence(fi);
        if (v == 0) continue; // dead face
        if (v < 3) {
            r.valid = false;
            r.violations.push_back({TopologyViolation::DegenerateFace,
                "Face has " + std::to_string(v) + " edges", fi});
        }
    }

    // Check for zero-length edges
    for (uint32_t ei = 0; ei < hem.edgeCount(); ++ei) {
        const auto& e = hem.edge(ei);
        if (e.face == HalfEdgeMesh::kInvalid) continue;
        uint32_t t = e.twin;
        if (t == HalfEdgeMesh::kInvalid || t >= hem.edgeCount()) continue;
        uint32_t src = e.src;
        uint32_t dst = hem.edge(t).src;
        if (src >= hem.vertexCount() || dst >= hem.vertexCount()) continue;

        const auto& p0 = hem.positions()[src];
        const auto& p1 = hem.positions()[dst];
        float dx = p1.x - p0.x;
        float dy = p1.y - p0.y;
        float dz = p1.z - p0.z;
        float len2 = dx * dx + dy * dy + dz * dz;
        if (len2 < 1e-12f) {
            r.valid = false;
            r.violations.push_back({TopologyViolation::ZeroLengthEdge,
                "Edge length is near zero", ei});
        }
    }

    // Check for duplicate vertices
    {
        std::unordered_map<uint64_t, uint32_t> posHash;
        const float eps = 1e-6f;
        for (uint32_t vi = 0; vi < hem.vertexCount(); ++vi) {
            const auto& p = hem.positions()[vi];
            uint64_t hx = static_cast<uint64_t>(p.x / eps);
            uint64_t hy = static_cast<uint64_t>(p.y / eps);
            uint64_t hz = static_cast<uint64_t>(p.z / eps);
            uint64_t key = (hx << 42) ^ (hy << 21) ^ hz;
            auto it = posHash.find(key);
            if (it != posHash.end()) {
                float d2 = (p.x - hem.positions()[it->second].x) * (p.x - hem.positions()[it->second].x) +
                           (p.y - hem.positions()[it->second].y) * (p.y - hem.positions()[it->second].y) +
                           (p.z - hem.positions()[it->second].z) * (p.z - hem.positions()[it->second].z);
                if (d2 < eps * eps) {
                    r.valid = false;
                    r.violations.push_back({TopologyViolation::DuplicateVertices,
                        "Duplicate vertex position", vi});
                }
            } else {
                posHash[key] = vi;
            }
        }
    }

    return r;
}

bool MeshTopologyValidation::wouldCreateNonManifoldEdge(
    const HalfEdgeMesh& hem, uint32_t src, uint32_t dst) {
    if (src >= hem.vertexCount() || dst >= hem.vertexCount()) return false;

    uint32_t existing = hem.findEdge(src, dst);
    if (existing != HalfEdgeMesh::kInvalid) {
        // Edge already exists — adding another would create non-manifold
        return hem.edgeValence(existing) >= 2;
    }

    uint32_t reverse = hem.findEdge(dst, src);
    if (reverse != HalfEdgeMesh::kInvalid) {
        return hem.edgeValence(reverse) >= 2;
    }

    return false;
}

bool MeshTopologyValidation::wouldCreateNonManifoldVertex(
    const HalfEdgeMesh& hem, uint32_t vertex) {
    if (vertex >= hem.vertexCount()) return false;

    auto neighbors = hem.vertexNeighbors(vertex);
    std::unordered_set<uint32_t> uniqueNeighbors(neighbors.begin(), neighbors.end());

    // A vertex is topologically valid if its one-ring is a single closed cycle
    // Non-manifold if the one-ring forms multiple disconnected cycles
    if (neighbors.empty()) return false;

    // Simplified check: if neighbor list has duplicates, vertex may be non-manifold
    return neighbors.size() != uniqueNeighbors.size();
}

int MeshTopologyValidation::computeEulerCharacteristic(
    uint32_t V, uint32_t E, uint32_t F) noexcept {
    return static_cast<int>(V) - static_cast<int>(E) + static_cast<int>(F);
}

int MeshTopologyValidation::computeGenus(
    int eulerCharacteristic, uint32_t boundaryLoops) noexcept {
    // chi = 2 - 2g - b, so g = (2 - b - chi) / 2.
    //
    // boundaryLoops is unsigned, and `2 - boundaryLoops - eulerCharacteristic` promoted the
    // whole expression to unsigned: with one boundary loop and chi = 3 it evaluated
    // 1u - 3 = 4294967294, halved to 2147483647, and every open mesh reported that as its
    // genus. Convert first, then do signed arithmetic.
    const int b = static_cast<int>(boundaryLoops);
    const int g = (2 - b - eulerCharacteristic) / 2;
    return g >= 0 ? g : 0;
}

} // namespace nexus::geometry
