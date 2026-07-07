#pragma once

#include <vector>
#include <array>
#include <map>
#include <cmath>
#include <algorithm>
#include <limits>
#include <cstdint>
#include <nexus/utility/math/vector3_utils.h>

namespace nexus::utility::graphics {
using Vector3 = nexus::utility::math::Vector3;

/// @brief Convex hull of a 3D point set via the QuickHull algorithm.
///
/// Builds a triangulated convex hull with consistently outward-facing normals.
/// The algorithm seeds an initial tetrahedron from extreme points, assigns each
/// remaining point to the face it lies in front of, then repeatedly picks the
/// point farthest from the hull, deletes the faces it can "see", and stitches
/// new faces across the horizon back to that point until no point lies outside.
class ConvexHull3D {
public:
    using Vector3 = nexus::utility::math::Vector3;

    /// @brief Indexed triangle mesh describing the hull.
    struct HullResult {
        std::vector<Vector3> vertices;
        std::vector<unsigned> faceIndices; // triples, CCW seen from outside
    };

    /// @brief Compute the convex hull of the given points.
    HullResult computeHull(const std::vector<Vector3>& points) {
        points_ = points;
        faces_.clear();

        HullResult result;
        if (points_.size() < 4) {
            // Degenerate: return the input points, no closed hull possible.
            result.vertices = points_;
            return result;
        }

        epsilon_ = computeEpsilon();
        if (!buildInitialTetrahedron()) {
            result.vertices = points_;
            return result;
        }

        std::vector<int> stack;
        for (int f = 0; f < static_cast<int>(faces_.size()); ++f)
            if (!faces_[f].outside.empty()) stack.push_back(f);

        long safety = static_cast<long>(points_.size()) * 40 + 1000;
        while (!stack.empty() && --safety > 0) {
            int fi = stack.back();
            stack.pop_back();
            if (fi < 0 || fi >= static_cast<int>(faces_.size())) continue;
            if (!faces_[fi].active || faces_[fi].outside.empty()) continue;

            const int apex = furthestPoint(faces_[fi]);
            if (apex < 0) { faces_[fi].outside.clear(); continue; }

            // Collect the faces visible from the apex.
            std::vector<int> visible;
            for (int f = 0; f < static_cast<int>(faces_.size()); ++f) {
                if (!faces_[f].active) continue;
                if (distanceToFace(faces_[f], points_[apex]) > epsilon_)
                    visible.push_back(f);
            }
            if (visible.empty()) { faces_[fi].outside.clear(); continue; }

            // Horizon = undirected edges owned by exactly one visible face.
            std::map<std::pair<int, int>, int> edgeCount;
            for (int f : visible) {
                const Face& face = faces_[f];
                addEdge(edgeCount, face.v[0], face.v[1]);
                addEdge(edgeCount, face.v[1], face.v[2]);
                addEdge(edgeCount, face.v[2], face.v[0]);
            }

            // Gather orphaned points and deactivate visible faces.
            std::vector<int> orphans;
            for (int f : visible) {
                for (int p : faces_[f].outside)
                    if (p != apex) orphans.push_back(p);
                faces_[f].active = false;
                faces_[f].outside.clear();
            }

            // Create new faces from each horizon edge to the apex.
            std::vector<int> newFaces;
            for (const auto& kv : edgeCount) {
                if (kv.second != 1) continue; // interior edge, skip
                int u = kv.first.first;
                int w = kv.first.second;
                int nf = makeFace(u, w, apex);
                if (nf >= 0) newFaces.push_back(nf);
            }

            // Redistribute orphan points onto the new faces.
            for (int p : orphans) {
                if (p == apex) continue;
                int best = -1;
                float bestDist = epsilon_;
                for (int nf : newFaces) {
                    float d = distanceToFace(faces_[nf], points_[p]);
                    if (d > bestDist) { bestDist = d; best = nf; }
                }
                if (best >= 0) faces_[best].outside.push_back(p);
            }

            for (int nf : newFaces)
                if (!faces_[nf].outside.empty()) stack.push_back(nf);
        }

        return buildResult();
    }

    /// @brief Signed volume of the most recently computed hull.
    float getVolume() const {
        double vol = 0.0;
        for (const Face& f : faces_) {
            if (!f.active) continue;
            const Vector3& a = points_[f.v[0]];
            const Vector3& b = points_[f.v[1]];
            const Vector3& c = points_[f.v[2]];
            vol += static_cast<double>(dot(a, cross(b, c)));
        }
        return static_cast<float>(std::abs(vol) / 6.0);
    }

    /// @brief Total surface area of the hull.
    float getSurfaceArea() const {
        double area = 0.0;
        for (const Face& f : faces_) {
            if (!f.active) continue;
            const Vector3& a = points_[f.v[0]];
            const Vector3& b = points_[f.v[1]];
            const Vector3& c = points_[f.v[2]];
            area += 0.5 * static_cast<double>(cross(b - a, c - a).length());
        }
        return static_cast<float>(area);
    }

    /// @brief True if a point lies inside (or on) the convex hull.
    bool isInside(const Vector3& p, float tolerance = 1e-5f) const {
        bool any = false;
        for (const Face& f : faces_) {
            if (!f.active) continue;
            any = true;
            if (distanceToFace(f, p) > tolerance) return false;
        }
        return any; // no faces -> not a valid hull
    }

private:
    struct Face {
        std::array<int, 3> v;
        Vector3 normal;
        float offset = 0.0f;     // dot(normal, points_[v[0]])
        bool active = true;
        std::vector<int> outside; // indices of points in front of this face
    };

    std::vector<Vector3> points_;
    std::vector<Face> faces_;
    Vector3 interior_{};
    float epsilon_ = 1e-6f;

    static float dot(const Vector3& a, const Vector3& b) {
        return a.x * b.x + a.y * b.y + a.z * b.z;
    }
    static Vector3 cross(const Vector3& a, const Vector3& b) {
        return {a.y * b.z - a.z * b.y,
                a.z * b.x - a.x * b.z,
                a.x * b.y - a.y * b.x};
    }

    float computeEpsilon() const {
        Vector3 lo(std::numeric_limits<float>::max(),
                   std::numeric_limits<float>::max(),
                   std::numeric_limits<float>::max());
        Vector3 hi(std::numeric_limits<float>::lowest(),
                   std::numeric_limits<float>::lowest(),
                   std::numeric_limits<float>::lowest());
        for (const Vector3& p : points_) {
            lo.x = std::min(lo.x, p.x); lo.y = std::min(lo.y, p.y); lo.z = std::min(lo.z, p.z);
            hi.x = std::max(hi.x, p.x); hi.y = std::max(hi.y, p.y); hi.z = std::max(hi.z, p.z);
        }
        float scale = std::max({std::abs(hi.x - lo.x), std::abs(hi.y - lo.y),
                                std::abs(hi.z - lo.z), 1.0f});
        return scale * 1e-6f;
    }

    static void addEdge(std::map<std::pair<int, int>, int>& m, int a, int b) {
        if (a > b) std::swap(a, b);
        ++m[{a, b}];
    }

    float distanceToFace(const Face& f, const Vector3& p) const {
        return dot(f.normal, p) - f.offset;
    }

    int furthestPoint(const Face& f) const {
        int best = -1;
        float bestDist = epsilon_;
        for (int p : f.outside) {
            float d = distanceToFace(f, points_[p]);
            if (d > bestDist) { bestDist = d; best = p; }
        }
        return best;
    }

    // Create an outward-oriented face; returns its index or -1 if degenerate.
    int makeFace(int a, int b, int c) {
        Face f;
        f.v = {a, b, c};
        Vector3 n = cross(points_[b] - points_[a], points_[c] - points_[a]);
        float len = n.length();
        if (len < 1e-12f) return -1;
        n = n / len;
        // Orient so the interior point is on the negative side.
        if (dot(n, interior_ - points_[a]) > 0.0f) {
            std::swap(f.v[1], f.v[2]);
            n = n * -1.0f;
        }
        f.normal = n;
        f.offset = dot(n, points_[f.v[0]]);
        f.active = true;
        int idx = static_cast<int>(faces_.size());
        faces_.push_back(std::move(f));
        return idx;
    }

    bool buildInitialTetrahedron() {
        const int n = static_cast<int>(points_.size());

        // Extreme points along the 6 axis directions.
        int ext[6] = {0, 0, 0, 0, 0, 0};
        for (int i = 1; i < n; ++i) {
            if (points_[i].x < points_[ext[0]].x) ext[0] = i;
            if (points_[i].x > points_[ext[1]].x) ext[1] = i;
            if (points_[i].y < points_[ext[2]].y) ext[2] = i;
            if (points_[i].y > points_[ext[3]].y) ext[3] = i;
            if (points_[i].z < points_[ext[4]].z) ext[4] = i;
            if (points_[i].z > points_[ext[5]].z) ext[5] = i;
        }

        // p0, p1: farthest apart among extreme points.
        int p0 = -1, p1 = -1;
        float maxD = -1.0f;
        for (int i = 0; i < 6; ++i)
            for (int j = i + 1; j < 6; ++j) {
                float d = (points_[ext[i]] - points_[ext[j]]).lengthSquared();
                if (d > maxD) { maxD = d; p0 = ext[i]; p1 = ext[j]; }
            }
        if (p0 < 0 || p1 < 0 || maxD < epsilon_ * epsilon_) return false;

        // p2: farthest from the line p0-p1.
        int p2 = -1;
        float maxLineDist = -1.0f;
        const Vector3 lineDir = (points_[p1] - points_[p0]).normalized();
        for (int i = 0; i < n; ++i) {
            Vector3 ap = points_[i] - points_[p0];
            Vector3 perp = ap - lineDir * dot(ap, lineDir);
            float d = perp.lengthSquared();
            if (d > maxLineDist) { maxLineDist = d; p2 = i; }
        }
        if (p2 < 0 || maxLineDist < epsilon_ * epsilon_) return false;

        // p3: farthest from the plane (p0,p1,p2).
        Vector3 planeN = cross(points_[p1] - points_[p0], points_[p2] - points_[p0]);
        float planeLen = planeN.length();
        if (planeLen < 1e-12f) return false;
        planeN = planeN / planeLen;
        int p3 = -1;
        float maxPlaneDist = -1.0f;
        for (int i = 0; i < n; ++i) {
            float d = std::abs(dot(planeN, points_[i] - points_[p0]));
            if (d > maxPlaneDist) { maxPlaneDist = d; p3 = i; }
        }
        if (p3 < 0 || maxPlaneDist < epsilon_) return false;

        interior_ = (points_[p0] + points_[p1] + points_[p2] + points_[p3]) * 0.25f;

        // Four faces of the tetrahedron (orientation fixed by makeFace).
        makeFace(p0, p1, p2);
        makeFace(p0, p1, p3);
        makeFace(p0, p2, p3);
        makeFace(p1, p2, p3);

        // Assign every point to the first face it lies in front of.
        for (int i = 0; i < n; ++i) {
            if (i == p0 || i == p1 || i == p2 || i == p3) continue;
            int best = -1;
            float bestDist = epsilon_;
            for (int f = 0; f < static_cast<int>(faces_.size()); ++f) {
                float d = distanceToFace(faces_[f], points_[i]);
                if (d > bestDist) { bestDist = d; best = f; }
            }
            if (best >= 0) faces_[best].outside.push_back(i);
        }
        return true;
    }

    HullResult buildResult() const {
        HullResult result;
        std::vector<int> remap(points_.size(), -1);
        for (const Face& f : faces_) {
            if (!f.active) continue;
            std::array<unsigned, 3> tri{};
            for (int i = 0; i < 3; ++i) {
                int vi = f.v[i];
                if (remap[vi] < 0) {
                    remap[vi] = static_cast<int>(result.vertices.size());
                    result.vertices.push_back(points_[vi]);
                }
                tri[i] = static_cast<unsigned>(remap[vi]);
            }
            result.faceIndices.push_back(tri[0]);
            result.faceIndices.push_back(tri[1]);
            result.faceIndices.push_back(tri[2]);
        }
        return result;
    }
};

} // namespace nexus::utility::graphics
