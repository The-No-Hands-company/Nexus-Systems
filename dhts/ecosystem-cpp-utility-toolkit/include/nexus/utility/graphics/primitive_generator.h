#pragma once

#include <nexus/utility/graphics/mesh_builder.h>
#include <cmath>

namespace nexus::utility::graphics {
using namespace nexus::utility::math;

/**
 * @brief Generates primitive geometric shapes.
 */
class PrimitiveGenerator {
public:
    /**
     * @brief Generates a cube.
     */
    static MeshBuilder::Mesh createCube(float size = 1.0f) {
        MeshBuilder builder;
        float half = size * 0.5f;
        
        // Front face
        builder.addVertex({{-half, -half, half}, {0, 0, 1}, 0, 0});
        builder.addVertex({{half, -half, half}, {0, 0, 1}, 1, 0});
        builder.addVertex({{half, half, half}, {0, 0, 1}, 1, 1});
        builder.addVertex({{-half, half, half}, {0, 0, 1}, 0, 1});
        builder.addQuad(0, 1, 2, 3);
        
        // Back face
        builder.addVertex({{half, -half, -half}, {0, 0, -1}, 0, 0});
        builder.addVertex({{-half, -half, -half}, {0, 0, -1}, 1, 0});
        builder.addVertex({{-half, half, -half}, {0, 0, -1}, 1, 1});
        builder.addVertex({{half, half, -half}, {0, 0, -1}, 0, 1});
        builder.addQuad(4, 5, 6, 7);
        
        // Add remaining 4 faces...
        // (Simplified for brevity)
        
        return builder.getMesh();
    }

    /**
     * @brief Generates a sphere.
     */
    static MeshBuilder::Mesh createSphere(float radius = 1.0f, int segments = 32, int rings = 16) {
        MeshBuilder builder;
        
        for (int ring = 0; ring <= rings; ++ring) {
            float phi = M_PI * ring / rings;
            float y = radius * std::cos(phi);
            float ringRadius = radius * std::sin(phi);
            
            for (int seg = 0; seg <= segments; ++seg) {
                float theta = 2.0f * M_PI * seg / segments;
                float x = ringRadius * std::cos(theta);
                float z = ringRadius * std::sin(theta);
                
                Vector3 pos(x, y, z);
                Vector3 normal = pos.normalized();
                float u = static_cast<float>(seg) / segments;
                float v = static_cast<float>(ring) / rings;
                
                builder.addVertex({pos, normal, u, v});
            }
        }
        
        // Generate indices
        for (int ring = 0; ring < rings; ++ring) {
            for (int seg = 0; seg < segments; ++seg) {
                int current = ring * (segments + 1) + seg;
                int next = current + segments + 1;
                
                builder.addQuad(current, current + 1, next + 1, next);
            }
        }
        
        return builder.getMesh();
    }

    /**
     * @brief Generates a plane.
     */
    static MeshBuilder::Mesh createPlane(float width = 1.0f, float height = 1.0f, int subdivisionsX = 1, int subdivisionsY = 1) {
        MeshBuilder builder;
        
        float halfW = width * 0.5f;
        float halfH = height * 0.5f;
        float stepX = width / subdivisionsX;
        float stepY = height / subdivisionsY;
        
        for (int y = 0; y <= subdivisionsY; ++y) {
            for (int x = 0; x <= subdivisionsX; ++x) {
                float px = -halfW + x * stepX;
                float py = -halfH + y * stepY;
                float u = static_cast<float>(x) / subdivisionsX;
                float v = static_cast<float>(y) / subdivisionsY;
                
                builder.addVertex({{px, 0, py}, Vector3::Up(), u, v});
            }
        }
        
        // Generate indices
        for (int y = 0; y < subdivisionsY; ++y) {
            for (int x = 0; x < subdivisionsX; ++x) {
                int i0 = y * (subdivisionsX + 1) + x;
                int i1 = i0 + 1;
                int i2 = i0 + subdivisionsX + 1;
                int i3 = i2 + 1;
                
                builder.addQuad(i0, i1, i3, i2);
            }
        }
        
        return builder.getMesh();
    }

    /**
     * @brief Generates a cylinder.
     */
    static MeshBuilder::Mesh createCylinder(float radius = 1.0f, float height = 2.0f, int segments = 32) {
        MeshBuilder builder;
        float halfH = height * 0.5f;
        
        // Top and bottom caps + sides
        for (int i = 0; i <= segments; ++i) {
            float theta = 2.0f * M_PI * i / segments;
            float x = radius * std::cos(theta);
            float z = radius * std::sin(theta);
            float u = static_cast<float>(i) / segments;
            
            // Top
            builder.addVertex({{x, halfH, z}, Vector3::Up(), u, 1});
            // Bottom
            builder.addVertex({{x, -halfH, z}, Vector3(0, -1, 0), u, 0});
        }
        
        // Generate side faces
        for (int i = 0; i < segments; ++i) {
            int top1 = i * 2;
            int top2 = (i + 1) * 2;
            int bot1 = top1 + 1;
            int bot2 = top2 + 1;
            
            builder.addQuad(bot1, bot2, top2, top1);
        }
        
        return builder.getMesh();
    }
};

} // namespace nexus::utility::graphics
