#include <nexus/geometry/SectionTool.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <unordered_map>

namespace nexus::geometry {
using Vec3 = nexus::render::Vec3;

std::vector<Vec3> SectionTool::computeSection(const Mesh& m, const Vec3& planePoint, const Vec3& planeNormal) {
    std::vector<Vec3> section;
    const auto& positions = m.attributes().positions();
    const auto& topology = m.topology();

    float nx = planeNormal.x, ny = planeNormal.y, nz = planeNormal.z;
    float nl = std::sqrt(nx * nx + ny * ny + nz * nz);
    if (nl < 1e-10f) return section;
    nx /= nl; ny /= nl; nz /= nl;

    // Each cut edge is shared by two faces, so a per-face-edge scan finds every crossing
    // twice. Key each crossing by its undirected edge (min,max vertex pair) and keep it
    // once — otherwise the section carries every point doubled, producing zero-length
    // segments for any perimeter/area consumer downstream.
    std::unordered_map<uint64_t, Vec3> hitsByEdge;

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

            if (da * db <= 0 && (da != 0 || db != 0)) {
                float t = std::abs(da) / (std::abs(da) + std::abs(db) + 1e-10f);
                Vec3 pt{
                    positions[a].x + (positions[b].x - positions[a].x) * t,
                    positions[a].y + (positions[b].y - positions[a].y) * t,
                    positions[a].z + (positions[b].z - positions[a].z) * t,
                };
                const uint64_t key = (static_cast<uint64_t>(std::min(a, b)) << 32) | std::max(a, b);
                hitsByEdge.emplace(key, pt);  // first crossing per edge wins; the twin is identical
            }
        }
    }

    section.reserve(hitsByEdge.size());
    for (const auto& [key, pt] : hitsByEdge) {
        (void)key;
        section.push_back(pt);
    }

    if (section.size() > 1) {
        Vec3 centroid{};
        for (const auto& p : section) {
            centroid = Vec3{centroid.x + p.x, centroid.y + p.y, centroid.z + p.z};
        }
        float nf = static_cast<float>(section.size());
        centroid = Vec3{centroid.x / nf, centroid.y / nf, centroid.z / nf};

        Vec3 refDir{section[0].x - centroid.x, section[0].y - centroid.y, section[0].z - centroid.z};
        float rl = std::sqrt(refDir.x * refDir.x + refDir.y * refDir.y + refDir.z * refDir.z);
        if (rl > 1e-10f) {
            refDir = Vec3{refDir.x / rl, refDir.y / rl, refDir.z / rl};

            std::sort(section.begin(), section.end(), [&](const Vec3& a, const Vec3& b) {
                float dxA = a.x - centroid.x, dyA = a.y - centroid.y, dzA = a.z - centroid.z;
                float dxB = b.x - centroid.x, dyB = b.y - centroid.y, dzB = b.z - centroid.z;

                Vec3 crossA{refDir.y * dzA - refDir.z * dyA,
                            refDir.z * dxA - refDir.x * dzA,
                            refDir.x * dyA - refDir.y * dxA};
                Vec3 crossB{refDir.y * dzB - refDir.z * dyB,
                            refDir.z * dxB - refDir.x * dzB,
                            refDir.x * dyB - refDir.y * dxB};

                float signA = crossA.x * nx + crossA.y * ny + crossA.z * nz;
                float signB = crossB.x * nx + crossB.y * ny + crossB.z * nz;

                float angleA = std::atan2(signA,
                    dxA * refDir.x + dyA * refDir.y + dzA * refDir.z);
                float angleB = std::atan2(signB,
                    dxB * refDir.x + dyB * refDir.y + dzB * refDir.z);
                return angleA < angleB;
            });
        }
    }

    return section;
}

Mesh SectionTool::sectionMesh(const std::vector<Vec3>& section) {
    Mesh mesh;
    if (section.size() < 2) return mesh;

    mesh.attributes().setPositions(section);

    for (size_t i = 0; i + 1 < section.size(); ++i) {
        Face f;
        f.indices = {static_cast<uint32_t>(i), static_cast<uint32_t>(i + 1), static_cast<uint32_t>(i + 1)};
        mesh.topology().addFace(f);
    }

    if (section.size() >= 3) {
        Face f;
        f.indices = {static_cast<uint32_t>(section.size() - 1), 0u, 0u};
        mesh.topology().addFace(f);
    }

    return mesh;
}

std::vector<std::vector<Vec3>> SectionTool::multiSection(const Mesh& m,
                                                          const Vec3& origin, const Vec3& axis,
                                                          uint32_t count, float spacing) {
    std::vector<std::vector<Vec3>> sections;
    if (count == 0 || spacing < 1e-10f) return sections;

    float ax = axis.x, ay = axis.y, az = axis.z;
    float al = std::sqrt(ax * ax + ay * ay + az * az);
    if (al < 1e-10f) return sections;
    ax /= al; ay /= al; az /= al;

    for (uint32_t i = 0; i < count; ++i) {
        float offset = static_cast<float>(i) * spacing;
        Vec3 planePoint{
            origin.x + ax * offset,
            origin.y + ay * offset,
            origin.z + az * offset,
        };

        auto section = computeSection(m, planePoint, axis);
        if (!section.empty()) {
            sections.push_back(std::move(section));
        }
    }

    return sections;
}

} // namespace nexus::geometry
