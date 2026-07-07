#pragma once

#include <vector>
#include <array>
#include <cstdint>
#include <cmath>
#include <limits>
#include <utility>
#include <algorithm>
#include <unordered_map>
#include <unordered_set>
#include <nexus/utility/math/vector3_utils.h>

namespace nexus::utility::graphics {
using Vector3 = nexus::utility::math::Vector3;

/// @brief Repair of common triangle-mesh defects.
///
/// Provides vertex welding, degenerate-triangle removal, hole filling by
/// planar ear-clipping of boundary loops, T-junction splitting, isolated-vertex
/// pruning, non-manifold-edge cleanup, and a combined repair pipeline that
/// applies each fix in a sensible order and reports how many defects were
/// corrected.
class MeshRepair {
public:
    using Vector3 = nexus::utility::math::Vector3;

    struct DedupResult {
        std::vector<Vector3> verts;
        std::vector<unsigned> indices;
        std::vector<unsigned> vertexMap;  // old index -> new index
    };
    struct FillResult {
        std::vector<Vector3> verts;
        std::vector<unsigned> indices;
        int holesFilled = 0;
    };
    struct IsolatedResult {
        std::vector<Vector3> verts;
        std::vector<unsigned> indices;
        int removedCount = 0;
    };
    struct TJunctionResult {
        std::vector<Vector3> verts;
        std::vector<unsigned> indices;
        int tJunctionsFixed = 0;
    };
    struct RepairResult {
        std::vector<Vector3> verts;
        std::vector<unsigned> indices;
        int dupVertsRemoved = 0;
        int degenTrisRemoved = 0;
        int holesFilled = 0;
        int tJunctionsFixed = 0;
        int isoVertsRemoved = 0;
    };

    // ── Vertex welding ──────────────────────────────────────────────────

    /// @brief Merge vertices closer than `epsilon` via a spatial hash grid.
    static DedupResult removeDuplicateVertices(const std::vector<Vector3>& verts,
                                               const std::vector<unsigned>& indices,
                                               float epsilon) {
        DedupResult out;
        const int n = static_cast<int>(verts.size());
        out.vertexMap.assign(n, 0);
        float cell = std::max(epsilon, 1e-8f);

        struct CKey {
            int x, y, z;
            bool operator==(const CKey& o) const { return x == o.x && y == o.y && z == o.z; }
        };
        struct CHash {
            size_t operator()(const CKey& k) const {
                size_t h = 1469598103934665603ULL;
                auto mix = [&](int v) {
                    h ^= static_cast<size_t>(static_cast<uint32_t>(v));
                    h *= 1099511628211ULL;
                };
                mix(k.x); mix(k.y); mix(k.z);
                return h;
            }
        };
        std::unordered_map<CKey, std::vector<int>, CHash> hash;
        float eps2 = epsilon * epsilon;

        auto keyOf = [&](const Vector3& v) {
            return CKey{static_cast<int>(std::floor(v.x / cell)),
                        static_cast<int>(std::floor(v.y / cell)),
                        static_cast<int>(std::floor(v.z / cell))};
        };

        for (int i = 0; i < n; ++i) {
            CKey base = keyOf(verts[i]);
            int found = -1;
            for (int dz = -1; dz <= 1 && found < 0; ++dz)
                for (int dy = -1; dy <= 1 && found < 0; ++dy)
                    for (int dx = -1; dx <= 1 && found < 0; ++dx) {
                        CKey k{base.x + dx, base.y + dy, base.z + dz};
                        auto it = hash.find(k);
                        if (it == hash.end()) continue;
                        for (int cand : it->second)
                            if ((out.verts[cand] - verts[i]).lengthSquared() <= eps2) {
                                found = cand;
                                break;
                            }
                    }
            if (found >= 0) {
                out.vertexMap[i] = static_cast<unsigned>(found);
            } else {
                int newIdx = static_cast<int>(out.verts.size());
                out.verts.push_back(verts[i]);
                hash[base].push_back(newIdx);
                out.vertexMap[i] = static_cast<unsigned>(newIdx);
            }
        }

        out.indices.reserve(indices.size());
        for (size_t t = 0; t + 2 < indices.size(); t += 3) {
            unsigned a = out.vertexMap[indices[t]];
            unsigned b = out.vertexMap[indices[t + 1]];
            unsigned c = out.vertexMap[indices[t + 2]];
            if (a == b || b == c || a == c) continue;
            out.indices.push_back(a);
            out.indices.push_back(b);
            out.indices.push_back(c);
        }
        return out;
    }

    // ── Degenerate triangles ────────────────────────────────────────────

    /// @brief Drop triangles with area below `epsilon` or repeated vertices.
    static std::vector<unsigned> removeDegenerateTriangles(const std::vector<Vector3>& verts,
                                                           const std::vector<unsigned>& indices,
                                                           float epsilon) {
        std::vector<unsigned> out;
        out.reserve(indices.size());
        for (size_t t = 0; t + 2 < indices.size(); t += 3) {
            unsigned a = indices[t], b = indices[t + 1], c = indices[t + 2];
            if (a == b || b == c || a == c) continue;
            float area = 0.5f * cross(verts[b] - verts[a], verts[c] - verts[a]).length();
            if (area < epsilon) continue;
            out.push_back(a);
            out.push_back(b);
            out.push_back(c);
        }
        return out;
    }

    // ── Hole filling ────────────────────────────────────────────────────

    /// @brief Fill boundary loops up to `maxHoleSize` edges via ear clipping.
    static FillResult fillHoles(const std::vector<Vector3>& verts,
                                const std::vector<unsigned>& indices,
                                int maxHoleSize) {
        FillResult out;
        out.verts = verts;
        out.indices = indices;

        // Directed boundary edges = those whose reverse is absent.
        std::unordered_map<uint64_t, int> dirCount;
        auto dkey = [](unsigned a, unsigned b) {
            return (static_cast<uint64_t>(a) << 32) | static_cast<uint64_t>(b);
        };
        for (size_t t = 0; t + 2 < indices.size(); t += 3) {
            unsigned tri[3] = {indices[t], indices[t + 1], indices[t + 2]};
            for (int e = 0; e < 3; ++e)
                ++dirCount[dkey(tri[e], tri[(e + 1) % 3])];
        }

        std::unordered_map<unsigned, unsigned> nextOf;
        for (const auto& kv : dirCount) {
            unsigned a = static_cast<unsigned>(kv.first >> 32);
            unsigned b = static_cast<unsigned>(kv.first & 0xffffffffu);
            if (dirCount.find(dkey(b, a)) == dirCount.end())
                nextOf[a] = b;  // boundary edge a->b
        }

        std::unordered_set<unsigned> used;
        for (const auto& kv : nextOf) {
            unsigned start = kv.first;
            if (used.count(start)) continue;

            std::vector<unsigned> loop;
            unsigned cur = start;
            bool ok = true;
            while (true) {
                if (used.count(cur)) { ok = (cur == start); break; }
                used.insert(cur);
                loop.push_back(cur);
                auto it = nextOf.find(cur);
                if (it == nextOf.end()) { ok = false; break; }
                cur = it->second;
                if (cur == start) break;
                if (loop.size() > indices.size()) { ok = false; break; }
            }
            if (!ok || loop.size() < 3) continue;
            if (static_cast<int>(loop.size()) > maxHoleSize) continue;

            triangulateLoop(out.verts, loop, out.indices);
            ++out.holesFilled;
        }
        return out;
    }

    // ── T-junction repair ───────────────────────────────────────────────

    /// @brief Split triangle edges that pass through another vertex.
    static TJunctionResult fixTJunctions(const std::vector<Vector3>& verts,
                                         const std::vector<unsigned>& indices) {
        TJunctionResult out;
        out.verts = verts;
        std::vector<std::array<unsigned, 3>> tris;
        for (size_t t = 0; t + 2 < indices.size(); t += 3)
            tris.push_back({indices[t], indices[t + 1], indices[t + 2]});

        constexpr float EPS = 1e-5f;
        bool changed = true;
        int guard = 0;
        const int maxPasses = static_cast<int>(tris.size()) * 4 + 16;
        while (changed && guard++ < maxPasses) {
            changed = false;
            for (size_t f = 0; f < tris.size(); ++f) {
                auto tri = tris[f];
                for (int e = 0; e < 3; ++e) {
                    unsigned a = tri[e];
                    unsigned b = tri[(e + 1) % 3];
                    unsigned opp = tri[(e + 2) % 3];
                    int hit = findVertexOnEdge(out.verts, a, b, opp, EPS);
                    if (hit < 0) continue;
                    // Replace triangle (a,b,opp) with (a,hit,opp) and (hit,b,opp).
                    tris[f] = {a, static_cast<unsigned>(hit), opp};
                    tris.push_back({static_cast<unsigned>(hit), b, opp});
                    ++out.tJunctionsFixed;
                    changed = true;
                    break;
                }
                if (changed) break;
            }
        }

        for (const auto& tr : tris) {
            out.indices.push_back(tr[0]);
            out.indices.push_back(tr[1]);
            out.indices.push_back(tr[2]);
        }
        return out;
    }

    // ── Isolated vertices ───────────────────────────────────────────────

    /// @brief Remove vertices not referenced by any triangle and compact indices.
    static IsolatedResult removeIsolatedVertices(const std::vector<Vector3>& verts,
                                                 const std::vector<unsigned>& indices) {
        IsolatedResult out;
        std::vector<int> remap(verts.size(), -1);
        for (unsigned idx : indices) {
            if (idx < remap.size() && remap[idx] < 0) {
                remap[idx] = static_cast<int>(out.verts.size());
                out.verts.push_back(verts[idx]);
            }
        }
        out.indices.reserve(indices.size());
        for (unsigned idx : indices)
            out.indices.push_back(static_cast<unsigned>(remap[idx]));
        out.removedCount = static_cast<int>(verts.size()) - static_cast<int>(out.verts.size());
        return out;
    }

    // ── Non-manifold edges ──────────────────────────────────────────────

    /// @brief Greedily drop faces that would push an edge beyond two incidences.
    static std::vector<unsigned> removeNonManifoldEdges(const std::vector<unsigned>& indices) {
        std::vector<unsigned> out;
        std::unordered_map<uint64_t, int> edgeUse;
        auto ekey = [](unsigned a, unsigned b) {
            if (a > b) std::swap(a, b);
            return (static_cast<uint64_t>(a) << 32) | static_cast<uint64_t>(b);
        };
        for (size_t t = 0; t + 2 < indices.size(); t += 3) {
            unsigned tri[3] = {indices[t], indices[t + 1], indices[t + 2]};
            bool ok = true;
            for (int e = 0; e < 3; ++e) {
                auto it = edgeUse.find(ekey(tri[e], tri[(e + 1) % 3]));
                if (it != edgeUse.end() && it->second >= 2) { ok = false; break; }
            }
            if (!ok) continue;
            for (int e = 0; e < 3; ++e) ++edgeUse[ekey(tri[e], tri[(e + 1) % 3])];
            out.push_back(tri[0]);
            out.push_back(tri[1]);
            out.push_back(tri[2]);
        }
        return out;
    }

    // ── Combined pipeline ───────────────────────────────────────────────

    /// @brief Apply every repair step in order and report the counts.
    static RepairResult repair(const std::vector<Vector3>& verts,
                               const std::vector<unsigned>& indices,
                               int maxHoleSize = 64) {
        RepairResult result;

        DedupResult dd = removeDuplicateVertices(verts, indices, 1e-5f);
        result.dupVertsRemoved = static_cast<int>(verts.size()) - static_cast<int>(dd.verts.size());

        size_t beforeTris = dd.indices.size() / 3;
        std::vector<unsigned> idx = removeDegenerateTriangles(dd.verts, dd.indices, 1e-12f);
        result.degenTrisRemoved = static_cast<int>(beforeTris - idx.size() / 3);

        idx = removeNonManifoldEdges(idx);

        IsolatedResult iso = removeIsolatedVertices(dd.verts, idx);
        result.isoVertsRemoved = iso.removedCount;

        FillResult fill = fillHoles(iso.verts, iso.indices, maxHoleSize);
        result.holesFilled = fill.holesFilled;

        TJunctionResult tj = fixTJunctions(fill.verts, fill.indices);
        result.tJunctionsFixed = tj.tJunctionsFixed;

        result.verts = std::move(tj.verts);
        result.indices = std::move(tj.indices);
        return result;
    }

private:
    static float dot(const Vector3& a, const Vector3& b) {
        return a.x * b.x + a.y * b.y + a.z * b.z;
    }
    static Vector3 cross(const Vector3& a, const Vector3& b) {
        return {a.y * b.z - a.z * b.y,
                a.z * b.x - a.x * b.z,
                a.x * b.y - a.y * b.x};
    }

    // Find a vertex lying strictly inside segment (a,b), excluding a/b/opp.
    static int findVertexOnEdge(const std::vector<Vector3>& verts, unsigned a, unsigned b,
                                unsigned opp, float eps) {
        const Vector3& pa = verts[a];
        const Vector3& pb = verts[b];
        Vector3 ab = pb - pa;
        float len2 = ab.lengthSquared();
        if (len2 < 1e-12f) return -1;
        for (unsigned v = 0; v < verts.size(); ++v) {
            if (v == a || v == b || v == opp) continue;
            Vector3 ap = verts[v] - pa;
            float t = dot(ap, ab) / len2;
            if (t <= eps || t >= 1.0f - eps) continue;
            Vector3 proj = pa + ab * t;
            if ((verts[v] - proj).length() <= eps) return static_cast<int>(v);
        }
        return -1;
    }

    // Ear-clip a boundary loop in its best-fit plane and append triangles.
    static void triangulateLoop(const std::vector<Vector3>& verts,
                                const std::vector<unsigned>& loop,
                                std::vector<unsigned>& outIndices) {
        const size_t m = loop.size();
        if (m < 3) return;

        // Newell's method for a robust loop normal.
        Vector3 normal;
        Vector3 centroid;
        for (size_t i = 0; i < m; ++i) {
            const Vector3& cu = verts[loop[i]];
            const Vector3& nx = verts[loop[(i + 1) % m]];
            normal.x += (cu.y - nx.y) * (cu.z + nx.z);
            normal.y += (cu.z - nx.z) * (cu.x + nx.x);
            normal.z += (cu.x - nx.x) * (cu.y + nx.y);
            centroid += cu;
        }
        float nl = normal.length();
        if (nl < 1e-12f) { fanTriangulate(loop, outIndices); return; }
        normal = normal / nl;
        centroid = centroid / static_cast<float>(m);

        Vector3 uAxis = (std::abs(normal.x) < 0.9f) ? cross(normal, Vector3(1, 0, 0))
                                                    : cross(normal, Vector3(0, 1, 0));
        uAxis = uAxis.normalized();
        Vector3 vAxis = cross(normal, uAxis).normalized();

        struct P2 { float x, y; unsigned idx; };
        std::vector<P2> poly(m);
        for (size_t i = 0; i < m; ++i) {
            Vector3 d = verts[loop[i]] - centroid;
            poly[i] = {dot(d, uAxis), dot(d, vAxis), loop[i]};
        }

        // Ensure counter-clockwise orientation in the 2D plane.
        float area = 0.0f;
        for (size_t i = 0; i < m; ++i) {
            const P2& p = poly[i];
            const P2& q = poly[(i + 1) % m];
            area += p.x * q.y - q.x * p.y;
        }
        if (area < 0.0f) std::reverse(poly.begin(), poly.end());

        std::vector<int> idxList(poly.size());
        for (int i = 0; i < static_cast<int>(poly.size()); ++i) idxList[i] = i;

        int guard = 0;
        const int maxGuard = static_cast<int>(poly.size()) * static_cast<int>(poly.size()) + 8;
        while (idxList.size() > 3 && guard++ < maxGuard) {
            bool clipped = false;
            int cnt = static_cast<int>(idxList.size());
            for (int i = 0; i < cnt; ++i) {
                int i0 = idxList[(i + cnt - 1) % cnt];
                int i1 = idxList[i];
                int i2 = idxList[(i + 1) % cnt];
                const P2& a = poly[i0];
                const P2& b = poly[i1];
                const P2& c = poly[i2];
                float crossZ = (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
                if (crossZ <= 0.0f) continue;  // reflex or collinear
                bool ear = true;
                for (int j = 0; j < cnt; ++j) {
                    int ij = idxList[j];
                    if (ij == i0 || ij == i1 || ij == i2) continue;
                    if (pointInTriangle(poly[ij], a, b, c)) { ear = false; break; }
                }
                if (!ear) continue;
                outIndices.push_back(poly[i0].idx);
                outIndices.push_back(poly[i1].idx);
                outIndices.push_back(poly[i2].idx);
                idxList.erase(idxList.begin() + i);
                clipped = true;
                break;
            }
            if (!clipped) break;  // fall back for non-simple loops
        }
        if (idxList.size() == 3) {
            outIndices.push_back(poly[idxList[0]].idx);
            outIndices.push_back(poly[idxList[1]].idx);
            outIndices.push_back(poly[idxList[2]].idx);
        }
    }

    template <typename P>
    static bool pointInTriangle(const P& p, const P& a, const P& b, const P& c) {
        float d1 = sign(p, a, b);
        float d2 = sign(p, b, c);
        float d3 = sign(p, c, a);
        bool hasNeg = (d1 < 0) || (d2 < 0) || (d3 < 0);
        bool hasPos = (d1 > 0) || (d2 > 0) || (d3 > 0);
        return !(hasNeg && hasPos);
    }
    template <typename P>
    static float sign(const P& p, const P& a, const P& b) {
        return (p.x - b.x) * (a.y - b.y) - (a.x - b.x) * (p.y - b.y);
    }

    static void fanTriangulate(const std::vector<unsigned>& loop,
                               std::vector<unsigned>& outIndices) {
        for (size_t i = 1; i + 1 < loop.size(); ++i) {
            outIndices.push_back(loop[0]);
            outIndices.push_back(loop[i]);
            outIndices.push_back(loop[i + 1]);
        }
    }
};

} // namespace nexus::utility::graphics
