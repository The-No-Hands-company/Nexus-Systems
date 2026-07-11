#include <nexus/geometry/Mesh.h>
#include <nexus/geometry/MeshCut.h>

#include <gtest/gtest.h>

#include <cmath>

namespace nexus::geometry::testing {

using namespace nexus::geometry::primitives;
using V = nexus::render::Vec3;

namespace {

float surfaceArea(const Mesh& m)
{
    const auto& p = m.attributes().positions();
    float       s = 0.f;
    for (size_t f = 0; f < m.topology().faceCount(); ++f) {
        const Face& fc = m.topology().face(f);
        if (fc.indices.size() != 3) continue;
        const V a = p[fc.indices[0]], b = p[fc.indices[1]], c = p[fc.indices[2]];
        s += 0.5f * std::sqrt((b - a).cross(c - a).lengthSq());
    }
    return s;
}

size_t triFaceCount(const Mesh& src)
{
    Mesh t = src;
    (void)t.topology().triangulate();
    return t.topology().faceCount();
}
float triSurfaceArea(const Mesh& src)
{
    Mesh t = src;
    (void)t.topology().triangulate();
    return surfaceArea(t);
}

}  // namespace

TEST(MeshCut, OverlappingBoxesSplitAndPreserveArea)
{
    const Mesh a = makeBox(2.f, 2.f, 2.f);                          // [-1,1]^3
    Mesh       b = makeBox(2.f, 2.f, 2.f);
    // Corner overlap so the box surfaces cross non-coplanarly.
    auto bp = b.attributes().positions();
    for (auto& v : bp) { v.x += 1.f; v.y += 1.f; v.z += 1.f; }      // [0,2]^3
    b.attributes().setPositions(std::move(bp));

    const MeshCutResult r = MeshCut::cut(a, b);

    EXPECT_TRUE(r.a.isValid());
    EXPECT_TRUE(r.b.isValid());
    // The cut adds edges but keeps each surface's geometry → area preserved.
    EXPECT_NEAR(surfaceArea(r.a), triSurfaceArea(a), 0.05f);
    EXPECT_NEAR(surfaceArea(r.b), triSurfaceArea(b), 0.05f);
    // Some faces were split along the intersection curve.
    EXPECT_GT(r.a.topology().faceCount(), triFaceCount(a));
    EXPECT_GT(r.b.topology().faceCount(), triFaceCount(b));
}

TEST(MeshCut, NonOverlappingBoxesAreUnchanged)
{
    const Mesh a = makeBox(2.f, 2.f, 2.f);
    Mesh       b = makeBox(2.f, 2.f, 2.f);
    auto bp = b.attributes().positions();
    for (auto& v : bp) { v.x += 10.f; }  // far apart
    b.attributes().setPositions(std::move(bp));

    const MeshCutResult r = MeshCut::cut(a, b);
    EXPECT_EQ(r.a.topology().faceCount(), triFaceCount(a));  // nothing to cut
    EXPECT_EQ(r.b.topology().faceCount(), triFaceCount(b));
    EXPECT_NEAR(surfaceArea(r.a), triSurfaceArea(a), 1e-3f);
}

}  // namespace nexus::geometry::testing
