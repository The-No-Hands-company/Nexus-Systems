#pragma once

#include <nexus/utility/math/vector3_utils.h>
#include <vector>
#include <cstdint>

namespace nexus::utility::graphics {

/**
 * @brief Mesh builder for procedural geometry.
 */
class MeshBuilder {
public:
    using Vector3 = nexus::utility::math::Vector3;
    using Vector3Utils = nexus::utility::math::Vector3Utils;
    struct Vertex {
        Vector3 position;
        Vector3 normal;
        float u, v; // Texture coordinates
        
        Vertex() : position(), normal(), u(0), v(0) {}
        Vertex(const Vector3& pos, const Vector3& norm = Vector3::Up(), float u = 0, float v = 0)
            : position(pos), normal(norm), u(u), v(v) {}
    };

    struct Mesh {
        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;
        
        void clear() {
            vertices.clear();
            indices.clear();
        }
    };

    /**
     * @brief Adds a vertex.
     */
    void addVertex(const Vertex& vertex) {
        mesh_.vertices.push_back(vertex);
    }

    /**
     * @brief Adds a triangle (3 indices).
     */
    void addTriangle(uint32_t i0, uint32_t i1, uint32_t i2) {
        mesh_.indices.push_back(i0);
        mesh_.indices.push_back(i1);
        mesh_.indices.push_back(i2);
    }

    /**
     * @brief Adds a quad (4 vertices as 2 triangles).
     */
    void addQuad(uint32_t i0, uint32_t i1, uint32_t i2, uint32_t i3) {
        addTriangle(i0, i1, i2);
        addTriangle(i0, i2, i3);
    }

    /**
     * @brief Calculates normals (flat shading).
     */
    void calculateNormals() {
        for (size_t i = 0; i < mesh_.indices.size(); i += 3) {
            auto& v0 = mesh_.vertices[mesh_.indices[i]];
            auto& v1 = mesh_.vertices[mesh_.indices[i + 1]];
            auto& v2 = mesh_.vertices[mesh_.indices[i + 2]];
            
            Vector3 edge1 = v1.position - v0.position;
            Vector3 edge2 = v2.position - v0.position;
            Vector3 normal = Vector3Utils::cross(edge1, edge2).normalized();
            
            v0.normal = normal;
            v1.normal = normal;
            v2.normal = normal;
        }
    }

    /**
     * @brief Gets the built mesh.
     */
    const Mesh& getMesh() const { return mesh_; }
    Mesh& getMesh() { return mesh_; }

    /**
     * @brief Clears the mesh.
     */
    void clear() { mesh_.clear(); }

private:
    Mesh mesh_;
};

} // namespace nexus::utility::graphics
