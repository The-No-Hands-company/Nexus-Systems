// Foundation — SEEDED INVARIANT FUZZ HARNESS (gap-audit Phase F). Until now every
// kernel test was a hand-picked configuration; this randomizes over a broad space
// (with a FIXED seed, so it is deterministic and reproducible on the Null backend)
// and asserts the invariants that MUST hold — the tool that surfaces unknown-unknowns
// instead of waiting for a human to guess the degenerate case. It does NOT assert
// guarantees the kernel does not yet make (e.g. full mesh-boolean watertightness on
// coplanar seams — a tracked gap); those are COUNTED and reported, never asserted,
// so the harness stays green while still measuring the residual.

#include <nexus/geometry/AnalyticBRep.h>
#include <nexus/geometry/BRepBoolean.h>
#include <nexus/geometry/HalfEdgeMesh.h>
#include <nexus/geometry/Mesh.h>
#include <nexus/geometry/MeshBooleanRobust.h>
#include <nexus/geometry/MeshMassProperties.h>
#include <nexus/geometry/MeshTopologyValidation.h>
#include <nexus/render/Camera.h>

#include <gtest/gtest.h>

#include <cmath>
#include <optional>
#include <random>
#include <utility>

namespace nexus::geometry::testing {

using nexus::render::Vec3;
using nexus::render::Mat4;

namespace {
// A random proper-rotation × uniform-scale × translation — exactly what
// Body::transform accepts (rejects shear/non-uniform/reflection).
Mat4 randXform(std::mt19937& rng)
{
    std::uniform_real_distribution<float> u(-1.f, 1.f), ang(0.f, 6.2831853f), sc(0.6f, 1.8f),
        tr(-1.5f, 1.5f);
    Vec3 ax{u(rng), u(rng), u(rng)};
    float l = std::sqrt(ax.x * ax.x + ax.y * ax.y + ax.z * ax.z);
    if (l < 1e-4f) { ax = {0.f, 0.f, 1.f}; l = 1.f; }
    ax = {ax.x / l, ax.y / l, ax.z / l};
    const float a = ang(rng), c = std::cos(a), s = std::sin(a), t = 1.f - c, sca = sc(rng);
    const float R[3][3] = {
        {t * ax.x * ax.x + c,       t * ax.x * ax.y - s * ax.z, t * ax.x * ax.z + s * ax.y},
        {t * ax.x * ax.y + s * ax.z, t * ax.y * ax.y + c,       t * ax.y * ax.z - s * ax.x},
        {t * ax.x * ax.z - s * ax.y, t * ax.y * ax.z + s * ax.x, t * ax.z * ax.z + c      }};
    Mat4 m = Mat4::identity();
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j) m.m[i][j] = sca * R[i][j];
    m.m[0][3] = tr(rng); m.m[1][3] = tr(rng); m.m[2][3] = tr(rng);
    return m;
}

brep::Body randPrim(std::mt19937& rng, bool& faceted)
{
    std::uniform_int_distribution<int> kind(0, 4), seg(6, 8), lat(3, 4);
    std::uniform_real_distribution<float> dim(0.6f, 2.2f);
    const int k = kind(rng);
    faceted = (k >= 3);
    if (k <= 2) return brep::makeBox(dim(rng), dim(rng), dim(rng));  // boxes dominate (cheap)
    if (k == 3) return brep::makeFacetedCylinder(dim(rng), dim(rng) + 0.6f, seg(rng));
    return brep::makeFacetedSphere(dim(rng), lat(rng), seg(rng));
}
}  // namespace

// THE STRONGEST INVARIANT, FUZZED: booleanToBody always returns a WATERTIGHT solid
// or a CLEAN EMPTY body — never a corrupt or leaky one — and is deterministic, over
// randomized primitive pairs under random rotation/scale/translation.
TEST(KernelFuzz, BRepBooleanWatertightOrEmptyUnderRandomTransforms)
{
    std::mt19937 rng(0xC0FFEEu);
    int watertight = 0, empty = 0;
    for (int it = 0; it < 45; ++it) {
        bool fA = false, fB = false;
        brep::Body A = randPrim(rng, fA);
        brep::Body B = randPrim(rng, fB);
        // Faceted solids are only translated (rotation explodes their imprint cost).
        if (fB) { std::uniform_real_distribution<float> t(-1.5f, 1.5f); B.translate({t(rng), t(rng), t(rng)}); }
        else    { (void)B.transform(randXform(rng)); }
        if (!fA && (rng() & 1u)) (void)A.transform(randXform(rng));
        for (brep::BooleanOp op : {brep::BooleanOp::Union, brep::BooleanOp::Intersection,
                                   brep::BooleanOp::Difference}) {
            const brep::Body r = brep::booleanToBody(A, B, op);
            const auto ig = r.checkIntegrity();
            ASSERT_TRUE(ig.ok) << "it=" << it << " op=" << static_cast<int>(op) << ": " << ig.reason;
            if (r.faceCount() == 0) {
                ++empty;
            } else {
                ASSERT_TRUE(r.isClosed()) << "it=" << it << " op=" << static_cast<int>(op) << " leaky";
                ASSERT_EQ(ig.boundaryEdges, 0u) << "it=" << it << " op=" << static_cast<int>(op);
                ++watertight;
            }
            // Determinism: identical inputs → byte-identical serialization.
            ASSERT_EQ(r.serialize(), brep::booleanToBody(A, B, op).serialize())
                << "it=" << it << " op=" << static_cast<int>(op);
        }
    }
    EXPECT_GT(watertight, 50);  // the space is rich enough to exercise real results
    EXPECT_GT(empty, 0);
}

// Mesh boolean: assert only the guarantees the kernel makes today — every output is
// FINITE, DETERMINISTIC, and topologically well-formed enough to validate. Full
// watertightness is a tracked gap (GAP-M): leaks are COUNTED, not asserted.
TEST(KernelFuzz, MeshBooleanFiniteAndDeterministicUnderRandom)
{
    std::mt19937 rng(0xBADF00Du);
    std::uniform_real_distribution<float> md(0.6f, 2.f), mt(-1.5f, 1.5f);
    int leaks = 0, checked = 0;
    for (int it = 0; it < 60; ++it) {
        Mesh A = primitives::makeBox(md(rng), md(rng), md(rng));
        Mesh B = primitives::makeBox(md(rng), md(rng), md(rng));
        auto pb = B.attributes().positions();
        const float tx = mt(rng), ty = mt(rng), tz = mt(rng);
        for (auto& v : pb) { v.x += tx; v.y += ty; v.z += tz; }
        B.attributes().setPositions(std::move(pb));
        for (auto op : {BooleanOperationType::Union, BooleanOperationType::Intersection,
                        BooleanOperationType::Difference}) {
            const Mesh r = robustMeshBoolean(A, B, op);
            ++checked;
            for (const auto& p : r.attributes().positions()) {
                ASSERT_TRUE(std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z))
                    << "non-finite output it=" << it << " op=" << static_cast<int>(op);
            }
            const float vol = MeshMassProperties::compute(r).volume;
            ASSERT_TRUE(std::isfinite(vol)) << "non-finite volume it=" << it;
            const auto v = MeshTopologyValidation::validate(r);
            if (r.topology().faceCount() > 0 && v.boundaryLoops != 0) ++leaks;
            // Determinism: same inputs → same face/vertex counts.
            const Mesh r2 = robustMeshBoolean(A, B, op);
            ASSERT_EQ(r.topology().faceCount(), r2.topology().faceCount());
            ASSERT_EQ(r.attributes().positions().size(), r2.attributes().positions().size());
        }
    }
    // The residual mesh-boolean leak rate is a tracked metric, not a failure.
    RecordProperty("meshBooleanChecked", checked);
    RecordProperty("meshBooleanLeaks", leaks);
    EXPECT_LT(leaks, checked);  // sanity: not everything leaks
}

// Every half-edge Euler operator either succeeds cleanly or refuses — it must NEVER
// corrupt the half-edge topology. Fuzz random op sequences on seed meshes and assert
// checkIntegrity() stays clean after each step (the authoritative post-condition).
TEST(KernelFuzz, HalfEdgeOpsPreserveIntegrityUnderRandomSequences)
{
    std::mt19937 rng(0x5EED1234u);
    const Mesh seeds[] = {primitives::makeBox(2.f, 2.f, 2.f),
                          primitives::makeSphere(1.f, 3, 6),
                          primitives::makePlane(2.f, 2.f, 2, 2)};
    for (const Mesh& seed : seeds) {
        for (int trial = 0; trial < 10; ++trial) {
            auto hemOpt = HalfEdgeMesh::fromMesh(seed);
            ASSERT_TRUE(hemOpt.has_value());
            HalfEdgeMesh hem = std::move(*hemOpt);
            ASSERT_TRUE(hem.checkIntegrity().ok);
            for (int step = 0; step < 12; ++step) {
                const uint32_t ec = hem.edgeCount();
                if (ec == 0) break;
                const uint32_t he = rng() % ec;
                switch (rng() % 4u) {
                    case 0: (void)hem.flipEdge(he); break;
                    case 1: (void)hem.splitEdge(he); break;
                    case 2: (void)hem.insertEdgeVertex(he, 0.5f); break;
                    default: (void)hem.collapseEdge(he, Vec3{0.f, 0.f, 0.f}); break;
                }
                const auto ig = hem.checkIntegrity();
                ASSERT_TRUE(ig.ok) << "seed op corrupted HEM: " << ig.reason;
            }
        }
    }
}

}  // namespace nexus::geometry::testing
