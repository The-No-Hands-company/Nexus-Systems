#pragma once

#include <nexus/geometry/HalfEdgeMesh.h>
#include <nexus/render/Camera.h>
#include <vector>
#include <cstdint>

namespace nexus::geometry {

class EdgeSlide {
public:
    /**
     * Slides selected vertices along their connected edges.
     * For each vertex, the movement is constrained to the average direction 
     * of its connected edges to preserve the overall topology.
     */
    static void slideVertices(HalfEdgeMesh& mesh, 
                             const std::vector<uint32_t>& vertexIndices, 
                             const nexus::render::Vec3& delta);
};

} // namespace nexus::geometry
