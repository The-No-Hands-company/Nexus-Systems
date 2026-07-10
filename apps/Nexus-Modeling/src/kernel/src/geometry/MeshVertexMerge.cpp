#include <nexus/geometry/MeshVertexMerge.h>
#include <unordered_map>
#include <cmath>

namespace nexus::geometry {

using Vec3 = nexus::render::Vec3;

MergeReport MeshVertexMerge::mergeToVertex(HalfEdgeMesh& mesh, uint32_t sourceIdx, uint32_t targetIdx) {
    if (sourceIdx == targetIdx) return {false, 0, 0};
    if (sourceIdx >= mesh.vertexCount() || targetIdx >= mesh.vertexCount()) 
        return {false, 0, 0};

    MergeReport report;
    uint32_t collapsedEdges = 0;

    uint32_t startHe = mesh.vertex(sourceIdx).edge;
    if (startHe == HalfEdgeMesh::kInvalid) return {false, 0, 0};

    uint32_t currHe = startHe;
    do {
        uint32_t twin = mesh.edge(currHe).twin;
        if (twin != HalfEdgeMesh::kInvalid) {
            mesh.edge(twin).src = targetIdx;
        }
        currHe = mesh.edge(currHe).next;
    } while (currHe != startHe && currHe != HalfEdgeMesh::kInvalid);
    
    report.success = true;
    report.verticesRemoved = 1;
    report.edgesCollapsed = collapsedEdges;
    return report;
}

MergeReport MeshVertexMerge::mergeByDistance(HalfEdgeMesh& mesh, float tolerance) {
    auto& positions = mesh.positions();
    MergeReport report;
    uint32_t removed = 0;

    for (size_t i = 0; i < positions.size(); ++i) {
        for (size_t j = i + 1; j < positions.size(); ++j) {
            if ((positions[i] - positions[j]).length() < tolerance) {
                mergeToVertex(mesh, (uint32_t)j, (uint32_t)i);
                removed++;
            }
        }
    }

    report.success = (removed > 0);
    report.verticesRemoved = removed;
    return report;
}

} // namespace nexus::geometry
