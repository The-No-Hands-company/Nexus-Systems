#include <nexus/geometry/Mesh.h>
#include <nexus/geometry/ModifierStack.h>

#include <gtest/gtest.h>

#include <cmath>

namespace nexus::geometry::testing {

using namespace nexus::geometry::primitives;

namespace {

// Axis-aligned bounds of a mesh.
struct Bounds {
    float minX, minY, minZ, maxX, maxY, maxZ;
};
Bounds boundsOf(const Mesh& m)
{
    const auto& p = m.attributes().positions();
    Bounds b{p[0].x, p[0].y, p[0].z, p[0].x, p[0].y, p[0].z};
    for (const auto& v : p) {
        b.minX = std::min(b.minX, v.x); b.maxX = std::max(b.maxX, v.x);
        b.minY = std::min(b.minY, v.y); b.maxY = std::max(b.maxY, v.y);
        b.minZ = std::min(b.minZ, v.z); b.maxZ = std::max(b.maxZ, v.z);
    }
    return b;
}

}  // namespace

TEST(ModifierStack, EmptyStackReturnsBaseUnchanged)
{
    Mesh box = makeBox(2.f, 2.f, 2.f);
    ModifierStack stack;
    stack.setBaseMesh(box);

    const Mesh out = stack.evaluate();
    EXPECT_EQ(out.attributes().vertexCount(), box.attributes().vertexCount());
    EXPECT_EQ(out.topology().faceCount(), box.topology().faceCount());
}

TEST(ModifierStack, EvaluateIsNonDestructive)
{
    Mesh box = makeBox(2.f, 2.f, 2.f);
    const size_t baseVerts = box.attributes().vertexCount();

    ModifierStack stack;
    stack.setBaseMesh(box);
    Modifier mir;
    mir.type = ModifierType::Mirror;
    mir.axis = 0;
    stack.addModifier(mir);

    (void)stack.evaluate();
    (void)stack.evaluate();
    // Base mesh must be untouched across repeated evaluations.
    EXPECT_EQ(stack.baseMesh().attributes().vertexCount(), baseVerts);
}

TEST(ModifierStack, TranslateShiftsGeometry)
{
    Mesh box = makeBox(2.f, 2.f, 2.f);
    ModifierStack stack;
    stack.setBaseMesh(box);
    Modifier t;
    t.type = ModifierType::Translate;
    t.vec  = nexus::render::Vec3{5.f, 0.f, 0.f};
    stack.addModifier(t);

    const Bounds b0 = boundsOf(box);
    const Bounds b1 = boundsOf(stack.evaluate());
    EXPECT_NEAR(b1.minX, b0.minX + 5.f, 1e-3f);
    EXPECT_NEAR(b1.maxX, b0.maxX + 5.f, 1e-3f);
}

TEST(ModifierStack, ScaleGrowsBounds)
{
    Mesh box = makeBox(2.f, 2.f, 2.f);
    ModifierStack stack;
    stack.setBaseMesh(box);
    Modifier s;
    s.type = ModifierType::Scale;
    s.vec  = nexus::render::Vec3{3.f, 1.f, 1.f};
    stack.addModifier(s);

    const Bounds b0 = boundsOf(box);
    const Bounds b1 = boundsOf(stack.evaluate());
    EXPECT_NEAR((b1.maxX - b1.minX), (b0.maxX - b0.minX) * 3.f, 1e-3f);
    EXPECT_NEAR((b1.maxY - b1.minY), (b0.maxY - b0.minY), 1e-3f);
}

TEST(ModifierStack, MirrorRoughlyDoublesGeometry)
{
    // Offset the box off-axis so the mirror produces a distinct reflected copy.
    Mesh box = makeBox(2.f, 2.f, 2.f);
    ModifierStack stack;
    stack.setBaseMesh(box);
    Modifier t;
    t.type = ModifierType::Translate;
    t.vec  = nexus::render::Vec3{3.f, 0.f, 0.f};
    stack.addModifier(t);
    Modifier mir;
    mir.type  = ModifierType::Mirror;
    mir.axis  = 0;
    mir.merge = false;  // no welding -> exact double
    stack.addModifier(mir);

    const Mesh out = stack.evaluate();
    EXPECT_EQ(out.topology().faceCount(), box.topology().faceCount() * 2u);
    // Mirror across X spans both sides of the plane.
    const Bounds b = boundsOf(out);
    EXPECT_LT(b.minX, 0.f);
    EXPECT_GT(b.maxX, 0.f);
}

TEST(ModifierStack, SolidifyAddsShellFaces)
{
    Mesh plane = makePlane(4.f, 4.f, 1u, 1u);
    const size_t baseFaces = plane.topology().faceCount();

    ModifierStack stack;
    stack.setBaseMesh(plane);
    Modifier sol;
    sol.type   = ModifierType::Solidify;
    sol.scalar = 0.5f;
    stack.addModifier(sol);

    const Mesh out = stack.evaluate();
    EXPECT_GT(out.topology().faceCount(), baseFaces);  // shelled -> more faces
    EXPECT_TRUE(out.isValid());
}

TEST(ModifierStack, DisabledModifierIsSkipped)
{
    Mesh box = makeBox(2.f, 2.f, 2.f);
    ModifierStack stack;
    stack.setBaseMesh(box);
    Modifier t;
    t.type = ModifierType::Translate;
    t.vec  = nexus::render::Vec3{10.f, 0.f, 0.f};
    stack.addModifier(t);
    ASSERT_TRUE(stack.setEnabled(0, false));

    const Bounds b0 = boundsOf(box);
    const Bounds b1 = boundsOf(stack.evaluate());
    EXPECT_NEAR(b1.minX, b0.minX, 1e-3f);  // disabled -> no shift
    EXPECT_NEAR(b1.maxX, b0.maxX, 1e-3f);
}

TEST(ModifierStack, OrderMattersAndReorderWorks)
{
    Mesh box = makeBox(2.f, 2.f, 2.f);
    ModifierStack stack;
    stack.setBaseMesh(box);
    // scale-then-translate vs translate-then-scale give different results.
    Modifier s; s.type = ModifierType::Scale;     s.vec = nexus::render::Vec3{2.f, 1.f, 1.f};
    Modifier t; t.type = ModifierType::Translate; t.vec = nexus::render::Vec3{5.f, 0.f, 0.f};
    stack.addModifier(t);
    stack.addModifier(s);
    const Bounds tThenS = boundsOf(stack.evaluate());  // translate first: (x+5)*2

    ASSERT_TRUE(stack.moveModifier(1, 0));  // now scale first: x*2 + 5
    const Bounds sThenT = boundsOf(stack.evaluate());
    EXPECT_GT(tThenS.maxX, sThenT.maxX);    // (x+5)*2 pushes farther than x*2+5
}

TEST(ModifierStack, RemoveAndClear)
{
    Mesh box = makeBox(2.f, 2.f, 2.f);
    ModifierStack stack;
    stack.setBaseMesh(box);
    Modifier t; t.type = ModifierType::Translate; t.vec = nexus::render::Vec3{1.f, 0.f, 0.f};
    stack.addModifier(t);
    stack.addModifier(t);
    EXPECT_EQ(stack.modifierCount(), 2u);
    EXPECT_TRUE(stack.removeModifier(0));
    EXPECT_EQ(stack.modifierCount(), 1u);
    EXPECT_FALSE(stack.removeModifier(5));
    stack.clearModifiers();
    EXPECT_EQ(stack.modifierCount(), 0u);
}

}  // namespace nexus::geometry::testing
