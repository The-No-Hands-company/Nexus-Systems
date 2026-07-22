#pragma once

#include <nexus/geometry/Mesh.h>

#include <array>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace nexus::geometry {

struct ConstraintEdge { uint32_t a, b; };

struct CDTResult {
    bool ok = false;
    std::string error;
    std::vector<std::array<uint32_t, 3>> triangles;
    std::vector<Vec2> vertices;
    std::vector<ConstraintEdge> constraints;
};

class ConstrainedDelaunay2D {
public:
    [[nodiscard]] static CDTResult triangulate(
        const std::vector<Vec2>& points,
        const std::vector<ConstraintEdge>& constraints);

    [[nodiscard]] bool isConstrained(uint32_t a, uint32_t b) const;

private:
    std::vector<std::array<uint32_t, 3>> m_triangles;
    std::vector<Vec2> m_vertices;
    std::vector<ConstraintEdge> m_constraints;

    struct EdgeKey {
        uint32_t a, b;
        bool operator==(const EdgeKey& o) const noexcept {
            return (a == o.a && b == o.b) || (a == o.b && b == o.a);
        }
    };
    struct EdgeKeyHash {
        std::size_t operator()(EdgeKey k) const noexcept {
            return std::hash<uint64_t>{}((static_cast<uint64_t>(k.a < k.b ? k.a : k.b) << 32)
                                       | static_cast<uint64_t>(k.a < k.b ? k.b : k.a));
        }
    };
    std::unordered_map<EdgeKey, bool, EdgeKeyHash> m_constrainedEdges;

    [[nodiscard]] static int orient2D(const Vec2& a, const Vec2& b, const Vec2& c) noexcept;
    [[nodiscard]] static bool inCircle(const Vec2& a, const Vec2& b, const Vec2& c,
                                       const Vec2& d) noexcept;
    [[nodiscard]] static bool segmentsCross(const Vec2& p1, const Vec2& p2,
                                            const Vec2& q1, const Vec2& q2) noexcept;

    void buildDelaunay();
    // One Bowyer-Watson pass with the super-triangle sized at `scale` times the input's
    // bounding-box extent. Returns whether the result tiles the input's convex hull
    // (`hullArea`); a false return means the super-triangle was too small and the caller
    // must retry larger. See src/geometry/DelaunaySuperTriangle.h.
    bool buildDelaunayAtScale(float scale, double hullArea);
    void enforceConstraint(uint32_t a, uint32_t b);
    int findTriangleContaining(const Vec2& p) const noexcept;
    int findAdjacentTriangle(uint32_t edgeA, uint32_t edgeB, int excludeTri) const noexcept;
    void flipEdge(uint32_t a, uint32_t b);
};

} // namespace nexus::geometry
