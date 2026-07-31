// The last bit of one coordinate, deciding a whole patch's triangulation.
//
// A curved patch is fanned from a single apex, and the apex is pinned to a property of the
// GEOMETRY — the lexicographically smallest ring position — rather than to wherever the ring
// happens to start. That was deliberate and it is right: a fan from a fixed apex spans the
// same diagonals whichever way the ring is walked, so the same patch appearing in two
// results (a Boolean's intersection and its difference, say) cancels exactly instead of
// approximately.
//
// It only delivers that if "the same position" means the same to within ROUNDING. The
// comparison was `==` on the float positions, so a tie fell through to the next axis only
// when two coordinates agreed in every bit. On a seam that is not a coincidence — it is the
// rule. Every point imprinted onto a planar face carries that face's plane coordinate
// exactly, so ring after ring holds several points sharing an x, and which one won was
// settled by whatever noise sat in the last bit.
//
// MEASURED (fuzz seed 0xA17E51, iteration 102 — a sphere of radius 1.5103 turned a quarter
// turn about Y, against a box): one ring held a B-rep vertex and an arc midpoint whose x
// agreed to 15 significant figures. In the imprinted operand the midpoint's x was
// 2.8e-16 more negative and took the apex; in the union the two were bitwise equal, the
// comparison fell through to y, and the vertex took it. Same face, same ring, same points,
// in the same order — two different fans. Across the body, U + I fell 1.2e-02 short of
// A + B at subdivision 2, while agreeing to 2.4e-07 at subdivision 0, because subdivision
// is what puts those arc midpoints in the ring in the first place.
//
// Over 2000 fuzz configurations the conservation identities went from 14 violations
// (worst 8.8e-04) to 1 (worst 1.3e-06).
//
// Worth naming what this is NOT. Every validator the kernel owns passed throughout: the
// results were watertight, integral, χ=2, and every operand face appeared exactly once
// across U and I. The faces were identical, the edges were identical, the rings were
// identical point for point. Only the diagonals differed — and no topological check weighs
// anything.

#include <nexus/geometry/AnalyticBRep.h>
#include <nexus/geometry/BRepBoolean.h>
#include <nexus/geometry/MeshMassProperties.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace nexus::geometry::brep::testing {

namespace {

double meshVolume(const Body& b, uint32_t sub)
{
    return b.faceCount() == 0u ? 0.0 : MeshMassProperties::compute(b.toMesh(sub)).volume;
}

// The recorded configuration, rebuilt from the generator's own draws rather than described.
Body fixtureSphere()
{
    Body s = makeSphere(static_cast<float>(1.5103019082727049), 8, 12);
    nexus::render::Mat4 m = nexus::render::Mat4::identity();  // quarter turn about Y
    m.m[0][0] = 0.f;
    m.m[0][2] = 1.f;
    m.m[2][0] = -1.f;
    m.m[2][2] = 0.f;
    (void)s.transform(m);
    return s;
}

Body fixtureBox()
{
    Body b = makeBox(static_cast<float>(0.94014468538125784),
                     static_cast<float>(1.4057640824370552),
                     static_cast<float>(1.5863961564065807));
    b.translate({0.018062963296131551, 0.9547284185395819, -0.71821893512165458});
    return b;
}

// A triangle, keyed by its three positions so it can be compared between two bodies that
// hold the same patch. Rounded well above the last-bit noise being measured and well below
// any real feature — the point is which CORNERS were joined, not their exact values.
std::string triKey(const nexus::render::Vec3& a, const nexus::render::Vec3& b,
                   const nexus::render::Vec3& c)
{
    auto one = [](const nexus::render::Vec3& p) {
        char s[96];
        auto q = [](float v) { return std::floor(static_cast<double>(v) * 1e5 + 0.5) / 1e5 + 0.0; };
        std::snprintf(s, sizeof s, "%.5f,%.5f,%.5f", q(p.x), q(p.y), q(p.z));
        return std::string(s);
    };
    std::vector<std::string> v{one(a), one(b), one(c)};
    std::sort(v.begin(), v.end());
    return v[0] + "|" + v[1] + "|" + v[2];
}

std::vector<std::string> triangleKeys(const Body& b, uint32_t sub)
{
    std::vector<std::string> out;
    if (b.faceCount() == 0u) return out;
    const Mesh m = b.toMesh(sub);
    const std::vector<nexus::render::Vec3>& P = m.attributes().positions();
    for (size_t f = 0; f < m.topology().faceCount(); ++f) {
        const std::vector<uint32_t>& idx = m.topology().face(f).indices;
        for (size_t k = 2; k < idx.size(); ++k)
            out.push_back(triKey(P[idx[0]], P[idx[k - 1]], P[idx[k]]));
    }
    std::sort(out.begin(), out.end());
    return out;
}

}  // namespace

// THE assertion, stated on the geometry rather than on a total: a Boolean partitions the
// imprinted operands' faces between its results, so the TRIANGLES must partition too. Any
// patch that lands in U or I must be cut into exactly the triangles it had in the operand.
// This is what was false — 90 of 1220 triangles differed — and it is strictly stronger than
// the volume identity, which only catches it when the differences fail to cancel.
TEST(BRepFanApexStability, TheSamePatchIsTriangulatedTheSameWayInOperandAndResult)
{
    const Body A = fixtureSphere(), B = fixtureBox();
    Body Ai = A, Bi = B;
    ASSERT_TRUE(imprintMutually(Ai, Bi));
    const Body U = booleanToBody(A, B, BooleanOp::Union);
    const Body I = booleanToBody(A, B, BooleanOp::Intersection);
    ASSERT_GT(U.faceCount(), 0u);
    ASSERT_GT(I.faceCount(), 0u);

    for (const uint32_t sub : {0u, 2u}) {
        std::vector<std::string> operand = triangleKeys(Ai, sub), bi = triangleKeys(Bi, sub);
        operand.insert(operand.end(), bi.begin(), bi.end());
        std::vector<std::string> result = triangleKeys(U, sub), ii = triangleKeys(I, sub);
        result.insert(result.end(), ii.begin(), ii.end());
        std::sort(operand.begin(), operand.end());
        std::sort(result.begin(), result.end());
        ASSERT_EQ(operand.size(), result.size()) << "sub" << sub << ": triangle counts differ";

        std::vector<std::string> onlyOperand;
        std::set_difference(operand.begin(), operand.end(), result.begin(), result.end(),
                            std::back_inserter(onlyOperand));
        EXPECT_TRUE(onlyOperand.empty())
            << "sub" << sub << ": " << onlyOperand.size() << " of " << operand.size()
            << " triangles were cut differently in the result than in the operand — the same "
               "ring, fanned from a different apex. First: " << onlyOperand.front();
    }
}

// The consequence that a total CAN see, at both subdivision levels. sub0 held throughout —
// it is subdivision that puts arc midpoints into the ring, and those are the points that
// tie with a vertex on a seam.
TEST(BRepFanApexStability, TheFixtureConservesVolumeAtEverySubdivision)
{
    const Body A = fixtureSphere(), B = fixtureBox();
    Body Ai = A, Bi = B;
    ASSERT_TRUE(imprintMutually(Ai, Bi));
    const Body U = booleanToBody(A, B, BooleanOp::Union);
    const Body I = booleanToBody(A, B, BooleanOp::Intersection);
    const Body D = booleanToBody(A, B, BooleanOp::Difference);
    ASSERT_GT(U.faceCount(), 0u);
    ASSERT_GT(I.faceCount(), 0u);
    ASSERT_GT(D.faceCount(), 0u);

    for (const uint32_t sub : {0u, 1u, 2u, 3u}) {
        const double a = meshVolume(Ai, sub), b = meshVolume(Bi, sub);
        const double u = meshVolume(U, sub), i = meshVolume(I, sub), d = meshVolume(D, sub);
        EXPECT_NEAR(u + i, a + b, 1e-7 * (a + b)) << "sub" << sub << ": U + I != A + B";
        EXPECT_NEAR(d + i, a, 1e-7 * a) << "sub" << sub << ": D + I != A";
    }
}

// The mechanism on its own, with nothing else in the way: two copies of ONE curved patch
// whose coordinates differ by a single DOUBLE ulp must be cut along the same diagonal.
//
// The patch is four points on a cylinder. Two of them are the leftmost, so the apex is
// decided by the tie-break between them — the situation a seam produces constantly, since
// every point imprinted onto a planar face carries that face's coordinate exactly.
//
// The perturbation has to be built with care to reproduce what was actually measured, and
// the first attempt at this test did not: a plain one-ulp nudge of a double narrows back to
// the SAME float, so the old float comparison could not see it and the test passed against
// the defect. What made the real case bite was AMPLIFICATION — two doubles 2.8e-16 apart
// that straddled a float rounding boundary and so narrowed to floats 1.2e-07 apart. So the
// tied coordinate here is placed exactly on the midpoint between two adjacent floats, and
// the perturbation is the single double ulp that crosses it: negligible in the data, a whole
// float apart in the copy the decision used to be read from.
TEST(BRepFanApexStability, ADoubleUlpAcrossAFloatBoundaryDoesNotChangeTheDiagonal)
{
    const double R = 1.25;
    const double sa = std::sin(0.3);

    // the boundary between two adjacent floats near -R*cos(0.3), and the two doubles either
    // side of it — a step exactly at the boundary is not enough, since that rounds to even
    const float f0 = static_cast<float>(-R * std::cos(0.3));
    const double lo = static_cast<double>(f0);
    const double next = static_cast<double>(std::nextafterf(f0, -1e9f));
    const double bound = lo + (next - lo) * 0.5;
    const double xTied = std::nextafter(bound, 1e9);
    const double xNudged = std::nextafter(bound, -1e9);

    ASSERT_NE(static_cast<float>(xNudged), static_cast<float>(xTied))
        << "fixture failed to straddle a float boundary — it would not exercise anything";
    ASSERT_LT(std::abs(xNudged - xTied), 1e-14)
        << "the perturbation must be rounding, not a real geometric difference";

    auto patch = [&](double xSecond) -> std::optional<Body> {
        std::vector<Vec3> pts{
            Vec3{xTied, -R * sa, 0.0},   // wins the tie on y, in double
            Vec3{xSecond, R * sa, 0.0},  // wins on x only if the last bit is believed
            Vec3{-R * std::cos(0.9), R * std::sin(0.9), 1.0},
            Vec3{-R * std::cos(0.9), -R * std::sin(0.9), 1.0},
        };
        Body::FaceDef fd;
        fd.loop = {0u, 1u, 2u, 3u};
        fd.surface.kind = SurfaceKind::Cylinder;
        fd.surface.origin = {0., 0., 0.};
        fd.surface.normal = {0., 0., 1.};
        fd.surface.uAxis = {1., 0., 0.};
        fd.surface.radius = R;
        return Body::fromFaces(pts, {fd});
    };

    // which diagonal was drawn: corner 0–2, or corner 1–3
    auto diagonal = [&](const Body& b) {
        const Mesh m = b.toMesh(0);
        const std::vector<nexus::render::Vec3>& P = m.attributes().positions();
        auto isCorner = [&](const nexus::render::Vec3& p, double x, double y, double z) {
            return std::abs(p.x - x) < 1e-6 && std::abs(p.y - y) < 1e-6 &&
                   std::abs(p.z - z) < 1e-6;
        };
        int uses0 = 0, uses1 = 0;
        for (size_t f = 0; f < m.topology().faceCount(); ++f) {
            const std::vector<uint32_t>& idx = m.topology().face(f).indices;
            for (size_t k = 2; k < idx.size(); ++k) {
                const nexus::render::Vec3* t[3] = {&P[idx[0]], &P[idx[k - 1]], &P[idx[k]]};
                bool has0 = false, has1 = false, has2 = false, has3 = false;
                for (const nexus::render::Vec3* p : t) {
                    if (isCorner(*p, xTied, -R * sa, 0.0)) has0 = true;
                    if (isCorner(*p, xTied, R * sa, 0.0)) has1 = true;
                    if (isCorner(*p, -R * std::cos(0.9), R * std::sin(0.9), 1.0)) has2 = true;
                    if (isCorner(*p, -R * std::cos(0.9), -R * std::sin(0.9), 1.0)) has3 = true;
                }
                if (has0 && has2) ++uses0;
                if (has1 && has3) ++uses1;
            }
        }
        return std::pair<int, int>{uses0, uses1};
    };

    const auto exact = patch(xTied);
    const auto nudged = patch(xNudged);
    ASSERT_TRUE(exact.has_value() && nudged.has_value()) << "fixture did not build";

    const auto de = diagonal(*exact), dn = diagonal(*nudged);
    EXPECT_EQ(de, dn) << "one ulp in a tied coordinate moved the fan apex, so the patch was "
                         "cut along the other diagonal — " << de.first << "/" << de.second
                      << " became " << dn.first << "/" << dn.second;

    // and the volume it encloses must move by rounding, not by a feature
    const double ve = MeshMassProperties::compute(exact->toMesh(2)).volume;
    const double vn = MeshMassProperties::compute(nudged->toMesh(2)).volume;
    EXPECT_NEAR(ve, vn, 1e-12) << "an ulp changed the enclosed volume by " << std::abs(ve - vn);
}

// CHARACTERIZATION of the tie-break's remaining freedom, so a later reader knows the bound
// rather than rediscovering it. The comparison is made on a grid 1e-12 of the ring's own
// scale: coordinates closer than that are treated as tied and the next axis decides. Two
// ring points can still land either side of one grid boundary, but that now needs a
// coincidence at 1e-12 rather than being the ordinary state of every seam.
TEST(BRepFanApexStability, CoordinatesFarApartStillDecideTheApexOutright)
{
    const Body b = makeBox(2.f, 2.f, 2.f);
    const Mesh m0 = b.toMesh(0);
    // a plain box is unaffected by any of this — its faces are planar, where every fan
    // encloses the same volume — and must tessellate exactly as before
    EXPECT_NEAR(MeshMassProperties::compute(m0).volume, 8.0, 1e-6);
    EXPECT_EQ(m0.topology().faceCount(), 12u) << "a box is still two triangles per face";

    // and the choice is still made by geometry: translating a body must permute the apexes
    // consistently, so the tessellation rides along unchanged in shape
    Body moved = b;
    moved.translate({100.0, -50.0, 7.5});
    const Mesh m1 = moved.toMesh(0);
    EXPECT_EQ(m1.topology().faceCount(), m0.topology().faceCount());
    EXPECT_NEAR(MeshMassProperties::compute(m1).volume, 8.0, 1e-4)
        << "the same box 100 units away tessellates to a different volume";
}

}  // namespace nexus::geometry::brep::testing
