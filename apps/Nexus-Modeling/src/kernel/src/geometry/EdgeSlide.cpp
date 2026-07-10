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

        // Calculate average edge direction for this vertex
        Vec3 avgDir{0, 0, 0};
        uint32_t edgeCount = 0;

        // Iterate over edges connected to this vertex
        uint32_t startHe = mesh.vertex(vIdx).edge;
        if (startHe == HalfEdgeMesh::kInvalid) continue;

        uint32_t currHe = startHe;
        do {
            Vec3 vStart = positions[mesh.edge(currHe).src];
            Vec3 vEnd = positions[mesh.edge(mesh.edge(currHe).twin).src];
            Vec3 edgeDir = (vEnd - vStart).normalize();
            
            // We want the component of the delta that aligns with the edge
            float projection = delta.dot(edgeDir);
            avgDir = avgDir + edgeDir * projection;
            
            edgeCount++;
            currHe = mesh.edge(currHe).next;
        } while (currHe != startHe && currHe != HalfEdgeMesh::kInvalid);

        if (edgeCount > 0) {
            // Normalizing the combined projection to get the best slide direction
            positions[vIdx] = positions[vIdx] + avgDir;
        }
    }
}

} // namespace nexus::geometry

