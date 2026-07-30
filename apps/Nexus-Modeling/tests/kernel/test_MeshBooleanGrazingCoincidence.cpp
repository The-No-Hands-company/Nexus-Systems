// Foundation — WHY THE COINCIDENCE TOLERANCE IS 1e-5, MEASURED ON THE ONE CONFIGURATION
// THAT NEEDS IT.
//
// The B-rep's move to double lifted its construction error to ~2e-16, and the obvious next
// step was to re-tighten Tolerance's defaults, which had been sized for float at roughly
// eight units in the last place. That failed, and the first explanation offered for the
// failure was wrong in an instructive way: the mesh boolean still stores positions as
// float, so the loose weld band was written up as a float-precision workaround that a
// mesh-side migration would remove.
//
// It is not. Probed at a tightened 1e-6, exactly ONE configuration out of 300 leaks — a
// sphere at one particular random offset, failing under all three operators — and the
// points it cannot weld are 3.19e-06 apart. That is roughly 25x float epsilon at unit
// scale, far too large to be rounding, and the geometry says plainly what it is:
//
//   v64  (-0.126088738, 0.526467443, 1.000000000)   on the box face, on the sphere
//   v66  (-0.126070619, 0.526456952, 1.000000000)   on the box face, on the sphere
//   v204 (-0.126087785, 0.526467562, 0.999996960)   a SPHERE VERTEX, 3.04e-06 below it
//
// All three lie on the sphere to within a micron of its 1.2 radius. The sphere grazes the
// box's top face so closely that its tessellation drops a vertex three microns under the
// plane, while the seam crossing for the same region lands exactly on it. Whether those
// count as one point or two is not a question about precision — it is the coincidence
// decision itself, and the tolerance is the parameter that makes it.
//
// So the band is a MODELING scale, not a numerical workaround. Tightening it does not
// clean anything up; it redefines what "the same point" means, and a grazing sphere then
// cannot close its seam. Widening the mesh boolean's arithmetic would not move this by one
// bit, because the distance being bridged is real.

#include <nexus/geometry/Mesh.h>
#include <nexus/geometry/MeshBooleanRobust.h>
#include <nexus/geometry/MeshTopologyValidation.h>
#include <nexus/geometry/Tolerance.h>

#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <utility>
#include <vector>

namespace nexus::geometry::testing {


namespace {

using V = nexus::render::Vec3;

Mesh translated(Mesh m, V d)
{
    auto p = m.attributes().positions();
    for (auto& q : p) { q.x += d.x; q.y += d.y; q.z += d.z; }
    m.attributes().setPositions(std::move(p));
    return m;
}

// The exact offset the characterization fixture reaches at seg=32, i=2 — the third draw of
// mt19937(4242) through uniform_real_distribution<float>(-1.6, 1.6). Reproduced literally
// so this test does not depend on the fixture's loop structure staying put.
constexpr V kGrazingOffset{-0.683170f, -0.531838f, 0.901768f};
constexpr float kSphereRadius = 1.2f;
constexpr float kBoxTopZ = 1.0f;

constexpr std::array<BooleanOperationType, 3> kOps = {BooleanOperationType::Union,
                                                      BooleanOperationType::Difference,
                                                      BooleanOperationType::Intersection};

}  // namespace

// THE measurement. The configuration contains a sphere vertex a few microns below the box's
// top face — a real distance, an order of magnitude above float resolution and an order
// BELOW the coincidence tolerance. That band is what decides it is the same point as the
// seam crossing on the plane.
TEST(MeshBooleanGrazingCoincidence, ASphereVertexLandsMicronsBelowTheBoxFace)
{
    const Mesh sphere = translated(primitives::makeSphere(kSphereRadius, 32, 36), kGrazingOffset);
    const auto& pos = sphere.attributes().positions();

    // The closest sphere vertex to the box's top plane, approaching from below.
    double closestBelow = 1e30;
    for (const V& p : pos) {
        const double gap = static_cast<double>(kBoxTopZ) - static_cast<double>(p.z);
        if (gap >= 0.0 && gap < closestBelow) closestBelow = gap;
    }

    EXPECT_LT(closestBelow, 1e-5)
        << "the grazing configuration no longer grazes — closest sphere vertex below the "
           "box face is " << closestBelow << ", so this test is measuring nothing";
    EXPECT_GT(closestBelow, 1e-7)
        << "the gap is " << closestBelow << ", which is float rounding rather than a real "
           "near-coincidence; the finding this test records would not hold";

    // It is a genuine surface point, not an artifact: it sits on the sphere.
    const double tol = static_cast<double>(Tolerance{}.at(2.0f * kSphereRadius));
    for (const V& p : pos) {
        if (static_cast<double>(kBoxTopZ) - static_cast<double>(p.z) != closestBelow) continue;
        const double dx = p.x - kGrazingOffset.x, dy = p.y - kGrazingOffset.y,
                     dz = p.z - kGrazingOffset.z;
        EXPECT_NEAR(std::sqrt(dx * dx + dy * dy + dz * dz), kSphereRadius, tol)
            << "the grazing vertex is not on the sphere";
        break;
    }
}

// And with the shipped band the configuration resolves: all three operators produce a
// watertight result. This is the assertion that would break if the tolerance were tightened
// without deciding, deliberately, that three-micron features in a two-unit model are
// distinct.
TEST(MeshBooleanGrazingCoincidence, TheGrazingConfigurationIsWatertightAtTheShippedBand)
{
    const Mesh box = primitives::makeBox(2.f, 2.f, 2.f);
    const Mesh sphere = translated(primitives::makeSphere(kSphereRadius, 32, 36), kGrazingOffset);

    for (const auto op : kOps) {
        const Mesh r = robustMeshBoolean(box, sphere, op);
        const auto v = MeshTopologyValidation::validate(r);
        EXPECT_EQ(v.boundaryLoops, 0u)
            << "op " << static_cast<int>(op) << " leaked on the grazing configuration";
    }
}

// The band is a scale, not a constant, and the grazing case must survive being modelled in
// different units. A configuration that only closes at one size would mean the band had been
// fitted to this fixture rather than expressing a coincidence scale.
TEST(MeshBooleanGrazingCoincidence, TheGrazingCaseResolvesAtEveryModelSize)
{
    for (const float s : {0.01f, 1.f, 100.f}) {
        const Mesh box = primitives::makeBox(2.f * s, 2.f * s, 2.f * s);
        const Mesh sphere = translated(primitives::makeSphere(kSphereRadius * s, 32, 36),
                                       {kGrazingOffset.x * s, kGrazingOffset.y * s,
                                        kGrazingOffset.z * s});
        for (const auto op : kOps) {
            const auto v = MeshTopologyValidation::validate(robustMeshBoolean(box, sphere, op));
            EXPECT_EQ(v.boundaryLoops, 0u)
                << "scale " << s << ", op " << static_cast<int>(op)
                << " leaked — the weld band is not tracking model size here";
        }
    }
}

}  // namespace nexus::geometry::testing
