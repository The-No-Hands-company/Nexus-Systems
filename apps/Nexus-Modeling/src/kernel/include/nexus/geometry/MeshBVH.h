#pragma once

#include <nexus/geometry/Mesh.h>

#include <cmath>
#include <cstdint>
#include <limits>
#include <vector>

namespace nexus::geometry {

struct Ray {
    nexus::render::Vec3 origin;
    nexus::render::Vec3 direction;
};

struct RayHit {
    float t = std::numeric_limits<float>::max();
    uint32_t triangleIndex = 0;
    float u = 0.f;
    float v = 0.f;
};

struct ClosestPointHit {
    float distanceSquared = std::numeric_limits<float>::max();
    uint32_t triangleIndex = 0;
    float baryU = 0.f;
    float baryV = 0.f;
    nexus::render::Vec3 point = {0.f, 0.f, 0.f};
    bool valid = false;
};

class MeshBVH {
public:
    struct Tri {
        nexus::render::Vec3 v0;
        nexus::render::Vec3 e1;
        nexus::render::Vec3 e2;
        uint32_t srcFace = 0;

        nexus::render::Vec3 center() const noexcept {
            return {
                v0.x + (e1.x + e2.x) * (1.f / 3.f),
                v0.y + (e1.y + e2.y) * (1.f / 3.f),
                v0.z + (e1.z + e2.z) * (1.f / 3.f),
            };
        }

        nexus::render::Vec3 normal() const noexcept {
            return e1.cross(e2).normalize();
        }
    };

    struct Node {
        nexus::render::Vec3 min;
        nexus::render::Vec3 max;
        int32_t leftChild = -1;
        int32_t firstTri = 0;
        int32_t triCount = 0;
        bool isLeaf = false;
    };

    void build(const Mesh& mesh);
    [[nodiscard]] RayHit raycast(const Ray& ray) const noexcept;
    [[nodiscard]] ClosestPointHit closestPoint(const nexus::render::Vec3& query) const noexcept;
    [[nodiscard]] bool isValid() const noexcept;

    // Every triangle whose bounding box the segment a→b passes through, appended to `out`
    // (which is cleared first). Unlike raycast, which reports only the nearest hit, this
    // is for callers that must consider EVERY triangle along a segment — a point-in-solid
    // parity count is the motivating case, where missing one crossing inverts the answer.
    //
    // Deliberately CONSERVATIVE: the box test is padded, so the result may contain
    // triangles the segment misses but never omits one it meets. A caller doing exact
    // parity re-tests each candidate exactly, so extra candidates cost time and change no
    // answer, while a single omission would be a wrong answer.
    void collectSegmentCandidates(const nexus::render::Vec3& a, const nexus::render::Vec3& b,
                                  std::vector<uint32_t>& out) const;

    // Every triangle whose bounding box overlaps the query box, appended to `out` (cleared
    // first). The broad phase for pairing two meshes: without it, finding which triangles
    // of B can meet a triangle of A means testing all of them.
    //
    // Conservative in the same way as the segment query — it may return triangles whose
    // boxes only just miss, never omit one that overlaps — so a caller may re-apply its own
    // exact box test and get precisely the set it would have found by brute force.
    void collectBoxCandidates(const nexus::render::Vec3& lo, const nexus::render::Vec3& hi,
                              std::vector<uint32_t>& out) const;

    [[nodiscard]] const std::vector<Node>& nodes() const noexcept { return m_nodes; }
    [[nodiscard]] const std::vector<Tri>& tris() const noexcept { return m_tris; }

private:
    struct BuildTri {
        nexus::render::Vec3 centroid;
        nexus::render::Vec3 aabbMin;
        nexus::render::Vec3 aabbMax;
        uint32_t srcFace = 0;
        // Where this triangle sits in m_tris BEFORE the build permutes anything. The
        // build sorts these entries, so m_tris has to be permuted to match afterwards or
        // leaf ranges will address the wrong triangles entirely.
        uint32_t triIndex = 0;
    };

    int32_t buildNode(std::vector<BuildTri>& items, int32_t first, int32_t count,
                      int32_t depth);
    [[nodiscard]] static bool intersectAabb(const Node& node, const Ray& ray, float& tMin, float& tMax) noexcept;
    [[nodiscard]] static bool intersectTri(const Tri& tri, const Ray& ray, float& t, float& u, float& v) noexcept;

    std::vector<Node> m_nodes;
    std::vector<Tri> m_tris;
    bool m_valid = false;
};

} // namespace nexus::geometry
