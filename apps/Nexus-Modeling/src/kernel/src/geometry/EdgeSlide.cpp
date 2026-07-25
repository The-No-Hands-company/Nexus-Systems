#include <nexus/geometry/EdgeSlide.h>
#include <algorithm>

namespace nexus::geometry {

using Vec3 = nexus::render::Vec3;

void EdgeSlide::slideVertices(HalfEdgeMesh& mesh, 
                              const std::vector<uint32_t>& vertexIndices, 
                              const Vec3& delta) {
    auto& positions = mesh.positions();
    
    for (uint32_t vIdx : vertexIndices) {
        if (vIdx >= positions.size()) continue;

        // Constrain the requested delta to the vertex's incident edge directions: move by
        // the average of delta projected onto each outgoing edge. A delta perpendicular to
        // every incident edge (e.g. along the surface normal) contributes nothing, so the
        // vertex slides within its edge fan rather than leaving the surface.
        //
        // The previous version walked mesh.edge(he).next — the *face* loop, not the vertex
        // one-ring — so after the first edge it measured directions of edges not incident to
        // the vertex at all, and it summed (never averaged) the projections. Scanning the
        // outgoing half-edges directly (src == vIdx) is boundary-safe and visits exactly the
        // incident edges regardless of fan structure.
        Vec3 avgDir{0, 0, 0};
        uint32_t edgeCount = 0;
        for (uint32_t e = 0; e < mesh.edgeCount(); ++e) {
            if (!mesh.isLiveEdge(e)) continue;
            if (mesh.edge(e).src != vIdx) continue;
            uint32_t nxt = mesh.edge(e).next;
            if (nxt == HalfEdgeMesh::kInvalid) continue;
            uint32_t dst = mesh.edge(nxt).src;  // head of this half-edge
            if (dst >= positions.size() || dst == vIdx) continue;

            Vec3 edgeDir = (positions[dst] - positions[vIdx]).normalize();
            float projection = delta.dot(edgeDir);
            avgDir = avgDir + edgeDir * projection;
            ++edgeCount;
        }

        if (edgeCount > 0) {
            positions[vIdx] = positions[vIdx] + avgDir * (1.f / static_cast<float>(edgeCount));
        }
    }
}

} // namespace nexus::geometry

