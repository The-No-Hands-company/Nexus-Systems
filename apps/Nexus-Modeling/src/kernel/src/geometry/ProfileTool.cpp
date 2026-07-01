#include <nexus/geometry/ProfileTool.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <unordered_map>

namespace nexus::geometry {
using Vec3 = nexus::render::Vec3;

std::vector<Vec3> ProfileTool::extractProfile(const Mesh& m, const Vec3& planePoint, const Vec3& planeNormal) {
    std::vector<Vec3> profile;
    const auto& positions = m.attributes().positions();
    const auto& topology = m.topology();

    float nx = planeNormal.x, ny = planeNormal.y, nz = planeNormal.z;
    float nl = std::sqrt(nx * nx + ny * ny + nz * nz);
    if (nl < 1e-10f) return profile;
    nx /= nl; ny /= nl; nz /= nl;

    struct Edge { uint32_t a, b; };
    std::vector<Edge> intersections;

    for (size_t fi = 0; fi < topology.faceCount(); ++fi) {
        const auto& face = topology.face(fi);
        for (size_t vi = 0; vi < face.indices.size(); ++vi) {
            uint32_t a = face.indices[vi];
            uint32_t b = face.indices[(vi + 1) % face.indices.size()];
            if (a >= positions.size() || b >= positions.size()) continue;

            float da = (positions[a].x - planePoint.x) * nx +
                       (positions[a].y - planePoint.y) * ny +
                       (positions[a].z - planePoint.z) * nz;
            float db = (positions[b].x - planePoint.x) * nx +
                       (positions[b].y - planePoint.y) * ny +
                       (positions[b].z - planePoint.z) * nz;

            if (da * db < 0) {
                float t = std::abs(da) / (std::abs(da) + std::abs(db));
                Vec3 pt{
                    positions[a].x + (positions[b].x - positions[a].x) * t,
                    positions[a].y + (positions[b].y - positions[a].y) * t,
                    positions[a].z + (positions[b].z - positions[a].z) * t,
                };
                profile.push_back(pt);
                intersections.push_back({a, b});
            }
        }
    }

    if (profile.size() < 2) return profile;

    std::sort(profile.begin(), profile.end(), [](const Vec3& p, const Vec3& q) {
        if (std::abs(p.x - q.x) > 1e-6f) return p.x < q.x;
        if (std::abs(p.y - q.y) > 1e-6f) return p.y < q.y;
        return p.z < q.z;
    });

    return profile;
}

Mesh ProfileTool::profileToCurve(const std::vector<Vec3>& profile) {
    Mesh mesh;
    if (profile.size() < 2) return mesh;

    mesh.attributes().setPositions(profile);

    for (size_t i = 0; i + 1 < profile.size(); ++i) {
        Face f;
        f.indices = {static_cast<uint32_t>(i), static_cast<uint32_t>(i + 1), static_cast<uint32_t>(i + 1)};
        mesh.topology().addFace(f);
    }

    return mesh;
}

std::vector<std::vector<Vec3>> ProfileTool::extractAllProfiles(const Mesh& m, const Vec3& axis) {
    std::vector<std::vector<Vec3>> profiles;
    const auto& positions = m.attributes().positions();

    float ax = axis.x, ay = axis.y, az = axis.z;
    float al = std::sqrt(ax * ax + ay * ay + az * az);
    if (al < 1e-10f) return profiles;
    ax /= al; ay /= al; az /= al;

    float minProj = std::numeric_limits<float>::max();
    float maxProj = -std::numeric_limits<float>::max();
    for (const auto& p : positions) {
        float proj = p.x * ax + p.y * ay + p.z * az;
        minProj = std::min(minProj, proj);
        maxProj = std::max(maxProj, proj);
    }

    const uint32_t numSlices = 10;
    float step = (maxProj - minProj) / static_cast<float>(numSlices + 1);
    if (step < 1e-6f) return profiles;

    for (uint32_t i = 1; i <= numSlices; ++i) {
        float d = minProj + step * static_cast<float>(i);
        Vec3 planePoint{ax * d, ay * d, az * d};
        auto profile = extractProfile(m, planePoint, axis);
        if (!profile.empty()) {
            profiles.push_back(std::move(profile));
        }
    }

    return profiles;
}

} // namespace nexus::geometry
