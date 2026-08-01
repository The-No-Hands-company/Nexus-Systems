// A tessellator that refined the boundary and then ignored it.
//
// `toMesh` placed subdivision points along every edge — correctly, on the edge's own curve, so
// both incident faces share them — and then triangulated each face by FANNING its ring from the
// ring's first vertex. On a flat face that is exact. On a curved one it was the whole of the
// under-refinement defect, and it produced three symptoms that all trace to the same cause: a
// fan connects ring points that are far apart ON THE SURFACE, and in 3D that chord cuts through
// the solid.
//
//   1. AREA AND VOLUME STALLED. cylinder(1,2,16) converged to 6.1757 against an exact
//      2*pi = 6.28319, and it CONVERGED — to the wrong number — because every level repeated the
//      same mistake. A cross-section carried the resolution of the UNREFINED rim however high
//      the subdivision count went.
//   2. ZERO-AREA TRIANGLES, AND WATERTIGHTNESS DEPENDED ON THEM. Where the ring's first vertex
//      lay on a straight refined edge — a cylinder's generatrix — the fan's trailing triangles
//      were three collinear points, measured at subdivisions 2 as (0.924, 0.383, z) for
//      z = -1, -0.333, 0.333. A zero-area triangle has no defined orientation, so its winding was
//      arbitrary; and it was ALSO the only coverage of those boundary sub-segments, which is why
//      dropping them opened 42 one-sided edges. 128 of them on a cylinder at subdivisions 8.
//   3. NON-MANIFOLD EDGES. The fan emitted the CHORD across a boundary edge that was already
//      subdivided, because its apex sat next to that edge in the ring. Edge (8,24) of that
//      cylinder has length 2.000 — the full un-refined generatrix — and was used FOUR times.
//      Those edges should not exist in the mesh at all. 368 of them on a sphere at subdivisions
//      16, and the sphere's tessellated area came out 7% ABOVE the true 4*pi*r^2, which for an
//      inscribed surface can only mean it was covering itself twice.
//
// A curved face is now triangulated in the surface's own (u,v) domain, where "short" means short
// along the surface. Two paths, both exact for what they claim, both declining rather than
// approximating — and anything they decline keeps the old fan, which is safe because
// `buildRing` derives a face's boundary from its coedges without consulting how the face will be
// triangulated, so a new-path face and an old-path neighbour still meet exactly.
//
// WHAT IS NOT FIXED, and is named rather than implied: the SPHERE. It is curved both ways, so it
// genuinely needs interior samples, and interior samples drawn from each face's own boundary
// break a property the kernel depends on — a tessellated identity like U+I == A+B holds only
// because a boundary-only triangulation's totals telescope across any decomposition. Bounded
// (and it must be bounded: classifyPoint tessellates on every query) the fragment stops refining
// while the primitive continues, and the identity drifts rather than converging. Measured, that
// attempt took the identity from 4.2e-04 at subdivisions 2 to 4.1e-03 at 8. The designed fix is
// to draw the lattice from the SURFACE and the subdivision count instead of from each face's
// boundary, so any two decompositions sample the same positions.

#include <nexus/geometry/AnalyticBRep.h>
#include <nexus/geometry/BRepBoolean.h>
#include <nexus/geometry/Mesh.h>
#include <nexus/geometry/MeshMassProperties.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <map>
#include <utility>
#include <vector>

namespace nexus::geometry::brep::testing {

namespace {

constexpr double kPi = 3.14159265358979323846;

struct Tri { uint32_t a, b, c; };

std::vector<Tri> triangles(const Mesh& m)
{
    std::vector<Tri> t;
    for (size_t i = 0; i < m.topology().faceCount(); ++i) {
        const auto& f = m.topology().face(i);
        for (size_t k = 1; k + 1 < f.indices.size(); ++k)
            t.push_back({f.indices[0], f.indices[k], f.indices[k + 1]});
    }
    return t;
}

double triArea(const Mesh& m, const Tri& t)
{
    const auto& p = m.attributes().positions();
    const auto& A = p[t.a]; const auto& B = p[t.b]; const auto& C = p[t.c];
    const double ux = B.x - A.x, uy = B.y - A.y, uz = B.z - A.z;
    const double vx = C.x - A.x, vy = C.y - A.y, vz = C.z - A.z;
    const double cx = uy * vz - uz * vy, cy = uz * vx - ux * vz, cz = ux * vy - uy * vx;
    return 0.5 * std::sqrt(cx * cx + cy * cy + cz * cz);
}

double unsignedArea(const Mesh& m)
{
    double a = 0.0;
    for (const Tri& t : triangles(m)) a += triArea(m, t);
    return a;
}

double tessVolume(const Body& b, uint32_t sub)
{
    return b.faceCount() == 0u ? 0.0 : MeshMassProperties::compute(b.toMesh(sub)).volume;
}

// Zero-area triangles, one-sided edges and edges used more than twice — the three things the
// fan produced and the ruled paths must not.
struct MeshHealth { int degenerate = 0, oneSided = 0, overUsed = 0; };

MeshHealth health(const Mesh& m)
{
    MeshHealth h;
    const std::vector<Tri> t = triangles(m);
    double biggest = 0.0;
    for (const Tri& x : t) biggest = std::max(biggest, triArea(m, x));
    const double floorA = biggest * 1e-9;
    std::map<std::pair<uint32_t, uint32_t>, int> undirected;
    for (const Tri& x : t) {
        if (triArea(m, x) <= floorA) ++h.degenerate;
        const uint32_t v[3] = {x.a, x.b, x.c};
        for (int k = 0; k < 3; ++k) {
            uint32_t a = v[k], b = v[(k + 1) % 3];
            if (a > b) std::swap(a, b);
            ++undirected[{a, b}];
        }
    }
    for (const auto& e : undirected) {
        if (e.second == 1) ++h.oneSided;
        else if (e.second > 2) ++h.overUsed;
    }
    return h;
}

}  // namespace

// THE defect: refining must actually refine. A chordal tessellation of a convex solid is
// inscribed, so it approaches the true volume from below and must keep approaching.
TEST(BRepCurvedTessellation, ACylinderConvergesToItsTrueVolume)
{
    const Body cyl = makeCylinder(1.f, 2.f, 16);
    const double exact = kPi * 1.0 * 1.0 * 2.0;

    double prev = 0.0;
    for (const uint32_t s : {0u, 1u, 2u, 4u, 8u, 16u}) {
        const double v = tessVolume(cyl, s);
        EXPECT_LT(v, exact) << "sub=" << s << ": an inscribed tessellation cannot exceed the solid";
        EXPECT_GT(v, prev) << "sub=" << s << ": refinement did not improve the volume";
        prev = v;
    }
    // it used to plateau 1.7% short; the ruled strips are exact for the rim resolution they have
    EXPECT_LT(exact - tessVolume(cyl, 16), exact * 1e-3)
        << "still more than 0.1% short at subdivisions 16";
}

TEST(BRepCurvedTessellation, AConeConvergesToItsTrueVolume)
{
    const Body cone = makeCone(1.f, 2.f, 16);
    const double exact = kPi * 1.0 * 1.0 * 2.0 / 3.0;

    double prev = 0.0;
    for (const uint32_t s : {0u, 1u, 2u, 4u, 8u, 16u}) {
        const double v = tessVolume(cone, s);
        EXPECT_LT(v, exact) << "sub=" << s;
        EXPECT_GT(v, prev) << "sub=" << s << ": refinement did not improve the volume";
        prev = v;
    }
    EXPECT_LT(exact - tessVolume(cone, 16), exact * 1e-3)
        << "still more than 0.1% short at subdivisions 16";
}

// Area is the quantity that exposes double coverage; signed volume cancels it, and so does every
// topological invariant the kernel owns. An inscribed tessellation's area cannot EXCEED the true
// surface area, and the fan's did.
TEST(BRepCurvedTessellation, TessellatedAreaNeverExceedsTheTrueSurfaceArea)
{
    struct Case { Body b; double exact; const char* what; };
    std::vector<Case> cases;
    cases.push_back({makeCylinder(1.f, 2.f, 16), 2.0 * kPi * 1.0 * 2.0 + 2.0 * kPi * 1.0 * 1.0,
                     "cylinder"});
    cases.push_back({makeCone(1.f, 2.f, 16), kPi * 1.0 * std::sqrt(5.0) + kPi, "cone"});

    for (const Case& c : cases)
        for (const uint32_t s : {0u, 1u, 2u, 4u, 8u, 16u}) {
            const double a = unsignedArea(c.b.toMesh(s));
            EXPECT_LT(a, c.exact * (1.0 + 1e-9))
                << c.what << " sub=" << s << ": tessellated area " << a << " exceeds " << c.exact
                << " — the surface is covering itself twice";
        }

    // and it converges up to it
    EXPECT_LT(cases[0].exact - unsignedArea(cases[0].b.toMesh(16)), cases[0].exact * 1e-3);
}

// The three defects the fan produced, asserted to be gone at every level. This is the check that
// gates everything else: `toMesh`'s watertightness used to REST on its zero-area triangles.
TEST(BRepCurvedTessellation, ADevelopableSolidTessellatesCleanlyAtEveryLevel)
{
    struct Case { Body b; const char* what; };
    std::vector<Case> cases;
    cases.push_back({makeCylinder(1.f, 2.f, 16), "cylinder"});
    cases.push_back({makeCone(1.f, 2.f, 16), "cone"});
    cases.push_back({makeCylinder(0.3f, 5.f, 7), "thin cylinder, odd segments"});

    for (const Case& c : cases)
        for (const uint32_t s : {0u, 1u, 2u, 3u, 4u, 8u, 16u}) {
            const MeshHealth h = health(c.b.toMesh(s));
            EXPECT_EQ(h.degenerate, 0) << c.what << " sub=" << s << ": zero-area triangles";
            EXPECT_EQ(h.oneSided, 0) << c.what << " sub=" << s << ": the mesh is not closed";
            EXPECT_EQ(h.overUsed, 0)
                << c.what << " sub=" << s << ": " << h.overUsed
                << " edges used more than twice — non-manifold";
        }
}

// A boolean fragment must tessellate like the primitive it was cut from, or every tessellated
// conservation identity in the suite becomes a lottery. This is the property that made the
// interior-lattice attempt unacceptable, and the reason the developable paths are exact rather
// than merely better: exactness is what makes a fragment agree with the whole.
TEST(BRepCurvedTessellation, AFragmentTessellatesLikeTheSolidItCameFrom)
{
    // the union with a box swallowed whole IS the cylinder, but its faces have been segmented
    const Body cyl = makeCylinder(1.f, 2.f, 12);
    const Body box = makeBox(1.f, 1.f, 1.f);
    const Body u = booleanToBody(box, cyl, BooleanOp::Union);
    ASSERT_GT(u.faceCount(), 0u);
    ASSERT_TRUE(u.isClosed());
    EXPECT_GT(u.faceCount(), cyl.faceCount()) << "the fixture did not segment anything";

    for (const uint32_t s : {0u, 1u, 2u, 4u}) {
        EXPECT_NEAR(tessVolume(u, s), tessVolume(cyl, s), 1e-9 * tessVolume(cyl, s))
            << "sub=" << s << ": a segmented cylinder does not tessellate like the cylinder";
        const MeshHealth h = health(u.toMesh(s));
        EXPECT_EQ(h.oneSided, 0) << "sub=" << s;
        EXPECT_EQ(h.overUsed, 0) << "sub=" << s;
    }
}

// Determinism, which is the kernel's contract: the grid and strip construction must not depend
// on anything but the body.
TEST(BRepCurvedTessellation, IsDeterministic)
{
    const Body cyl = makeCylinder(1.f, 2.f, 16);
    for (const uint32_t s : {0u, 3u, 7u}) {
        const Mesh a = cyl.toMesh(s), b = cyl.toMesh(s);
        ASSERT_EQ(a.attributes().positions().size(), b.attributes().positions().size());
        ASSERT_EQ(a.topology().faceCount(), b.topology().faceCount());
        for (size_t i = 0; i < a.attributes().positions().size(); ++i) {
            EXPECT_EQ(a.attributes().positions()[i].x, b.attributes().positions()[i].x);
            EXPECT_EQ(a.attributes().positions()[i].y, b.attributes().positions()[i].y);
            EXPECT_EQ(a.attributes().positions()[i].z, b.attributes().positions()[i].z);
        }
        for (size_t i = 0; i < a.topology().faceCount(); ++i)
            EXPECT_EQ(a.topology().face(i).indices, b.topology().face(i).indices) << "face " << i;
    }
}

// The sphere is NOT fixed, and that is recorded here rather than left for someone to discover.
// If this starts failing because a sphere converges, the designed surface-lattice fix has landed
// and this characterization should become a convergence assertion like the cylinder's above.
TEST(BRepCurvedTessellation, ASphereStillUnderRefinesAndIsRecordedAsSuch)
{
    const Body s = makeSphere(1.f, 8, 12);
    const double exact = 4.0 / 3.0 * kPi;
    // it improves with refinement, but plateaus well short
    EXPECT_GT(tessVolume(s, 8), tessVolume(s, 1));
    EXPECT_GT(exact - tessVolume(s, 16), exact * 0.01)
        << "a sphere now tessellates to within 1% — the surface-parameter lattice must have "
           "landed, so retire this characterization and assert convergence instead";
    // and its area still exceeds the true one, which is the double-coverage signature
    EXPECT_GT(unsignedArea(s.toMesh(8)), 4.0 * kPi)
        << "the sphere's tessellation no longer over-covers — retire this characterization";
}

}  // namespace nexus::geometry::brep::testing
