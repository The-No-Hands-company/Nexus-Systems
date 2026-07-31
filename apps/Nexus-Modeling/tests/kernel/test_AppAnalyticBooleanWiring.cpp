// The editor and the analytic B-rep, connected.
//
// Until now the app layer had no reference to `brep` anywhere in it: primitives were
// placed as triangles and BooleanMode cut those triangles against each other. Every
// analytic surface intersection, seam imprint and face-classification path in the kernel
// was unreachable from the UI, so improving them could not change anything a user saw.
//
// A feature now carries its analytic solid alongside its mesh. The mesh is what gets
// drawn and picked; the body is what the feature IS. When both operands of an operation
// have one, the operation runs on the real surfaces and the result is itself a solid — so
// it can feed the NEXT operation unchanged, which is what keeps a chain exact rather than
// degrading a little at every step.
//
// Two properties matter more than the happy path and are asserted below.
//
// The analytic path DECLINES rather than fails. booleanToBody holds itself to
// watertight-or-empty and returns a clean empty body for configurations it cannot sew —
// tangencies above all, where the true result is not a manifold solid at all. The mesh
// path must still produce the user's operation in that case.
//
// And a fallback must CLEAR the stale body. The body described the solid before the
// operation; the mesh path cannot update it. Left attached, it would hand the next
// operation geometry that no longer matches what is on screen, and the model would
// silently diverge from the picture of it.

#include <nexus/app/BooleanMode.h>
#include <nexus/app/PrimitiveModelingMode.h>
#include <nexus/cad/CadDocument.h>
#include <nexus/geometry/AnalyticBRep.h>
#include <nexus/parametric/ParametricSketchProfile.h>

#include <gtest/gtest.h>

#include <cmath>

namespace nexus::app::testing {

namespace {

InputEvent click()
{
    InputEvent e;
    e.type = InputEventType::MouseDown;
    e.button = 0;
    return e;
}

// Add a feature carrying both representations of the same solid.
parametric::FeatureId addSolid(cad::CadDocument& doc, geometry::brep::Body body, bool withBody)
{
    const auto id = doc.addSketch(parametric::ParametricSketchFactory::createSketch());
    auto* node = doc.history().node(id);
    if (!node) return parametric::kInvalidFeatureId;
    geometry::Mesh m = body.toMesh(0);
    (void)m.computeVertexNormals();
    node->mesh.emplace(std::move(m));
    if (withBody) node->body.emplace(std::move(body));
    node->dirty = false;
    return id;
}

// Drive BooleanMode through pick A / pick B / confirm.
void runBoolean(BooleanMode& mode, AppContext& ctx, parametric::FeatureId a,
                parametric::FeatureId b)
{
    mode.onActivate(ctx);
    ctx.activeSelectedFeature = static_cast<uint32_t>(a);
    (void)mode.handleInput(ctx, click());
    ctx.activeSelectedFeature = static_cast<uint32_t>(b);
    (void)mode.handleInput(ctx, click());
    (void)mode.handleInput(ctx, click());
}

double meshVolume(const geometry::Mesh& m)
{
    const auto& pos = m.attributes().positions();
    const auto& topo = m.topology();
    double v = 0.0;
    for (size_t f = 0; f < topo.faceCount(); ++f) {
        const auto& idx = topo.face(f).indices;
        for (size_t i = 1; i + 1 < idx.size(); ++i) {
            const auto& a = pos[idx[0]];
            const auto& b = pos[idx[i]];
            const auto& c = pos[idx[i + 1]];
            v += (static_cast<double>(a.x) * (static_cast<double>(b.y) * c.z - static_cast<double>(b.z) * c.y) -
                  static_cast<double>(a.y) * (static_cast<double>(b.x) * c.z - static_cast<double>(b.z) * c.x) +
                  static_cast<double>(a.z) * (static_cast<double>(b.x) * c.y - static_cast<double>(b.y) * c.x)) /
                 6.0;
        }
    }
    return v;
}

}  // namespace

// Placing a primitive gives the feature an analytic solid, and it must agree with the
// triangles the user is looking at — a body that described different geometry would move
// the model the moment an operation used it.
TEST(AppAnalyticBooleanWiring, PlacedPrimitiveCarriesABodyThatMatchesItsMesh)
{
    cad::CadDocument doc;
    AppContext ctx;
    ctx.document = &doc;
    ctx.snapToGrid = true;
    ctx.cursorWorldPos = {0.f, 0.f, 0.f};

    PrimitiveModelingMode mode;
    mode.onActivate(ctx);
    ASSERT_TRUE(mode.executeAction("modeling.box", ctx));
    (void)mode.handleInput(ctx, click());

    parametric::FeatureNode* placed = nullptr;
    for (uint32_t i = 1; i <= doc.history().featureCount() + 4; ++i)
        if (auto* n = doc.history().node(static_cast<parametric::FeatureId>(i)); n && n->mesh)
            placed = n;
    ASSERT_NE(placed, nullptr) << "no feature was placed";
    ASSERT_TRUE(placed->body.has_value()) << "a placed box carries no analytic body";
    EXPECT_TRUE(placed->body->isClosed());
    EXPECT_TRUE(placed->body->checkIntegrity().ok);

    // the two representations must describe the same solid
    EXPECT_NEAR(meshVolume(placed->body->toMesh(0)), meshVolume(*placed->mesh), 1e-4)
        << "the analytic body and the displayed mesh enclose different volumes";
}

// The operation runs analytically and its result is STILL a solid, so the next operation
// can consume it directly. That is the property the whole B-rep path exists for.
TEST(AppAnalyticBooleanWiring, BooleanOfTwoBodiesStaysAnalyticAndChains)
{
    cad::CadDocument doc;
    AppContext ctx;
    ctx.document = &doc;

    geometry::brep::Body cutter = geometry::brep::makeCylinder(0.5f, 4.f, 24);
    const auto a = addSolid(doc, geometry::brep::makeBox(2.f, 2.f, 2.f), true);
    const auto b = addSolid(doc, std::move(cutter), true);
    ASSERT_NE(a, parametric::kInvalidFeatureId);
    ASSERT_NE(b, parametric::kInvalidFeatureId);

    BooleanMode mode;
    ASSERT_TRUE(mode.executeAction("boolean.difference", ctx));
    runBoolean(mode, ctx, a, b);

    auto* nodeA = doc.history().node(a);
    ASSERT_NE(nodeA, nullptr);
    ASSERT_TRUE(nodeA->body.has_value())
        << "the result carries no body — the operation fell back to the mesh path";
    EXPECT_TRUE(nodeA->body->isClosed()) << "the analytic result is not watertight";
    EXPECT_TRUE(nodeA->body->checkIntegrity().ok);
    EXPECT_TRUE(nodeA->body->checkGeometry().ok);
    ASSERT_TRUE(nodeA->mesh.has_value());
    EXPECT_GT(nodeA->mesh->attributes().vertexCount(), 0u) << "nothing to draw";

    // a bore through a 2x2x2 box removes pi*r^2*h over the box's height
    const double expected = 8.0 - 3.14159265358979 * 0.25 * 2.0;
    EXPECT_NEAR(meshVolume(*nodeA->mesh), expected, 0.05)
        << "the drilled box does not have the volume the geometry dictates";

    // and the result feeds the NEXT operation as a solid
    const auto c = addSolid(doc, geometry::brep::makeBox(1.f, 1.f, 6.f), true);
    ASSERT_NE(c, parametric::kInvalidFeatureId);
    BooleanMode second;
    ASSERT_TRUE(second.executeAction("boolean.difference", ctx));
    runBoolean(second, ctx, a, c);
    auto* chained = doc.history().node(a);
    ASSERT_NE(chained, nullptr);
    EXPECT_TRUE(chained->body.has_value()) << "the chain dropped to the mesh path";
    if (chained->body) EXPECT_TRUE(chained->body->isClosed());
}

// An operand with no analytic form — a torus, an import, a sculpt — must still work, on
// the mesh path, and must not be blocked by the analytic one.
TEST(AppAnalyticBooleanWiring, OperandWithoutABodyStillOperatesViaTheMeshPath)
{
    cad::CadDocument doc;
    AppContext ctx;
    ctx.document = &doc;

    const auto a = addSolid(doc, geometry::brep::makeBox(2.f, 2.f, 2.f), true);
    const auto b = addSolid(doc, geometry::brep::makeBox(1.f, 1.f, 4.f), /*withBody=*/false);
    ASSERT_NE(a, parametric::kInvalidFeatureId);
    ASSERT_NE(b, parametric::kInvalidFeatureId);
    ASSERT_FALSE(doc.history().node(b)->body.has_value());

    BooleanMode mode;
    ASSERT_TRUE(mode.executeAction("boolean.union", ctx));
    runBoolean(mode, ctx, a, b);

    auto* nodeA = doc.history().node(a);
    ASSERT_NE(nodeA, nullptr);
    ASSERT_TRUE(nodeA->mesh.has_value()) << "the mesh path produced nothing";
    EXPECT_GT(nodeA->mesh->attributes().vertexCount(), 0u);
}

// THE correctness guard on the fallback. A body that survived a mesh-path operation would
// describe the solid as it was BEFORE it, and the next analytic operation would silently
// model geometry the user is not looking at.
TEST(AppAnalyticBooleanWiring, FallingBackToTheMeshPathClearsTheStaleBody)
{
    cad::CadDocument doc;
    AppContext ctx;
    ctx.document = &doc;

    const auto a = addSolid(doc, geometry::brep::makeBox(2.f, 2.f, 2.f), true);
    const auto b = addSolid(doc, geometry::brep::makeBox(1.f, 1.f, 4.f), /*withBody=*/false);
    auto* nodeA = doc.history().node(a);
    ASSERT_NE(nodeA, nullptr);
    ASSERT_TRUE(nodeA->body.has_value()) << "fixture must start with a body to lose";

    BooleanMode mode;
    ASSERT_TRUE(mode.executeAction("boolean.difference", ctx));
    runBoolean(mode, ctx, a, b);

    nodeA = doc.history().node(a);
    ASSERT_NE(nodeA, nullptr);
    EXPECT_FALSE(nodeA->body.has_value())
        << "a body from before the operation is still attached after the mesh path ran — "
           "the next analytic operation would use geometry that is no longer on screen";
}

// A feature with no body at all is untouched by any of this: the wiring is additive, and
// every pre-existing mesh-only workflow behaves exactly as before.
TEST(AppAnalyticBooleanWiring, MeshOnlyFeaturesAreUnaffected)
{
    cad::CadDocument doc;
    AppContext ctx;
    ctx.document = &doc;

    const auto a = addSolid(doc, geometry::brep::makeBox(2.f, 2.f, 2.f), false);
    const auto b = addSolid(doc, geometry::brep::makeBox(1.f, 1.f, 4.f), false);
    auto* nodeA = doc.history().node(a);
    ASSERT_NE(nodeA, nullptr);
    const size_t before = nodeA->mesh->attributes().vertexCount();

    BooleanMode mode;
    ASSERT_TRUE(mode.executeAction("boolean.difference", ctx));
    runBoolean(mode, ctx, a, b);

    nodeA = doc.history().node(a);
    ASSERT_NE(nodeA, nullptr);
    EXPECT_FALSE(nodeA->body.has_value());
    ASSERT_TRUE(nodeA->mesh.has_value());
    EXPECT_NE(nodeA->mesh->attributes().vertexCount(), before)
        << "the mesh path did not run at all";
}

}  // namespace nexus::app::testing
