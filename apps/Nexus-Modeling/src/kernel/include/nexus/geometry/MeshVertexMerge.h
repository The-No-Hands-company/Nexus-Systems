#pragma once

#include <nexus/geometry/HalfEdgeMesh.h>
#include <vector>
#include <cstdint>
#include <optional>

namespace nexus::geometry {

struct MergeReport {
    bool     success = false;
    uint32_t verticesRemoved = 0;
    uint32_t edgesCollapsed = 0;
};

class MeshVertexMerge {
public:
    /**
     * Merges a source vertex into a target vertex.
     * All edges connected to the source vertex are re-routed to the target.
     */
    static MergeReport mergeToVertex(HalfEdgeMesh& mesh, uint32_t sourceIdx, uint32_t targetIdx);

    /**
     * Merges all vertices within a certain distance of each other.
     * This is a "cleaning" operation to remove coincident vertices.
     */
    static MergeReport mergeByDistance(HalfEdgeMesh& mesh, float tolerance = 1e-5f);
};

} // namespace nexus::geometry
