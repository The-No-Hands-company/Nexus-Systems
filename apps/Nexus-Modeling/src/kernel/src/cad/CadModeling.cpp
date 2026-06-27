#include <nexus/cad/CadModeling.h>
#include <nexus/cad/CadSurfacing.h>
#include <nexus/cad/CadTolerantModeling.h>
#include <nexus/cad/CadMBD.h>
#include <nexus/cad/CadIncrementalSolver.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>
#include <unordered_map>
#include <unordered_set>

namespace nexus::cad {

// ── Surfacing ─────────────────────────────────────────────────────
geometry::NurbsSurface CadSurfacing::createBlend(const geometry::NurbsSurface& a, float ua, float va,
                                                   const geometry::NurbsSurface& b, float ub, float vb,
                                                   const BlendSpec& spec) noexcept {
    (void)ua; (void)va; (void)ub; (void)vb;
    int32_t degU = std::max(a.degreeU(), b.degreeU());
    int32_t degV = std::max(a.degreeV(), b.degreeV());
    int32_t nU = 4;
    int32_t nV = 4;

    std::vector<float> kU, kV;
    for (int32_t i = 0; i <= degU; ++i) kU.push_back(0.0f);
    for (int32_t i = 0; i <= degU; ++i) kU.push_back(1.0f);
    for (int32_t i = 0; i <= degV; ++i) kV.push_back(0.0f);
    for (int32_t i = 0; i <= degV; ++i) kV.push_back(1.0f);

    std::vector<Vec3> ctl;
    ctl.reserve(static_cast<size_t>(nU * nV));

    float alpha = 1.0f / (1.0f + spec.tangentScale);
    const auto& ctlA = a.controlPoints();
    const auto& ctlB = b.controlPoints();
    int32_t nUA = a.controlPointCountU();
    int32_t nVA = a.controlPointCountV();

    for (int32_t i = 0; i < nU; ++i) {
        for (int32_t j = 0; j < nV; ++j) {
            Vec3 pt{};
            int32_t ia = (i * nUA) / nU;
            int32_t ja = (j * nVA) / nV;
            int32_t ib = (i * b.controlPointCountU()) / nU;
            int32_t jb = (j * b.controlPointCountV()) / nV;

            size_t idxA = static_cast<size_t>(ia * nVA + ja);
            size_t idxB = static_cast<size_t>(ib * b.controlPointCountV() + jb);

            if (idxA < ctlA.size() && idxB < ctlB.size()) {
                pt = Vec3{
                    ctlA[idxA].x * alpha + ctlB[idxB].x * (1.0f - alpha),
                    ctlA[idxA].y * alpha + ctlB[idxB].y * (1.0f - alpha),
                    ctlA[idxA].z * alpha + ctlB[idxB].z * (1.0f - alpha),
                };
            }
            ctl.push_back(pt);
        }
    }

    return geometry::NurbsSurface(degU, degV, std::move(kU), std::move(kV), std::move(ctl), nU, nV);
}

bool CadSurfacing::validateClassA(const geometry::NurbsSurface& s) noexcept {
    if (s.degreeU() < 3 || s.degreeV() < 3) return false;
    const auto& ctl = s.controlPoints();
    if (ctl.size() < 16) return false;

    for (size_t i = 0; i + 1 < ctl.size(); ++i) {
        float dx = ctl[i + 1].x - ctl[i].x;
        float dy = ctl[i + 1].y - ctl[i].y;
        float dz = ctl[i + 1].z - ctl[i].z;
        if (std::sqrt(dx * dx + dy * dy + dz * dz) > 100.0f) return false;
    }
    return true;
}

CurvatureComb CadSurfacing::generateCurvatureComb(const geometry::NurbsSurface& s, float radius, bool isoU) noexcept {
    CurvatureComb comb;
    const int samples = std::max(2, static_cast<int>(radius * 8.0f));
    auto [uMin, uMax] = s.domainU();
    auto [vMin, vMax] = s.domainV();

    comb.points.reserve(samples);
    comb.normals.reserve(samples);
    comb.curvature.reserve(samples);

    float uMid = (uMin + uMax) * 0.5f;
    float vMid = (vMin + vMax) * 0.5f;

    for (int i = 0; i < samples; ++i) {
        float t = static_cast<float>(i) / static_cast<float>(samples - 1);
        float u = isoU ? uMin + t * (uMax - uMin) : uMid;
        float v = isoU ? vMid : vMin + t * (vMax - vMin);

        Vec3 pt = s.evaluate(u, v);
        Vec3 du = s.derivativeU(u, v);
        Vec3 dv = s.derivativeV(u, v);

        Vec3 n{du.y * dv.z - du.z * dv.y,
               du.z * dv.x - du.x * dv.z,
               du.x * dv.y - du.y * dv.x};
        float nLen = std::sqrt(n.x * n.x + n.y * n.y + n.z * n.z);
        if (nLen > 1e-10f) {
            n = Vec3{n.x / nLen, n.y / nLen, n.z / nLen};
        }

        float duLen = std::sqrt(du.x * du.x + du.y * du.y + du.z * du.z);
        float dvLen = std::sqrt(dv.x * dv.x + dv.y * dv.y + dv.z * dv.z);

        comb.points.push_back(pt);
        comb.normals.push_back(n);
        comb.curvature.push_back(0.1f + duLen * dvLen * 0.5f);
    }

    return comb;
}

geometry::Mesh CadSurfacing::generateZebraStripes(const geometry::NurbsSurface& s, uint32_t count) noexcept {
    geometry::Mesh mesh;
    std::vector<Vec3> positions;
    auto [uMin, uMax] = s.domainU();
    auto [vMin, vMax] = s.domainV();

    float spacing = 1.0f / static_cast<float>(count);
    float uSpan = uMax - uMin;

    for (uint32_t i = 0; i < count; ++i) {
        float t0 = static_cast<float>(i) * spacing;
        float t1 = static_cast<float>(i + 1) * spacing;

        Vec3 p0 = s.evaluate(uMin + t0 * uSpan, (vMin + vMax) * 0.5f);
        Vec3 p1 = s.evaluate(uMin + t1 * uSpan, (vMin + vMax) * 0.5f);
        Vec3 p2 = s.evaluate(uMin + t0 * uSpan, vMin);
        Vec3 p3 = s.evaluate(uMin + t1 * uSpan, vMin);

        uint32_t base = static_cast<uint32_t>(positions.size());
        positions.push_back(p0);
        positions.push_back(p1);
        positions.push_back(p2);
        positions.push_back(p3);

        geometry::Face f1, f2;
        f1.indices = {base, base + 1, base + 2};
        f2.indices = {base + 1, base + 3, base + 2};
        mesh.topology().addFace(f1);
        mesh.topology().addFace(f2);
    }

    mesh.attributes().setPositions(positions);
    return mesh;
}

// ── Tolerant ──────────────────────────────────────────────────────
geometry::Mesh CadTolerantModeling::fuzzyUnion(const geometry::Mesh& a, const geometry::Mesh& b,
                                                 const TolerantBooleanOptions& opts) noexcept {
    geometry::Mesh result = a;
    auto posA = result.attributes().positions();
    auto posB = b.attributes().positions();

    float eps = opts.gapTolerance;
    std::vector<Vec3> merged = posA;
    uint32_t offsetA = static_cast<uint32_t>(merged.size());

    for (const auto& pb : posB) {
        bool found = false;
        for (uint32_t j = 0; j < offsetA; ++j) {
            float dx = pb.x - merged[j].x;
            float dy = pb.y - merged[j].y;
            float dz = pb.z - merged[j].z;
            if (std::sqrt(dx * dx + dy * dy + dz * dz) < eps) {
                found = true;
                break;
            }
        }
        if (!found) {
            merged.push_back(pb);
        }
    }

    result.attributes().setPositions(merged);

    for (size_t fi = 0; fi < a.topology().faceCount(); ++fi) {
        result.topology().addFace(a.topology().face(fi));
    }
    for (size_t fi = 0; fi < b.topology().faceCount(); ++fi) {
        geometry::Face f = b.topology().face(fi);
        for (auto& idx : f.indices) idx += offsetA;
        result.topology().addFace(f);
    }

    return result;
}

geometry::Mesh CadTolerantModeling::healGaps(const geometry::Mesh& mesh, float maxGap) noexcept {
    geometry::Mesh result = mesh;
    auto positions = result.attributes().positions();
    const auto& topo = result.topology();

    std::unordered_map<uint64_t, uint32_t> edgeCount;
    for (size_t fi = 0; fi < topo.faceCount(); ++fi) {
        const auto& face = topo.face(fi);
        for (size_t vi = 0; vi < face.indices.size(); ++vi) {
            uint32_t a = face.indices[vi];
            uint32_t b = face.indices[(vi + 1) % face.indices.size()];
            uint64_t key = (static_cast<uint64_t>(std::min(a, b)) << 32) | std::max(a, b);
            edgeCount[key]++;
        }
    }

    for (const auto& [key, count] : edgeCount) {
        if (count == 1) {
            uint32_t a = static_cast<uint32_t>(key >> 32);
            uint32_t b = static_cast<uint32_t>(key & 0xFFFFFFFF);
            if (a < positions.size() && b < positions.size()) {
                float dx = positions[a].x - positions[b].x;
                float dy = positions[a].y - positions[b].y;
                float dz = positions[a].z - positions[b].z;
                float dist = std::sqrt(dx * dx + dy * dy + dz * dz);
                if (dist < maxGap && dist > 1e-10f) {
                    positions[a] = Vec3{
                        (positions[a].x + positions[b].x) * 0.5f,
                        (positions[a].y + positions[b].y) * 0.5f,
                        (positions[a].z + positions[b].z) * 0.5f};
                    positions[b] = positions[a];
                }
            }
        }
    }

    result.attributes().setPositions(positions);
    return result;
}

geometry::Mesh CadTolerantModeling::removeSlivers(const geometry::Mesh& mesh, float threshold) noexcept {
    geometry::Mesh result;
    auto positions = mesh.attributes().positions();
    result.attributes().setPositions(positions);

    const auto& topo = mesh.topology();
    for (size_t fi = 0; fi < topo.faceCount(); ++fi) {
        const auto& face = topo.face(fi);
        if (face.indices.size() < 3) continue;

        uint32_t i0 = face.indices[0];
        uint32_t i1 = face.indices[1];
        uint32_t i2 = face.indices[2];
        if (i0 >= positions.size() || i1 >= positions.size() || i2 >= positions.size()) continue;

        float ax = positions[i1].x - positions[i0].x;
        float ay = positions[i1].y - positions[i0].y;
        float az = positions[i1].z - positions[i0].z;
        float bx = positions[i2].x - positions[i0].x;
        float by = positions[i2].y - positions[i0].y;
        float bz = positions[i2].z - positions[i0].z;

        float cx = ay * bz - az * by;
        float cy = az * bx - ax * bz;
        float cz = ax * by - ay * bx;
        float area = 0.5f * std::sqrt(cx * cx + cy * cy + cz * cz);

        if (area >= threshold) {
            result.topology().addFace(face);
        }
    }

    return result;
}

geometry::Mesh CadTolerantModeling::repairImport(const geometry::Mesh& m) noexcept {
    auto repaired = healGaps(m, 0.1f);
    return removeSlivers(repaired, 0.001f);
}

// ── MBD ───────────────────────────────────────────────────────────
geometry::Mesh CadMBD::exportAnnotationsMesh(const std::vector<PMIAnnotation>& annotations) noexcept {
    geometry::Mesh mesh;
    std::vector<Vec3> positions;

    for (const auto& ann : annotations) {
        float x = ann.position.x;
        float y = ann.position.y;
        float z = ann.position.z;
        uint32_t base = static_cast<uint32_t>(positions.size());

        switch (ann.type) {
            case PMIAnnotation::Dimension: {
                positions.push_back(Vec3{x, y, z});
                positions.push_back(Vec3{x + 10, y, z});
                positions.push_back(Vec3{x, y + 10, z});
                geometry::Face f;
                f.indices = {base, base + 1, base + 2};
                mesh.topology().addFace(f);
                break;
            }
            case PMIAnnotation::GDnT: {
                float h = 5.0f;
                positions.push_back(Vec3{x - h, y - h, z});
                positions.push_back(Vec3{x + h, y - h, z});
                positions.push_back(Vec3{x + h, y + h, z});
                positions.push_back(Vec3{x - h, y + h, z});
                geometry::Face f1, f2;
                f1.indices = {base, base + 1, base + 2};
                f2.indices = {base, base + 2, base + 3};
                mesh.topology().addFace(f1);
                mesh.topology().addFace(f2);
                break;
            }
            case PMIAnnotation::Datum: {
                float s = 4.0f;
                positions.push_back(Vec3{x, y, z});
                positions.push_back(Vec3{x + s, y - s, z});
                positions.push_back(Vec3{x + s, y + s, z});
                geometry::Face f;
                f.indices = {base, base + 1, base + 2};
                mesh.topology().addFace(f);
                break;
            }
            default: {
                positions.push_back(Vec3{x - 3, y - 3, z});
                positions.push_back(Vec3{x + 3, y - 3, z});
                positions.push_back(Vec3{x + 3, y + 3, z});
                positions.push_back(Vec3{x - 3, y + 3, z});
                geometry::Face f1, f2;
                f1.indices = {base, base + 1, base + 2};
                f2.indices = {base, base + 2, base + 3};
                mesh.topology().addFace(f1);
                mesh.topology().addFace(f2);
                break;
            }
        }
    }

    mesh.attributes().setPositions(positions);
    return mesh;
}

// ── IncrementalSolver ─────────────────────────────────────────────
bool CadIncrementalSolver::solve(parametric::ConstraintGraph& graph,
                                  const SolverCache& cache,
                                  const std::vector<parametric::ParametricConstraintId>& changed) noexcept {
    if (!cache.valid || changed.empty()) return cache.valid;

    bool anyApplied = false;
    for (auto cid : changed) {
        for (const auto& dc : graph.distanceConstraints()) {
            if (dc.id != cid) continue;
            const auto* ptA = graph.point(dc.entityA);
            const auto* ptB = graph.point(dc.entityB);
            if (!ptA || !ptB) continue;

            double dx = ptB->x - ptA->x;
            double dy = ptB->y - ptA->y;
            double dz = ptB->z - ptA->z;
            double dist = std::sqrt(dx * dx + dy * dy + dz * dz);
            if (dist < 1e-12) continue;

            double scale = dc.targetDistance / dist;
            parametric::ParametricPoint3 newPt{
                ptA->x + dx * scale,
                ptA->y + dy * scale,
                ptA->z + dz * scale,
            };
            (void)graph.setPoint(dc.entityB, newPt);
            anyApplied = true;
        }
        for (const auto& cc : graph.coincidentConstraints()) {
            if (cc.id != cid) continue;
            const auto* ptA = graph.point(cc.entityA);
            if (!ptA) continue;
            (void)graph.setPoint(cc.entityB, *ptA);
            anyApplied = true;
        }
    }

    return anyApplied || cache.valid;
}

SolverCache CadIncrementalSolver::buildCache(const parametric::ConstraintGraph& g) noexcept {
    SolverCache c;
    c.valid = true;
    for (const auto& e : g.entities()) {
        c.lastPositions.push_back(e.point.x);
        c.lastPositions.push_back(e.point.y);
        c.lastPositions.push_back(e.point.z);
    }
    for (const auto& dc : g.distanceConstraints())
        c.changedConstraints.push_back(dc.id);
    for (const auto& cc : g.coincidentConstraints())
        c.changedConstraints.push_back(cc.id);
    return c;
}

void CadIncrementalSolver::invalidate(SolverCache& c, parametric::ParametricEntityId) noexcept {
    c.valid = false;
}

}
