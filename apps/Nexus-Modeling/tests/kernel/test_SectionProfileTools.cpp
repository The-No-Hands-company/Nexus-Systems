#include <gtest/gtest.h>

#include <nexus/geometry/SectionTool.h>
#include <nexus/geometry/ProfileTool.h>
#include <nexus/geometry/Mesh.h>

#include <algorithm>
#include <cmath>
#include <vector>

namespace {

using namespace nexus::geometry;
using nexus::render::Vec3;

// Does `pts` contain a point near (x,y,z)?
bool hasPoint(const std::vector<Vec3>& pts, float x, float y, float z, float tol = 1e-4f) {
    for (const auto& p : pts)
        if (std::abs(p.x - x) < tol && std::abs(p.y - y) < tol && std::abs(p.z - z) < tol)
            return true;
    return false;
}

} // namespace

// The z = 0 plane cuts a side-2 quad box through its four vertical edges. Each edge is
// shared by two side faces, so a naive per-face scan reports every point twice; the section
// must dedupe to exactly the four corner points (+/-1, +/-1, 0).
TEST(SectionTool, BoxCrossSectionHasNoDuplicatePoints) {
    Mesh box = primitives::makeBox(2.f, 2.f, 2.f);  // quads, corners at +/-1
    auto section = SectionTool::computeSection(box, {0.f, 0.f, 0.f}, {0.f, 0.f, 1.f});

    EXPECT_EQ(section.size(), 4u) << "each shared cut edge must contribute exactly one point";
    for (const auto& p : section) EXPECT_NEAR(p.z, 0.f, 1e-5f);
    EXPECT_TRUE(hasPoint(section, 1.f, 1.f, 0.f));
    EXPECT_TRUE(hasPoint(section, 1.f, -1.f, 0.f));
    EXPECT_TRUE(hasPoint(section, -1.f, 1.f, 0.f));
    EXPECT_TRUE(hasPoint(section, -1.f, -1.f, 0.f));
}

// With duplicates removed and the points angularly ordered, consecutive points trace the
// square boundary: perimeter = 4 sides x length 2 = 8, and no zero-length segment.
TEST(SectionTool, BoxCrossSectionFormsAClosedSquareLoop) {
    Mesh box = primitives::makeBox(2.f, 2.f, 2.f);
    auto s = SectionTool::computeSection(box, {0.f, 0.f, 0.f}, {0.f, 0.f, 1.f});
    ASSERT_EQ(s.size(), 4u);

    double perim = 0.0;
    for (size_t i = 0; i < s.size(); ++i) {
        const Vec3& a = s[i];
        const Vec3& b = s[(i + 1) % s.size()];
        const double d = std::sqrt(double(a.x - b.x) * (a.x - b.x)
                                 + double(a.y - b.y) * (a.y - b.y)
                                 + double(a.z - b.z) * (a.z - b.z));
        EXPECT_GT(d, 1e-6) << "angular ordering left a zero-length (duplicate) segment";
        perim += d;
    }
    EXPECT_NEAR(perim, 8.0, 1e-4);
}

TEST(SectionTool, PlaneMissingTheMeshYieldsEmptySection) {
    Mesh box = primitives::makeBox(2.f, 2.f, 2.f);
    auto section = SectionTool::computeSection(box, {0.f, 0.f, 5.f}, {0.f, 0.f, 1.f});
    EXPECT_TRUE(section.empty());
}

TEST(SectionTool, MultiSectionProducesOnePerCuttingPlane) {
    Mesh box = primitives::makeBox(2.f, 2.f, 2.f);
    // Planes at z = -0.6, 0, 0.6 all cut the box; z outside +/-1 would not.
    auto sections = SectionTool::multiSection(box, {0.f, 0.f, -0.6f}, {0.f, 0.f, 1.f}, 3, 0.6f);
    EXPECT_EQ(sections.size(), 3u);
    for (const auto& s : sections) EXPECT_EQ(s.size(), 4u);
}

// ProfileTool shares the plane-intersection core and must likewise not double its points.
TEST(ProfileTool, BoxProfileHasNoDuplicatePoints) {
    Mesh box = primitives::makeBox(2.f, 2.f, 2.f);
    auto profile = ProfileTool::extractProfile(box, {0.f, 0.f, 0.f}, {0.f, 0.f, 1.f});

    EXPECT_EQ(profile.size(), 4u);
    EXPECT_TRUE(hasPoint(profile, 1.f, 1.f, 0.f));
    EXPECT_TRUE(hasPoint(profile, 1.f, -1.f, 0.f));
    EXPECT_TRUE(hasPoint(profile, -1.f, 1.f, 0.f));
    EXPECT_TRUE(hasPoint(profile, -1.f, -1.f, 0.f));
}
