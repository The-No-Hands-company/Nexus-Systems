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
#include <nexus/geometry/BooleanOperation.h>
#include <nexus/geometry/HalfEdgeMesh.h>
#include <nexus/geometry/Mesh.h>
#include <nexus/geometry/MeshBooleanRobust.h>
#include <nexus/geometry/MeshMassProperties.h>
#include <nexus/geometry/MeshDecimator.h>
#include <nexus/geometry/MeshDisplace.h>
#include <nexus/geometry/MeshLaplacian.h>
#include <nexus/geometry/MeshRepair.h>
#include <nexus/geometry/RemeshOperation.h>
#include <nexus/geometry/MeshSimplify.h>
#include <nexus/geometry/MeshThicken.h>
#include <nexus/geometry/MeshTopologyValidation.h>
#include <nexus/geometry/MeshVertexWeld.h>
#include <nexus/geometry/MeshSurfaceNets.h>  // included WITH MeshVoxelize.h below: compile-time regression guard for the VoxelGrid double-def
#include <nexus/geometry/MeshVoxelize.h>
#include <nexus/geometry/SubdivisionSurface.h>
#include <nexus/geometry/Tolerance.h>
#include <nexus/render/Camera.h>

#include <gtest/gtest.h>

#include <cmath>
#include <limits>
#include <optional>
#include <random>
#include <utility>
#include <vector>

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

// THE ANALYTIC SURFACE SCOPE, FUZZED — and fuzzed on VOLUME as well as topology.
//
// The B-rep fuzz above builds its solids from makeBox, makeFacetedCylinder and
// makeFacetedSphere: all-planar bodies. Every curved capability this kernel has gained —
// circle seams on spheres and cylinders, cone sections, parallel cylinder rulings, a sphere
// meeting a cylinder on its axis — was therefore never fuzzed at all, because the generator
// could not produce a curved surface to intersect.
//
// It also checks only that the result is well-formed. Everything this kernel validates is
// topological, so a result can be watertight, integral, closed and the wrong SIZE, and
// several defects have been exactly that. So the identities are fuzzed too: union plus
// intersection equals both operands, difference plus intersection equals the first, each
// measured against the MUTUALLY IMPRINTED operands (the result carries the seam vertices
// and the pristine operands do not, so comparing against those measures tessellation
// density rather than volume).
//
// WHAT IT FOUND, on its first serious run: 22 volume-identity violations in 2000 random
// pairs, all of them sphere against box, all at subdivision 2 and above and NONE at
// subdivision 0, magnitudes from 1.1e-05 to 8.8e-04. Zero leaky results and zero
// non-deterministic ones across the same 6000 booleans, so the topology is sound and it is
// specifically the refinement that disagrees.
//
// Narrowed so far, all measured: the output's edges match the operands' edge for edge —
// same curve kind and the same refined midpoint, to zero — and the face counts match. So
// the divergence is inside face tessellation rather than in the boundary. Root cause is NOT
// yet established; two candidate explanations were tested and rejected (arcs lost in the
// sew: counts match exactly; arcs restored on the complement range: no complement arcs in
// any output). It is COUNTED here with a bound rather than asserted away, so it cannot
// drift and cannot be forgotten.
TEST(KernelFuzz, AnalyticSurfaceBooleansWatertightDeterministicAndVolumeConserving)
{
    auto analyticPrim = [](std::mt19937& rng) {
        std::uniform_int_distribution<int> kind(0, 3);
        std::uniform_real_distribution<float> dim(0.5f, 1.8f);
        switch (kind(rng)) {
            case 0: { const float w = dim(rng), h = dim(rng), d = dim(rng);
                      return brep::makeBox(w, h, d); }
            case 1: { const float r = dim(rng), h = dim(rng) + 1.f;
                      return brep::makeCylinder(r, h, 16); }
            case 2: { const float r = dim(rng);
                      return brep::makeSphere(r, 8, 12); }
            default: { const float r = dim(rng), h = dim(rng) + 1.f;
                       return brep::makeCone(r, h, 16); }
        }
    };
    auto volumeOf = [](const brep::Body& b, uint32_t sub) {
        return MeshMassProperties::compute(b.toMesh(sub)).volume;
    };

    std::mt19937 rng(0xA17E51u);
    std::uniform_real_distribution<double> tr(-1.0, 1.0);
    int watertight = 0, empty = 0, identityViolations = 0;
    double worstIdentity = 0.0;

    for (int it = 0; it < 120; ++it) {
        brep::Body A = analyticPrim(rng);
        brep::Body B = analyticPrim(rng);
        B.translate({tr(rng), tr(rng), tr(rng)});
        // Occasionally give A a quarter turn about a principal axis, so axes meet at right
        // angles as well as in parallel — which is what separates the pairs this kernel can
        // answer from the quartics it declines.
        if (rng() % 4u == 0u) {
            Mat4 m = Mat4::identity();
            switch (rng() % 3u) {
                case 0: m.m[1][1] = 0; m.m[1][2] = -1; m.m[2][1] = 1; m.m[2][2] = 0; break;
                case 1: m.m[0][0] = 0; m.m[0][2] = 1; m.m[2][0] = -1; m.m[2][2] = 0; break;
                default: m.m[0][0] = 0; m.m[0][1] = -1; m.m[1][0] = 1; m.m[1][1] = 0; break;
            }
            (void)A.transform(m);
        }

        brep::Body Ai = A, Bi = B;
        const bool imprinted = brep::imprintMutually(Ai, Bi);

        brep::Body results[3];
        int idx = 0;
        for (brep::BooleanOp op : {brep::BooleanOp::Union, brep::BooleanOp::Intersection,
                                   brep::BooleanOp::Difference}) {
            const brep::Body r = brep::booleanToBody(A, B, op);
            const auto ig = r.checkIntegrity();
            ASSERT_TRUE(ig.ok) << "it=" << it << " op=" << static_cast<int>(op) << ": " << ig.reason;
            if (r.faceCount() == 0) {
                ++empty;
            } else {
                ASSERT_TRUE(r.isClosed()) << "it=" << it << " op=" << static_cast<int>(op)
                                          << ": an analytic boolean returned a leaky solid";
                ASSERT_EQ(ig.boundaryEdges, 0u) << "it=" << it << " op=" << static_cast<int>(op);
                ++watertight;
                // Determinism sampled rather than checked every time: it doubles the cost of
                // the whole test and has never once varied.
                if (it % 4 == 0)
                    ASSERT_EQ(r.serialize(), brep::booleanToBody(A, B, op).serialize())
                        << "it=" << it << " op=" << static_cast<int>(op);
            }
            results[idx++] = r;
        }

        if (!imprinted) continue;
        const brep::Body& U = results[0];
        const brep::Body& I = results[1];
        const brep::Body& D = results[2];
        for (const uint32_t sub : {0u, 2u}) {
            if (U.faceCount() && I.faceCount()) {
                const double lhs = volumeOf(U, sub) + volumeOf(I, sub);
                const double rhs = volumeOf(Ai, sub) + volumeOf(Bi, sub);
                if (std::abs(rhs) > 1e-9 && std::abs(lhs - rhs) > 1e-5 * std::abs(rhs)) {
                    ++identityViolations;
                    worstIdentity = std::max(worstIdentity, std::abs((lhs - rhs) / rhs));
                }
            }
            if (D.faceCount() && I.faceCount()) {
                const double lhs = volumeOf(D, sub) + volumeOf(I, sub);
                const double rhs = volumeOf(Ai, sub);
                if (std::abs(rhs) > 1e-9 && std::abs(lhs - rhs) > 1e-5 * std::abs(rhs)) {
                    ++identityViolations;
                    worstIdentity = std::max(worstIdentity, std::abs((lhs - rhs) / rhs));
                }
            }
        }
    }

    // The space has to be rich enough to have exercised anything at all.
    EXPECT_GT(watertight, 100) << "the generator produced almost no real results";
    EXPECT_GT(empty, 0) << "nothing was declined — the out-of-scope pairs are not being hit";

    // This used to allow up to 4 violations at up to 2e-3, bounded on both sides so the
    // "known, unexplained refinement disagreement" could neither grow nor be quietly fixed.
    // It has since been explained and fixed: the fan apex was chosen by an exact comparison
    // of coordinates that a seam makes equal, so the last bit decided how a curved patch was
    // cut (see BRepFanApexStability). Over 2000 configurations of this same generator the
    // count went 14 → 1 and the worst went 8.8e-04 → 1.3e-06, so the allowance is spent and
    // the gate is now zero across this sweep.
    EXPECT_EQ(identityViolations, 0)
        << "volume-identity violations rose to " << identityViolations
        << " (worst " << worstIdentity << ") — a boolean is returning the wrong size";
}

// Mesh boolean: assert only the guarantees the kernel makes today — every output is
// FINITE, DETERMINISTIC, and topologically well-formed enough to validate. Full
// watertightness is a tracked gap (GAP-M): leaks are COUNTED, not asserted.
// EVERY GENERATOR IN THIS FILE PRODUCES A PRIMITIVE, so until now the fuzzer had never seen a
// body with a HOLE in it — and that is where this kernel's recent defects have lived. A hole in
// a face arises the moment a Boolean result is fed back as an operand, which is the ordinary way
// anyone models: drill, then cut, then boss. Four separate defects in that territory were found
// by hand rather than by this harness, and each one it would have caught immediately:
//
//   * a face split gave its holes to whichever piece inherited the old face record, not to the
//     piece that contains them;
//   * a cut whose line crossed a hole was applied as though the hole were not there;
//   * the arc-crossing solver skipped a line lying IN an arc's plane, which is the ordinary case
//     for a hole in a planar face;
//   * a cut line was required to cross a boundary exactly twice, which is false of any face that
//     has been cut before.
//
// So the chain itself is fuzzed here: a random Boolean result becomes an operand for the next
// one, to whatever depth still yields a solid. The assertion is the kernel's strongest —
// watertight or cleanly empty, never leaky — plus determinism, which a chain is a good test of
// because every step's input is the previous step's output.
//
// The decline rate is RECORDED rather than asserted tightly: a chain reaching a configuration the
// Boolean does not yet describe returns empty by contract, and that is not a failure. The floor
// exists so the chain cannot quietly stop producing solids altogether.
TEST(KernelFuzz, ChainedBooleansStayWatertightOrEmptyAndDeterministic)
{
    auto prim = [](std::mt19937& rng) {
        std::uniform_int_distribution<int> kind(0, 3);
        std::uniform_real_distribution<float> dim(0.5f, 1.8f);
        switch (kind(rng)) {
            case 0: { const float w = dim(rng), h = dim(rng), d = dim(rng);
                      return brep::makeBox(w, h, d); }
            case 1: { const float r = dim(rng) * 0.6f, h = dim(rng) + 2.f;
                      return brep::makeCylinder(r, h, 16); }
            case 2: { const float r = dim(rng);
                      return brep::makeSphere(r, 8, 12); }
            default: { const float r = dim(rng), h = dim(rng) + 1.f;
                       return brep::makeCone(r, h, 16); }
        }
    };
    auto sound = [](const brep::Body& b) {
        return b.faceCount() > 0 && b.isClosed() && b.checkIntegrity().ok && b.checkGeometry().ok;
    };
    auto holedFaces = [](const brep::Body& b) {
        int n = 0;
        for (uint32_t f = 0; f < static_cast<uint32_t>(b.faceCount()); ++f)
            if (b.face(f).alive && !b.face(f).innerLoops.empty()) ++n;
        return n;
    };

    // A HOLE NEEDS A DRILL, AND RANDOM PLACEMENT ALMOST NEVER DRILLS. The first version of this
    // test placed a random primitive at a random offset and asserted, as a guard on its own
    // reach, that some chain eventually produced a face with a hole. It never did — not once in
    // 60 chains. A hole appears only when a tool passes CLEANLY THROUGH a face without reaching
    // its edges, and a uniformly-placed primitive of comparable size to the body does that
    // essentially never. The guard was right and the generator was blind, which is the same
    // lesson this file learned when it turned out never to have emitted a curved surface.
    //
    // So drilling is generated deliberately: a slender tool, longer than the body and narrow
    // against it, aimed near the middle of its bounding box. That is not stacking the deck — it
    // is the difference between a generator that CAN reach the territory and one that cannot.
    auto boundsOf = [](const brep::Body& b, Vec3d& lo, Vec3d& hi) {
        lo = Vec3d{1e30, 1e30, 1e30};
        hi = Vec3d{-1e30, -1e30, -1e30};
        for (uint32_t v = 0; v < static_cast<uint32_t>(b.vertexCount()); ++v) {
            if (!b.vertex(v).alive) continue;
            const Vec3d& p = b.vertex(v).point;
            lo = Vec3d{std::min(lo.x, p.x), std::min(lo.y, p.y), std::min(lo.z, p.z)};
            hi = Vec3d{std::max(hi.x, p.x), std::max(hi.y, p.y), std::max(hi.z, p.z)};
        }
    };

    std::mt19937 rng(0x5EED11u);
    std::uniform_real_distribution<double> tr(-1.2, 1.2);
    int chains = 0, steps = 0, solids = 0, empties = 0, sawHole = 0, drills = 0;
    int coaxialDrills = 0;
    size_t byReason[5] = {0, 0, 0, 0, 0};

    for (int it = 0; it < 60; ++it) {
        brep::Body cur = prim(rng);
        ASSERT_TRUE(sound(cur)) << "it=" << it << ": the seed primitive is not a solid";
        ++chains;

        for (int depth = 0; depth < 3; ++depth) {
            brep::Body tool;
            const bool drill = (rng() % 2u == 0u);
            if (drill) {
                // slender and long, aimed near the middle of the body: this is what bores a hole
                Vec3d lo{}, hi{};
                boundsOf(cur, lo, hi);
                const double sx = hi.x - lo.x, sy = hi.y - lo.y, sz = hi.z - lo.z;
                const double span = std::max(sx, std::max(sy, sz));
                if (!(span > 1e-6)) break;
                std::uniform_real_distribution<double> frac(0.10, 0.30), jit(-0.15, 0.15);
                const double rad = frac(rng) * span;
                tool = (rng() % 2u == 0u)
                           ? brep::makeCylinder(static_cast<float>(rad),
                                                static_cast<float>(span * 3.0), 16)
                           : brep::makeBox(static_cast<float>(rad * 2.0),
                                           static_cast<float>(rad * 2.0),
                                           static_cast<float>(span * 3.0));
                const Vec3d c{0.5 * (lo.x + hi.x), 0.5 * (lo.y + hi.y), 0.5 * (lo.z + hi.z)};
                // Some drills are placed EXACTLY on the axis. Jittering every one of them
                // — which is what this generator did at first — puts the tool off-axis
                // with probability 1, and the axially-aligned configurations are precisely
                // the ones with a closed-form seam (a cone or a sphere met by a coaxial
                // bore). The cone∩cylinder, cone∩sphere and cone∩cone pairs were missing
                // from intersectSurfaces entirely, and this test could not see it: adding
                // them moved the fully-jittered corpus by exactly zero steps while a
                // coaxial probe went from 23.9% empty to 0%. A generator that always
                // perturbs cannot reach the aligned case.
                const bool coaxial = (rng() % 4u == 0u);
                const double jx = coaxial ? 0.0 : jit(rng) * span;
                const double jy = coaxial ? 0.0 : jit(rng) * span;
                tool.translate({c.x + jx, c.y + jy, c.z});
                ++drills;
                if (coaxial) ++coaxialDrills;
            } else {
                tool = prim(rng);
                tool.translate({tr(rng), tr(rng), tr(rng)});
            }
            const brep::BooleanOp op = (rng() % 3u == 0u)   ? brep::BooleanOp::Union
                                     : (rng() % 2u == 0u)   ? brep::BooleanOp::Intersection
                                                            : brep::BooleanOp::Difference;
            brep::BooleanDiagnostic diag = brep::BooleanDiagnostic::Ok;
            const brep::Body r = brep::booleanToBody(cur, tool, op, Tolerance{}, &diag);
            ++steps;
            ++byReason[static_cast<size_t>(diag)];

            // THE contract, at every step of every chain.
            const auto ig = r.checkIntegrity();
            ASSERT_TRUE(ig.ok) << "it=" << it << " depth=" << depth << ": " << ig.reason;
            // The diagnostic must never contradict the value it describes.
            ASSERT_EQ(diag == brep::BooleanDiagnostic::Ok, r.faceCount() > 0u)
                << "it=" << it << " depth=" << depth
                << " diagnostic=" << static_cast<int>(diag) << " faces=" << r.faceCount();
            if (r.faceCount() == 0u) { ++empties; break; }   // declined: the chain ends here
            ASSERT_TRUE(r.isClosed())
                << "it=" << it << " depth=" << depth << ": non-empty but not closed";
            ASSERT_TRUE(r.checkGeometry().ok)
                << "it=" << it << " depth=" << depth << ": " << r.checkGeometry().reason;
            ++solids;
            if (holedFaces(r) > 0) ++sawHole;

            // Determinism: the same two operands must give the same answer, every time.
            const brep::Body again = brep::booleanToBody(cur, tool, op);
            ASSERT_EQ(again.faceCount(), r.faceCount())
                << "it=" << it << " depth=" << depth << ": not deterministic";
            ASSERT_EQ(again.vertexCount(), r.vertexCount()) << "it=" << it << " depth=" << depth;
            for (uint32_t v = 0; v < static_cast<uint32_t>(r.vertexCount()); ++v) {
                ASSERT_EQ(again.vertex(v).point.x, r.vertex(v).point.x) << "v=" << v;
                ASSERT_EQ(again.vertex(v).point.y, r.vertex(v).point.y) << "v=" << v;
                ASSERT_EQ(again.vertex(v).point.z, r.vertex(v).point.z) << "v=" << v;
            }
            cur = r;
        }
    }

    EXPECT_EQ(chains, 60);
    EXPECT_GT(steps, 60) << "the sweep did not run";
    // Recorded, not tightly asserted — a chain that reaches a configuration the Boolean does not
    // describe returns empty by contract. The floor is here so chains cannot stop producing
    // solids altogether without anyone noticing.
    EXPECT_GT(solids, steps / 4)
        << solids << " solids from " << steps << " chained steps (" << empties
        << " declined) — chaining has regressed";
    EXPECT_GT(drills, 0) << "the generator never attempted a drill";
    EXPECT_GT(coaxialDrills, 0)
        << "every drill was jittered off the axis, so the closed-form seams (cone or sphere "
           "met by a coaxial bore) are never exercised — check the generator";
    EXPECT_GT(sawHole, 0)
        << "no chain ever produced a face with a hole, so this test is not reaching the "
           "territory it exists for — check the generator";

    // WHAT the declines are made of. Until booleanToBody could say, every empty looked
    // alike and the corpus could only report "48% empty" — a number that mixes a missing
    // capability with a correct answer. Measured over ~3600 offline steps: 53.0% Ok,
    // 35.2% UnexpressibleSeam, 6.9% SewFailed, 4.9% EmptyResult, 0% ImprintBudget. So the
    // absent curved SSI is roughly FIVE TIMES the arrangement gap, which is the opposite
    // of the priority the two had been recorded in.
    const size_t unexpressible = byReason[static_cast<size_t>(brep::BooleanDiagnostic::UnexpressibleSeam)];
    const size_t sewFailed = byReason[static_cast<size_t>(brep::BooleanDiagnostic::SewFailed)];
    EXPECT_GT(unexpressible, 0u)
        << "no chain hit a quartic seam — the generator has stopped producing skew/off-axis "
           "curved pairs, and the SSI gap is no longer being measured";
    // A CEILING, not a floor: this is the size of a known gap, and it must only shrink.
    // Marching-SSI work should drive it toward zero and then tighten this bound.
    EXPECT_LT(unexpressible, static_cast<size_t>(steps) / 2u)
        << unexpressible << " of " << steps << " steps declined for an inexpressible seam "
        << "(" << sewFailed << " sew failures) — the curved-surface gap has WIDENED";
}

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
    RecordProperty("meshBooleanChecked", checked);
    RecordProperty("meshBooleanLeaks", leaks);
    // This was a tracked metric while the randomized battery still leaked. Sizing the
    // Bowyer-Watson super-triangle to the input's thinness (rather than a fixed multiple
    // of its bounding box) took it to zero: a sliver triple's circumcircle no longer
    // swallows the super-triangle, so no cut face loses real area. It is an assertion now
    // — a leak here is a regression, not a known residual.
    EXPECT_EQ(leaks, 0) << "a randomized mesh boolean leaked (" << leaks << " of " << checked
                        << ") — watertightness regressed";
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
            for (int step = 0; step < 16; ++step) {
                const uint32_t ec = hem.edgeCount(), vc = hem.vertexCount(), fc = hem.faceCount();
                if (ec == 0 || vc == 0 || fc == 0) break;
                // Random in-range indices INCLUDING tombstoned slots — the whole point:
                // every mutating op must refuse a dead index rather than corrupt.
                const uint32_t he = rng() % ec, v = rng() % vc, v2 = rng() % vc, f = rng() % fc;
                switch (rng() % 13u) {
                    case 0:  (void)hem.flipEdge(he); break;
                    case 1:  (void)hem.splitEdge(he); break;
                    case 2:  (void)hem.insertEdgeVertex(he, 0.5f); break;
                    case 3:  (void)hem.collapseEdge(he, Vec3{0.f, 0.f, 0.f}); break;
                    case 4:  (void)hem.insertEdgeLoop(he, 0.5f); break;
                    case 5:  (void)hem.slideEdgeLoop(he, 0.3f); break;
                    case 6:  (void)hem.bevelVertex(v, 0.1f); break;
                    case 7:  (void)hem.pokeFace(f); break;
                    case 8:  (void)hem.connectVertices(v, v2); break;
                    case 9:  (void)hem.insetFace(f, 0.3f); break;
                    case 10: (void)hem.extrudeFacePrism(f, 0.2f); break;
                    case 11: (void)hem.extrudeFaces(std::vector<uint32_t>{f}, 0.2f, (rng() & 1u) != 0u); break;
                    default: (void)hem.splitFacesAlongLoop(std::vector<uint32_t>{v, v2}); break;
                }
                const auto ig = hem.checkIntegrity();
                ASSERT_TRUE(ig.ok) << "seed op corrupted HEM: " << ig.reason;
            }
        }
    }
}

// The public liveness query mirrors the tombstone state, and every edge-index
// mutating op REFUSES a dead index (the succeed-or-refuse-never-corrupt contract
// that keeps the fuzz sweep above safe on the raw [0,edgeCount()) index space).
TEST(KernelFuzz, LivenessQueryTracksTombstonesAndOpsRefuseDeadIndices)
{
    auto hemOpt = HalfEdgeMesh::fromMesh(primitives::makeBox(2.f, 2.f, 2.f));
    ASSERT_TRUE(hemOpt.has_value());
    HalfEdgeMesh hem = std::move(*hemOpt);
    for (uint32_t e = 0; e < hem.edgeCount(); ++e) EXPECT_TRUE(hem.isLiveEdge(e));
    for (uint32_t f = 0; f < hem.faceCount(); ++f) EXPECT_TRUE(hem.isLiveFace(f));

    ASSERT_TRUE(hem.insertEdgeVertex(0, 0.5f));  // an Euler op tombstones split-away edges
    ASSERT_TRUE(hem.checkIntegrity().ok);

    uint32_t dead = HalfEdgeMesh::kInvalid;
    for (uint32_t e = 0; e < hem.edgeCount(); ++e) {
        if (!hem.isLiveEdge(e)) { dead = e; break; }
    }
    ASSERT_NE(dead, HalfEdgeMesh::kInvalid) << "the Euler op should tombstone at least one edge";

    // Every edge-index mutating op refuses the dead index and leaves the mesh clean.
    EXPECT_FALSE(hem.flipEdge(dead));
    EXPECT_FALSE(hem.splitEdge(dead));
    EXPECT_FALSE(hem.insertEdgeVertex(dead, 0.5f));
    EXPECT_FALSE(hem.collapseEdge(dead, Vec3{0.f, 0.f, 0.f}));
    EXPECT_FALSE(hem.insertEdgeLoop(dead, 0.5f));
    EXPECT_FALSE(hem.slideEdgeLoop(dead, 0.3f));
    EXPECT_TRUE(hem.checkIntegrity().ok);
}

namespace {
Mesh rawMesh(std::vector<Vec3> pos, std::vector<std::vector<uint32_t>> faces)
{
    Mesh m;
    m.attributes().setPositions(std::move(pos));
    for (auto& f : faces) { Face fc; fc.indices = std::move(f); m.topology().addFace(fc); }
    return m;
}
}  // namespace

// MALFORMED-INPUT hardening (GAP-TEST1): feed adversarial (but in-range) mesh
// archetypes through every public entry point and assert the kernel handles them
// GRACEFULLY — no crash, no heap corruption, no NaN/±Inf propagated into an output
// that claims success. This battery surfaced (and this increment fixed) that
// MeshVertexWeld::weld and MeshRepair::repair(All) previously PROPAGATED non-finite
// positions; they now reject (weld→empty) / decline (repair→ok=false), matching the
// pervasive non-finite-rejection convention the boolean already followed.
TEST(KernelFuzz, MalformedInputsHandledGracefullyNoCrashNoNaNLeak)
{
    const float NaN = std::numeric_limits<float>::quiet_NaN();
    const float Inf = std::numeric_limits<float>::infinity();
    std::vector<std::pair<const char*, Mesh>> archetypes;
    archetypes.emplace_back("nonmanifold-edge",
        rawMesh({{0,0,0},{1,0,0},{0,1,0},{0,-1,0},{0,0,1}}, {{0,1,2},{0,1,3},{0,1,4}}));
    archetypes.emplace_back("degenerate-collinear", rawMesh({{0,0,0},{1,0,0},{2,0,0}}, {{0,1,2}}));
    archetypes.emplace_back("duplicate-verts",
        rawMesh({{0,0,0},{1,0,0},{0,1,0},{0,0,0}}, {{0,1,2},{3,1,2}}));
    archetypes.emplace_back("self-intersect",
        rawMesh({{-1,-1,0},{1,-1,0},{0,1,0},{0,-1,-1},{0,-1,1},{0,1,0.5f}}, {{0,1,2},{3,4,5}}));
    archetypes.emplace_back("nan-pos", rawMesh({{0,0,0},{NaN,0,0},{0,1,0}}, {{0,1,2}}));
    archetypes.emplace_back("inf-pos", rawMesh({{0,0,0},{Inf,0,0},{0,1,0}}, {{0,1,2}}));
    archetypes.emplace_back("empty", Mesh{});
    archetypes.emplace_back("single-tri", rawMesh({{0,0,0},{1,0,0},{0,1,0}}, {{0,1,2}}));

    auto allFinite = [](const Mesh& m) {
        for (const auto& p : m.attributes().positions())
            if (!isFinite(p)) return false;
        return true;
    };

    const Mesh box = primitives::makeBox(2.f, 2.f, 2.f);
    for (const auto& [name, m] : archetypes) {
        for (auto op : {BooleanOperationType::Union, BooleanOperationType::Intersection,
                        BooleanOperationType::Difference}) {
            // The mesh boolean rejects/guards non-finite → output is always finite.
            EXPECT_TRUE(allFinite(robustMeshBoolean(m, box, op))) << name << " robustMB(m,box)";
            EXPECT_TRUE(allFinite(robustMeshBoolean(box, m, op))) << name << " robustMB(box,m)";
            Mesh out;
            (void)BooleanOperation::compute(m, box, op, out);  // must not crash
            EXPECT_TRUE(allFinite(out)) << name << " compute";
        }
        (void)BooleanOperation::validateMesh(m);         // must not crash
        (void)MeshTopologyValidation::validate(m);       // must not crash
        (void)HalfEdgeMesh::fromMesh(m);                 // nullopt on non-manifold, never crash

        // weld rejects non-finite by returning empty → output always finite.
        EXPECT_TRUE(allFinite(MeshVertexWeld::weld(m))) << name << " weld";
        // repair either succeeds with finite output, or declines (ok=false).
        Mesh c = m;
        const auto rr = MeshRepair::repairAll(c);
        if (rr.ok) EXPECT_TRUE(allFinite(c)) << name << " repairAll claimed ok but leaked non-finite";
    }
}

// The non-finite-rejection convention, applied breadth-wise: every mesh-RETURNING
// processing op must refuse NaN/±Inf input rather than propagate it into a "result".
// (inc59 fixed weld/repair; this audit found the same gap in the decimate / thicken /
// smooth / displace / subdivide surface and fixed all of them.)
TEST(KernelFuzz, NonFiniteRejectedAcrossMeshReturningOps)
{
    auto allFinite = [](const Mesh& m) {
        for (const auto& p : m.attributes().positions())
            if (!isFinite(p)) return false;
        return true;
    };
    for (float bad : {std::numeric_limits<float>::quiet_NaN(),
                      std::numeric_limits<float>::infinity()}) {
        Mesh m = primitives::makeBox(2.f, 2.f, 2.f);
        (void)m.topology().triangulate();
        auto pos = m.attributes().positions();
        ASSERT_FALSE(pos.empty());
        pos[0] = {bad, 0.5f, 0.5f};
        m.attributes().setPositions(std::move(pos));

        EXPECT_TRUE(allFinite(MeshSimplify::decimateByRatio(m, 0.5f))) << "decimateByRatio";
        EXPECT_TRUE(allFinite(MeshSimplify::decimateToTarget(m, 4u))) << "decimateToTarget";
        EXPECT_TRUE(allFinite(MeshThicken::solidify(m))) << "solidify";
        EXPECT_TRUE(allFinite(MeshLaplacian::smooth(m))) << "smooth";
        EXPECT_TRUE(allFinite(MeshDisplace::displaceByScalar(m, [](const Vec3&) { return 0.1f; })))
            << "displaceByScalar";
        EXPECT_TRUE(allFinite(MeshDisplace::displaceByVector(m, [](const Vec3& q) { return q; })))
            << "displaceByVector";
        if (auto hem = HalfEdgeMesh::fromMesh(m)) {
            EXPECT_FALSE(SubdivisionSurface::catmullClark(*hem).has_value()) << "catmullClark";
            EXPECT_FALSE(SubdivisionSurface::loopSubdivide(*hem).has_value()) << "loopSubdivide";
        }
    }
}

// The last two mesh-processing ops with a different result shape: MeshDecimator
// (returns optional) and RemeshOperation (returns a report + out-param mesh). Both
// previously propagated non-finite — decimation into its output HEM, and remesh
// reporting VALID with a NaN output mesh (the worst kind). They now reject.
TEST(KernelFuzz, NonFiniteRejectedByDecimateAndRemesh)
{
    for (float bad : {std::numeric_limits<float>::quiet_NaN(),
                      std::numeric_limits<float>::infinity()}) {
        Mesh m = primitives::makeBox(2.f, 2.f, 2.f);
        (void)m.topology().triangulate();
        auto pos = m.attributes().positions();
        ASSERT_FALSE(pos.empty());
        pos[0] = {bad, 0.5f, 0.5f};
        m.attributes().setPositions(std::move(pos));

        Mesh out;
        const RemeshReport rep = RemeshOperation::apply(m, RemeshDesc{}, out);
        EXPECT_FALSE(rep.valid) << "remesh must decline non-finite input, not report valid";

        if (auto hem = HalfEdgeMesh::fromMesh(m)) {
            EXPECT_FALSE(MeshDecimator::decimate(*hem).has_value()) << "decimate must reject non-finite";
        }
    }
}

// Voxelization rejects non-finite input (empty grid, not a finite-but-garbage one).
// This test's inclusion of both MeshVoxelize.h and MeshSurfaceNets.h is also the
// compile-time regression guard for the VoxelGrid double-definition (now: VoxelGrid
// in MeshVoxelize.h, SurfaceNetsGrid in MeshSurfaceNets.h — no clash).
TEST(KernelFuzz, VoxelizeRejectsNonFiniteInput)
{
    for (float bad : {std::numeric_limits<float>::quiet_NaN(),
                      std::numeric_limits<float>::infinity()}) {
        Mesh m = primitives::makeBox(2.f, 2.f, 2.f);
        (void)m.topology().triangulate();
        auto pos = m.attributes().positions();
        ASSERT_FALSE(pos.empty());
        pos[0] = {bad, 0.5f, 0.5f};
        m.attributes().setPositions(std::move(pos));
        EXPECT_TRUE(MeshVoxelize::voxelize(m).occupancy.empty()) << "voxelize must reject non-finite input";
    }
    // A valid mesh still voxelizes to a non-empty grid (behaviour unchanged).
    EXPECT_FALSE(MeshVoxelize::voxelize(primitives::makeBox(2.f, 2.f, 2.f)).occupancy.empty());
}

}  // namespace nexus::geometry::testing
