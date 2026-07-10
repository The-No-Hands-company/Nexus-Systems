#pragma once

#include <nexus/geometry/Mesh.h>
#include <nexus/geometry/HalfEdgeMesh.h>
#include <vector>
#include <cstdint>

namespace nexus::app {

class SelectionUtils {
public:
    /**
     * Selects a ring of edges. An edge ring is a set of edges that are 
     * opposite to the current selection across a face.
     */
    static std::vector<uint32_t> selectEdgeRing(nexus::geometry::HalfEdgeMesh& mesh, 
                                             const std::vector<uint32_t>& seedEdges);

    /**
     * Selects an edge loop. An edge loop is a set of edges that share 
     * a common boundary or are connected in a continuous circuit.
     */
    static std::vector<uint32_t> selectEdgeLoop(nexus::geometry::HalfEdgeMesh& mesh, 
                                              const std::vector<uint32_t>& seedEdges);

    /**
     * Selects all faces/edges/vertices that share similar properties 
     * (e.g., similar area, length, or normal direction).
     */
    static std::vector<uint32_t> selectSimilarFaces(nexus::geometry::HalfEdgeMesh& mesh, 
                                                  uint32_t seedFace, 
                                                  float tolerance = 0.1f);
};

} // namespace nexus::app
