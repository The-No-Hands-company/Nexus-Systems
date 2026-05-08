#include <nexus/geometry/MeshDiagnosticOverlay.h>

#include <algorithm>
#include <cmath>
#include <map>
#include <vector>

namespace nexus::geometry {

namespace {

using Vec3 = nexus::render::Vec3;

inline Vec3 vec3Sub(const Vec3& a, const Vec3& b) noexcept
{
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}

inline Vec3 vec3Cross(const Vec3& a, const Vec3& b) noexcept
{
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x,
    };
}

inline float vec3LengthSq(const Vec3& v) noexcept
{
    return v.x * v.x + v.y * v.y + v.z * v.z;
}

struct EdgeKey {
    uint32_t v0 = 0;
    uint32_t v1 = 0;

    [[nodiscard]] bool operator<(const EdgeKey& o) const noexcept
    {
        return v0 != o.v0 ? v0 < o.v0 : v1 < o.v1;
    }
};

} // namespace

bool MeshDiagnosticExporter::extract(const Mesh&                         mesh,
                                     const MeshDiagnosticOverlayOptions& options,
                                     MeshDiagnosticOverlayData&          out) noexcept
{
    out.clear();

    const size_t faceCount   = mesh.topology().faceCount();
    const size_t vertexCount = mesh.attributes().vertexCount();

    if (faceCount == 0u) {
        return false;
    }

    // ── Edge → face-count map ─────────────────────────────────────────────
    const bool wantNonManifold = hasMode(options.modes, MeshOverlayMode::NonManifoldEdges);
    const bool wantBoundary    = hasMode(options.modes, MeshOverlayMode::BoundaryEdges);
    const bool wantDegenerate  = hasMode(options.modes, MeshOverlayMode::DegenerateFaces);
    const bool wantIsolated    = hasMode(options.modes, MeshOverlayMode::IsolatedVertices);

    if (wantNonManifold || wantBoundary) {
        std::map<EdgeKey, uint32_t> edgeFaceCount;
        for (size_t fi = 0; fi < faceCount; ++fi) {
            const Face& f = mesh.topology().face(fi);
            const size_t n = f.indices.size();
            for (size_t i = 0; i < n; ++i) {
                const uint32_t a = f.indices[i];
                const uint32_t b = f.indices[(i + 1) % n];
                const EdgeKey key{std::min(a, b), std::max(a, b)};
                ++edgeFaceCount[key];
            }
        }

        for (const auto& [key, count] : edgeFaceCount) {
            if (count > 2u && wantNonManifold) {
                out.edges.push_back({key.v0, key.v1, OverlayEdgeKind::NonManifold});
                ++out.nonManifoldEdgeCount;
            } else if (count == 1u && wantBoundary) {
                out.edges.push_back({key.v0, key.v1, OverlayEdgeKind::Boundary});
                ++out.boundaryEdgeCount;
            }
        }
    }

    // ── Degenerate faces ──────────────────────────────────────────────────
    if (wantDegenerate) {
        const auto& positions = mesh.attributes().positions();
        const float areaSqThreshold = options.degenerateAreaThreshold
                                    * options.degenerateAreaThreshold;

        for (size_t fi = 0; fi < faceCount; ++fi) {
            const Face& f = mesh.topology().face(fi);

            // Check for repeated indices first (degenerate by construction)
            bool hasRepeat = false;
            for (size_t i = 0; i < f.indices.size() && !hasRepeat; ++i) {
                for (size_t j = i + 1; j < f.indices.size() && !hasRepeat; ++j) {
                    if (f.indices[i] == f.indices[j]) {
                        hasRepeat = true;
                    }
                }
            }

            if (hasRepeat) {
                out.faces.push_back({static_cast<uint32_t>(fi), OverlayFaceKind::Degenerate});
                ++out.degenerateFaceCount;
                continue;
            }

            // For triangles, also check geometric area
            if (f.indices.size() == 3u && positions.size() > 0u) {
                const uint32_t i0 = f.indices[0];
                const uint32_t i1 = f.indices[1];
                const uint32_t i2 = f.indices[2];
                if (i0 < positions.size() && i1 < positions.size() && i2 < positions.size()) {
                    const Vec3 e01 = vec3Sub(positions[i1], positions[i0]);
                    const Vec3 e02 = vec3Sub(positions[i2], positions[i0]);
                    const Vec3 cross = vec3Cross(e01, e02);
                    // |cross|^2 = (2 * area)^2
                    if (vec3LengthSq(cross) * 0.25f < areaSqThreshold) {
                        out.faces.push_back({static_cast<uint32_t>(fi),
                                             OverlayFaceKind::Degenerate});
                        ++out.degenerateFaceCount;
                    }
                }
            }
        }
    }

    // ── Isolated vertices ─────────────────────────────────────────────────
    if (wantIsolated && vertexCount > 0u) {
        std::vector<bool> referenced(vertexCount, false);
        for (size_t fi = 0; fi < faceCount; ++fi) {
            const Face& f = mesh.topology().face(fi);
            for (const uint32_t idx : f.indices) {
                if (idx < vertexCount) {
                    referenced[idx] = true;
                }
            }
        }
        for (uint32_t vi = 0; vi < static_cast<uint32_t>(vertexCount); ++vi) {
            if (!referenced[vi]) {
                out.vertices.push_back({vi, OverlayVertexKind::Isolated});
                ++out.isolatedVertexCount;
            }
        }
    }

    return true;
}

} // namespace nexus::geometry
