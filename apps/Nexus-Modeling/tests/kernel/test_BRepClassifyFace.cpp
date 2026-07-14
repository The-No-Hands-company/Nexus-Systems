// Foundation — analytic B-rep face-vs-solid classification (the boolean's
// per-face keep/discard decision). Body::classifyFace samples a face's centroid
// and classifies it against another solid; faceCentroid exposes the sample
// point. Proven on overlapping boxes: faces inside the other solid classify
// Inside, faces outside classify Outside, a coincident face OnBoundary.

#include <nexus/geometry/AnalyticBRep.h>

#include <gtest/gtest.h>

namespace nexus::geometry::brep::testing {

using nexus::render::Vec3;
using PC = Body::PointContainment;

namespace {
Vec3 sub(Vec3 a, Vec3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
Vec3 cross(Vec3 a, Vec3 b) { return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; }
Vec3 norm(Vec3 a)
{
    const float l = std::sqrt(a.x * a.x + a.y * a.y + a.z * a.z);
    return l > 1e-20f ? Vec3{a.x / l, a.y / l, a.z / l} : Vec3{0, 0, 0};
}

// An axis-aligned box of the given size centred at `c` (a correctly-built Body
// with valid per-edge Line curves — not a vertex-translated one).
Body boxAt(Vec3 c, float w, float h, float d)
{
    const float hw = w * 0.5f, hh = h * 0.5f, hd = d * 0.5f;
    const std::vector<Vec3> pts = {
        {c.x - hw, c.y - hh, c.z - hd}, {c.x + hw, c.y - hh, c.z - hd},
        {c.x + hw, c.y + hh, c.z - hd}, {c.x - hw, c.y + hh, c.z - hd},
        {c.x - hw, c.y - hh, c.z + hd}, {c.x + hw, c.y - hh, c.z + hd},
        {c.x + hw, c.y + hh, c.z + hd}, {c.x - hw, c.y + hh, c.z + hd},
    };
    const uint32_t rings[6][4] = {
        {0, 3, 2, 1}, {4, 5, 6, 7}, {0, 1, 5, 4}, {2, 3, 7, 6}, {0, 4, 7, 3}, {1, 2, 6, 5},
    };
    std::vector<Body::FaceDef> defs;
    for (const auto& r : rings) {
        Body::FaceDef fd;
        fd.loop = {r[0], r[1], r[2], r[3]};
        Surface s;
        s.kind = SurfaceKind::Plane;
        s.origin = pts[r[0]];
        s.normal = norm(cross(sub(pts[r[1]], pts[r[0]]), sub(pts[r[3]], pts[r[0]])));
        s.uAxis = norm(sub(pts[r[1]], pts[r[0]]));
        fd.surface = s;
        defs.push_back(std::move(fd));
    }
    auto b = Body::fromFaces(pts, defs);
    return b.has_value() ? std::move(*b) : Body{};
}
}  // namespace

TEST(BRepClassifyFace, FaceCentroidIsOnThePlanarFace)
{
    const Body a = makeBox(2.f, 2.f, 2.f);  // [-1,1]^3
    // Each face's centroid must lie on that face's plane (|coord| == 1 on one axis).
    for (uint32_t f = 0; f < a.faceCount(); ++f) {
        const Vec3 c = a.faceCentroid(f);
        const bool onAFace = std::abs(std::abs(c.x) - 1.f) < 1e-5f ||
                             std::abs(std::abs(c.y) - 1.f) < 1e-5f ||
                             std::abs(std::abs(c.z) - 1.f) < 1e-5f;
        EXPECT_TRUE(onAFace) << "face " << f;
        // Its own solid classifies its face centroid as OnBoundary.
        EXPECT_EQ(a.classifyFace(f, a), PC::OnBoundary) << "face " << f;
    }
}

TEST(BRepClassifyFace, OverlappingBoxesSplitInsideOutside)
{
    const Body a = makeBox(2.f, 2.f, 2.f);          // [-1,1]^3
    const Body b = boxAt({1.5f, 0.f, 0.f}, 2, 2, 2);  // [0.5,2.5] x [-1,1]^2

    int inside = 0, outside = 0, boundary = 0;
    uint32_t plusXFace = kInvalid;
    for (uint32_t f = 0; f < a.faceCount(); ++f) {
        const PC c = a.classifyFace(f, b);
        inside += (c == PC::Inside);
        outside += (c == PC::Outside);
        boundary += (c == PC::OnBoundary);
        if (std::abs(a.faceCentroid(f).x - 1.f) < 1e-5f) plusXFace = f;
    }
    // Only A's +X face centroid (1,0,0) sits inside B; the rest are outside it.
    ASSERT_NE(plusXFace, kInvalid);
    EXPECT_EQ(a.classifyFace(plusXFace, b), PC::Inside);
    EXPECT_EQ(inside, 1);
    EXPECT_EQ(outside, 5);
    EXPECT_EQ(boundary, 0);
}

TEST(BRepClassifyFace, DisjointSolidAllFacesOutside)
{
    const Body a = makeBox(2.f, 2.f, 2.f);
    const Body b = boxAt({10.f, 0.f, 0.f}, 2, 2, 2);  // far away
    for (uint32_t f = 0; f < a.faceCount(); ++f)
        EXPECT_EQ(a.classifyFace(f, b), PC::Outside) << "face " << f;
}

TEST(BRepClassifyFace, CoincidentFaceIsOnBoundary)
{
    const Body a = makeBox(2.f, 2.f, 2.f);            // [-1,1]^3
    const Body b = boxAt({2.f, 0.f, 0.f}, 2, 2, 2);  // [1,3] x [-1,1]^2, shares x=1 face
    // A's +X face centroid (1,0,0) lies exactly on B's -X face.
    uint32_t plusXFace = kInvalid;
    for (uint32_t f = 0; f < a.faceCount(); ++f)
        if (std::abs(a.faceCentroid(f).x - 1.f) < 1e-5f) plusXFace = f;
    ASSERT_NE(plusXFace, kInvalid);
    EXPECT_EQ(a.classifyFace(plusXFace, b), PC::OnBoundary);
}

TEST(BRepClassifyFace, Deterministic)
{
    const Body a = makeBox(2.f, 2.f, 2.f);
    const Body b = boxAt({1.5f, 0.3f, 0.2f}, 2, 2, 2);
    for (uint32_t f = 0; f < a.faceCount(); ++f) {
        const PC first = a.classifyFace(f, b);
        for (int r = 0; r < 4; ++r) EXPECT_EQ(a.classifyFace(f, b), first) << "face " << f;
    }
}

}  // namespace nexus::geometry::brep::testing
