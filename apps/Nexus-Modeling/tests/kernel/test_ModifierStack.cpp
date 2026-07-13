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

// Foundation sweep: the evaluated result carries stable element IDs, whether
// the stack is empty or ran modifiers (was previously skipped).
TEST(ModifierStack, EvaluatedResultHasStableElementIds)
{
    Mesh box = makeBox(2.f, 2.f, 2.f);
    ModifierStack empty;
    empty.setBaseMesh(box);
    EXPECT_TRUE(empty.evaluate().hasStableElementIds());

    ModifierStack withMod;
    withMod.setBaseMesh(box);
    Modifier solid;
    solid.type = ModifierType::Solidify;
    solid.scalar = 0.1f;
    withMod.addModifier(solid);
    EXPECT_TRUE(withMod.evaluate().hasStableElementIds());
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

TEST(ModifierStack, ArrayCreatesCopies)
{
    Mesh box = makeBox(2.f, 2.f, 2.f);
    ModifierStack stack;
    stack.setBaseMesh(box);
    Modifier arr;
    arr.type  = ModifierType::Array;
    arr.count = 3;
    arr.vec   = nexus::render::Vec3{5.f, 0.f, 0.f};
    arr.merge = false;  // exact triple
    stack.addModifier(arr);

    const Mesh out = stack.evaluate();
    EXPECT_EQ(out.topology().faceCount(), box.topology().faceCount() * 3u);
    const Bounds b = boundsOf(out);
    EXPECT_NEAR(b.maxX - b.minX, (boundsOf(box).maxX - boundsOf(box).minX) + 10.f, 1e-3f);
}

TEST(ModifierStack, ArrayCountOneIsIdentity)
{
    Mesh box = makeBox(2.f, 2.f, 2.f);
    ModifierStack stack;
    stack.setBaseMesh(box);
    Modifier arr;
    arr.type  = ModifierType::Array;
    arr.count = 1;
    arr.vec   = nexus::render::Vec3{5.f, 0.f, 0.f};
    stack.addModifier(arr);

    EXPECT_EQ(stack.evaluate().topology().faceCount(), box.topology().faceCount());
}

TEST(ModifierStack, DisplacePushesAlongNormals)
{
    Mesh sphere = makeSphere(2.f);
    ModifierStack stack;
    stack.setBaseMesh(sphere);
    Modifier d;
    d.type   = ModifierType::Displace;
    d.scalar = 0.5f;
    stack.addModifier(d);

    const Bounds b0 = boundsOf(sphere);
    const Bounds b1 = boundsOf(stack.evaluate());
    // Outward normal displacement grows the radius.
    EXPECT_GT(b1.maxX, b0.maxX);
    EXPECT_LT(b1.minX, b0.minX);
    EXPECT_TRUE(stack.evaluate().isValid());
}

TEST(ModifierStack, EvaluatedIsCachedAndInvalidatedOnChange)
{
    Mesh box = makeBox(2.f, 2.f, 2.f);
    ModifierStack stack;
    stack.setBaseMesh(box);

    const Mesh& a = stack.evaluated();
    const Mesh& b = stack.evaluated();
    EXPECT_EQ(&a, &b);  // same cached object returned without recompute
    const size_t baseFaces = a.topology().faceCount();

    Modifier arr;
    arr.type  = ModifierType::Array;
    arr.count = 2;
    arr.vec   = nexus::render::Vec3{5.f, 0.f, 0.f};
    arr.merge = false;
    stack.addModifier(arr);  // must invalidate the cache
    EXPECT_EQ(stack.evaluated().topology().faceCount(), baseFaces * 2u);
}

TEST(ModifierStack, SetModifierUpdatesParams)
{
    Mesh box = makeBox(2.f, 2.f, 2.f);
    ModifierStack stack;
    stack.setBaseMesh(box);
    Modifier t;
    t.type = ModifierType::Translate;
    t.vec  = nexus::render::Vec3{1.f, 0.f, 0.f};
    stack.addModifier(t);

    Modifier t2 = t;
    t2.vec = nexus::render::Vec3{9.f, 0.f, 0.f};
    ASSERT_TRUE(stack.setModifier(0, t2));
    EXPECT_FALSE(stack.setModifier(3, t2));

    const Bounds b = boundsOf(stack.evaluate());
    EXPECT_NEAR(b.minX, boundsOf(box).minX + 9.f, 1e-3f);
}

TEST(ModifierStack, SerializeRoundTrip)
{
    ModifierStack src;
    src.setBaseMesh(makeBox(2.f, 2.f, 2.f));
    Modifier a; a.type = ModifierType::Translate; a.vec = nexus::render::Vec3{5.f, 0.f, 0.f};
    Modifier b; b.type = ModifierType::Array; b.count = 3; b.vec = nexus::render::Vec3{2.f, 0.f, 0.f}; b.merge = false;
    Modifier c; c.type = ModifierType::Mirror; c.axis = 1; c.enabled = false;
    src.addModifier(a); src.addModifier(b); src.addModifier(c);

    const std::vector<uint8_t> blob = serializeModifierStack(src);
    ASSERT_FALSE(blob.empty());

    ModifierStack dst;
    dst.setBaseMesh(makeBox(2.f, 2.f, 2.f));
    ASSERT_TRUE(deserializeModifierStack(blob.data(), blob.size(), dst));
    ASSERT_EQ(dst.modifierCount(), 3u);
    EXPECT_EQ(dst.modifierAt(0).type, ModifierType::Translate);
    EXPECT_NEAR(dst.modifierAt(0).vec.x, 5.f, 1e-6f);
    EXPECT_EQ(dst.modifierAt(1).type, ModifierType::Array);
    EXPECT_EQ(dst.modifierAt(1).count, 3);
    EXPECT_FALSE(dst.modifierAt(1).merge);
    EXPECT_EQ(dst.modifierAt(2).type, ModifierType::Mirror);
    EXPECT_EQ(dst.modifierAt(2).axis, 1);
    EXPECT_FALSE(dst.modifierAt(2).enabled);
    EXPECT_EQ(src.evaluate().topology().faceCount(), dst.evaluate().topology().faceCount());
}

TEST(ModifierStack, SerializeIsDeterministic)
{
    ModifierStack s;
    s.setBaseMesh(makeBox(2.f, 2.f, 2.f));
    Modifier a; a.type = ModifierType::Displace; a.scalar = 0.3f;
    s.addModifier(a);
    EXPECT_EQ(serializeModifierStack(s), serializeModifierStack(s));
}

TEST(ModifierStack, DeserializePreservesBaseMesh)
{
    ModifierStack src;
    src.setBaseMesh(makeBox(2.f, 2.f, 2.f));
    Modifier a; a.type = ModifierType::Translate; a.vec = nexus::render::Vec3{1.f, 0.f, 0.f};
    src.addModifier(a);
    const std::vector<uint8_t> blob = serializeModifierStack(src);

    ModifierStack dst;
    dst.setBaseMesh(makeSphere(3.f));
    const size_t baseVerts = dst.baseMesh().attributes().vertexCount();
    ASSERT_TRUE(deserializeModifierStack(blob.data(), blob.size(), dst));
    EXPECT_EQ(dst.baseMesh().attributes().vertexCount(), baseVerts);  // base left untouched
    EXPECT_EQ(dst.modifierCount(), 1u);
}

TEST(ModifierStack, DeserializeRejectsTruncatedNullAndBadVersion)
{
    ModifierStack src;
    src.setBaseMesh(makeBox(2.f, 2.f, 2.f));
    Modifier a; a.type = ModifierType::Translate; a.vec = nexus::render::Vec3{1.f, 0.f, 0.f};
    src.addModifier(a);
    const std::vector<uint8_t> blob = serializeModifierStack(src);

    ModifierStack dst;
    dst.setBaseMesh(makeBox(2.f, 2.f, 2.f));
    Modifier keep; keep.type = ModifierType::Scale; keep.vec = nexus::render::Vec3{2.f, 2.f, 2.f};
    dst.addModifier(keep);

    EXPECT_FALSE(deserializeModifierStack(blob.data(), blob.size() / 2, dst));  // truncated
    EXPECT_EQ(dst.modifierCount(), 1u);                                         // unchanged on failure
    EXPECT_FALSE(deserializeModifierStack(nullptr, 0, dst));

    std::vector<uint8_t> bad = blob;
    bad[0] = bad[1] = bad[2] = bad[3] = 0;  // version 0
    EXPECT_FALSE(deserializeModifierStack(bad.data(), bad.size(), dst));
    EXPECT_EQ(dst.modifierCount(), 1u);
}

}  // namespace nexus::geometry::testing
