#include <gtest/gtest.h>
#include <vector>
#include <utility>
#include <map>
#include <algorithm>
#include <nexus/geometry/MeshThicken.h>

#include <nexus/geometry/Mesh.h>
#include <nexus/geometry/MeshTopologyValidation.h>

using namespace nexus::geometry;
using namespace nexus::geometry::primitives;

TEST(MeshTopologyValidation, EulerCharacteristicOfCubeIsTwo) {
    EXPECT_EQ(MeshTopologyValidation::computeEulerCharacteristic(8, 12, 6), 2);
}

TEST(MeshTopologyValidation, EulerCharacteristicOfTetrahedron) {
    EXPECT_EQ(MeshTopologyValidation::computeEulerCharacteristic(4, 6, 4), 2);
}

TEST(MeshTopologyValidation, GenusOfSphereIsZero) {
    EXPECT_EQ(MeshTopologyValidation::computeGenus(2, 0), 0);
}

TEST(MeshTopologyValidation, GenusOfTorusIsOne) {
    EXPECT_EQ(MeshTopologyValidation::computeGenus(0, 0), 1);
}

TEST(MeshTopologyValidation, CubeValidationPasses) {
    Mesh cube = makeBox(2.0f, 2.0f, 2.0f);
    auto result = MeshTopologyValidation::validate(cube);
    EXPECT_TRUE(result.valid);
}

TEST(MeshTopologyValidation, EmptyMeshFailsValidation) {
    Mesh empty;
    auto result = MeshTopologyValidation::validate(empty);
    EXPECT_FALSE(result.valid);
}

TEST(MeshTopologyValidation, EulerOnCubeIsTwo) {
    Mesh cube = makeBox(2.0f, 2.0f, 2.0f);
    auto result = MeshTopologyValidation::validate(cube);
    EXPECT_EQ(result.euler, 2);
}

// ── Validation against independently-known topology ─────────────────────────────
//
// Every watertightness claim in the boolean work rests on boundaryLoops == 0, and until
// now nothing tested it directly: four tests above call the static arithmetic helpers with
// numbers written by hand, and the two that validate a mesh both use a closed cube. A
// closed cube is exactly the case where the defects below cancel out.
//
// These use surfaces whose answers come from topology rather than from a previous run:
// chi = 2 for a sphere, 0 for a torus, 1 for a disk, and chi = 2 - 2g - b in general.

namespace {

Mesh dropTriangles(const Mesh& src, const std::vector<size_t>& drop)
{
    Mesh m = src;
    (void)m.topology().triangulate();
    std::vector<Face> keep;
    for (size_t f = 0; f < m.topology().faceCount(); ++f) {
        if (std::find(drop.begin(), drop.end(), f) == drop.end()) keep.push_back(m.topology().face(f));
    }
    m.topology().clearFaces();
    for (const auto& f : keep) m.topology().addFace(f);
    return m;
}

}  // namespace

TEST(MeshTopologyValidation, ClosedSurfacesHaveNoBoundaryAndKnownEuler)
{
    const auto cube = MeshTopologyValidation::validate(makeBox(2.f, 2.f, 2.f));
    EXPECT_EQ(cube.boundaryLoops, 0u);
    EXPECT_EQ(cube.euler, 2);
    EXPECT_EQ(cube.genus, 0);

    const auto sphere = MeshTopologyValidation::validate(primitives::makeSphere(1.f, 12, 16));
    EXPECT_EQ(sphere.boundaryLoops, 0u);
    EXPECT_EQ(sphere.euler, 2);
    EXPECT_EQ(sphere.genus, 0);

    // A torus is the case that separates genus from Euler characteristic.
    const auto torus = MeshTopologyValidation::validate(primitives::makeTorus(1.f, 0.3f, 16, 12));
    EXPECT_EQ(torus.boundaryLoops, 0u);
    EXPECT_EQ(torus.euler, 0);
    EXPECT_EQ(torus.genus, 1);
}

// An OPEN surface is where the edge count mattered. E was taken as half the half-edge
// count, which assumes every edge carries two half-edges — true only when closed. A
// boundary edge carries one, so E came out short by half the boundary and the Euler
// characteristic too high by the same amount: a 4x4 plane, a disk with chi = 1, reported 9.
TEST(MeshTopologyValidation, OpenSurfacesReportTheCorrectEuler)
{
    const auto plane = MeshTopologyValidation::validate(makePlane(2.f, 2.f, 4, 4));
    EXPECT_EQ(plane.boundaryLoops, 1u) << "a plane is a disk — exactly one boundary loop";
    EXPECT_EQ(plane.euler, 1) << "a disk has chi = 1";

    const Mesh cube = makeBox(2.f, 2.f, 2.f);
    const auto punctured = MeshTopologyValidation::validate(dropTriangles(cube, {0}));
    EXPECT_EQ(punctured.boundaryLoops, 1u);
    EXPECT_EQ(punctured.euler, 1) << "a sphere with one puncture is a disk";
}

// Genus was computed as (2 - boundaryLoops - euler) / 2 with boundaryLoops unsigned, so
// the whole expression went unsigned: one boundary loop and chi = 3 evaluated 1u - 3 as
// 4294967294 and reported a genus of 2147483647. Every open mesh got that number.
TEST(MeshTopologyValidation, GenusIsSaneOnOpenSurfaces)
{
    const auto plane = MeshTopologyValidation::validate(makePlane(2.f, 2.f, 4, 4));
    EXPECT_GE(plane.genus, 0);
    EXPECT_LE(plane.genus, 1) << "a disk cannot have genus " << plane.genus;

    const auto punctured =
        MeshTopologyValidation::validate(dropTriangles(makeBox(2.f, 2.f, 2.f), {0}));
    EXPECT_GE(punctured.genus, 0);
    EXPECT_LE(punctured.genus, 1) << "a punctured sphere cannot have genus " << punctured.genus;

    // Directly: the arithmetic must not wrap for any plausible boundary count.
    for (uint32_t loops = 0; loops < 8; ++loops) {
        for (int chi = -6; chi <= 2; ++chi) {
            const int g = MeshTopologyValidation::computeGenus(chi, loops);
            EXPECT_GE(g, 0) << "genus went negative at chi=" << chi << " loops=" << loops;
            EXPECT_LT(g, 1000) << "genus overflowed at chi=" << chi << " loops=" << loops;
        }
    }
}

// Three faces meeting at one edge is the textbook non-manifold case, and it was reported
// VALID with no violations. The check looked for an edge whose half-edge valence exceeded
// two — but a half-edge mesh cannot represent that configuration in the first place, so
// the third face never got a twin, read as boundary, and the valence never rose. The
// evidence has to be taken from the face list, before any structure is built.
TEST(MeshTopologyValidation, NonManifoldEdgeIsDetected)
{
    Mesh m;
    m.attributes().setPositions({{0.f, 0.f, 0.f}, {1.f, 0.f, 0.f},
                                 {0.f, 1.f, 0.f}, {0.f, -1.f, 0.f}, {0.f, 0.f, 1.f}});
    for (const std::vector<uint32_t>& idx :
         std::vector<std::vector<uint32_t>>{{0, 1, 2}, {0, 1, 3}, {0, 1, 4}}) {
        Face f{};
        f.indices = idx;
        m.topology().addFace(f);
    }
    ASSERT_TRUE(m.isValid());

    const auto r = MeshTopologyValidation::validate(m);
    EXPECT_FALSE(r.valid) << "three faces on one edge was accepted as valid";
    bool sawNonManifoldEdge = false;
    for (const auto& v : r.violations) {
        if (v.kind == TopologyViolation::NonManifoldEdge) sawNonManifoldEdge = true;
    }
    EXPECT_TRUE(sawNonManifoldEdge) << "no NonManifoldEdge violation was raised";
}

// A closed surface must also be consistently ORIENTED, or no half-edge structure can pair
// its faces. Solidifying an open surface used to produce a shell that was closed as a set
// of triangles yet wound inconsistently — six directed edges each traversed twice the same
// way — because the boundary edges it built its walls from had been stored sorted, losing
// the direction their face traversed them.
TEST(MeshTopologyValidation, EveryDirectedEdgeOfAClosedSurfaceIsUsedOnce)
{
    auto check = [](const char* what, const Mesh& src) {
        Mesh m = src;
        ASSERT_TRUE(m.topology().triangulate());
        std::map<std::pair<uint32_t, uint32_t>, int> directed;
        for (size_t f = 0; f < m.topology().faceCount(); ++f) {
            const auto& fc = m.topology().face(f);
            if (fc.indices.size() < 3) continue;
            for (size_t k = 0; k < fc.indices.size(); ++k) {
                directed[{fc.indices[k], fc.indices[(k + 1) % fc.indices.size()]}]++;
            }
        }
        int repeated = 0;
        for (const auto& [e, c] : directed) {
            if (c > 1) ++repeated;
        }
        EXPECT_EQ(repeated, 0) << what << ": " << repeated
                               << " directed edges are traversed more than once — the "
                                  "surface is closed but not consistently oriented";
    };

    check("box", makeBox(2.f, 2.f, 2.f));
    check("sphere", primitives::makeSphere(1.f, 12, 16));

    ThickenOptions opts;
    opts.thickness = 0.1f;
    const Mesh shell = MeshThicken::solidify(makePlane(1.f, 1.f, 1, 1), opts);
    check("solidified plane", shell);
    const auto r = MeshTopologyValidation::validate(shell);
    EXPECT_EQ(r.boundaryLoops, 0u) << "the solidified shell is not closed";
    EXPECT_EQ(r.euler, 2) << "the solidified shell is not a genus-0 closed surface";
}
