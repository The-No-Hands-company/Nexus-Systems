#include <gtest/gtest.h>

#include <nexus/app/AppMode.h>
#include <nexus/app/ArrayMode.h>
#include <nexus/app/BevelMode.h>
#include <nexus/app/BooleanMode.h>
#include <nexus/app/LatheMode.h>
#include <nexus/app/MergeMode.h>
#include <nexus/app/ModelingApplication.h>
#include <nexus/cad/CadSelection.h>
#include <nexus/geometry/Mesh.h>
#include <nexus/parametric/FeatureHistory.h>

#include <memory>

using namespace nexus::app;
using namespace nexus::cad;
using namespace nexus::geometry;
using namespace nexus::parametric;

// ── New mode registration tests ──────────────────────────────────────────

TEST(ModeTools, NewModesAreRegistered)
{
    auto& reg = ModeRegistry::instance();
    EXPECT_TRUE(reg.isRegistered("bevel"));
    EXPECT_TRUE(reg.isRegistered("lathe"));
    EXPECT_TRUE(reg.isRegistered("array"));
    EXPECT_TRUE(reg.isRegistered("merge"));
}

TEST(ModeTools, NewModesCreateValidInstances)
{
    auto& reg = ModeRegistry::instance();
    for(const auto& id : {"bevel", "lathe", "array", "merge"}) {
        auto mode = reg.create(id);
        ASSERT_NE(mode, nullptr) << "Failed to create mode: " << id;
        EXPECT_EQ(mode->modeId(), id);
    }
}

TEST(ModeTools, NewModesProvideDisplayNames)
{
    auto& reg = ModeRegistry::instance();
    EXPECT_EQ(reg.create("bevel")->displayName(), "Bevel");
    EXPECT_EQ(reg.create("lathe")->displayName(), "Lathe");
    EXPECT_EQ(reg.create("array")->displayName(), "Array");
    EXPECT_EQ(reg.create("merge")->displayName(), "Merge");
}

TEST(ModeTools, NewModesHaveActions)
{
    auto& reg = ModeRegistry::instance();
    EXPECT_GT(reg.create("bevel")->actions().size(), 0u);
    EXPECT_GT(reg.create("lathe")->actions().size(), 0u);
    EXPECT_GT(reg.create("array")->actions().size(), 0u);
    EXPECT_GT(reg.create("merge")->actions().size(), 0u);
}

// ── BooleanMode action consistency ──────────────────────────────────────

TEST(ModeTools, BooleanModeActions)
{
    auto& reg = ModeRegistry::instance();
    auto mode = reg.create("boolean");
    auto actions = mode->actions();
    EXPECT_EQ(actions.size(), 3u);
    EXPECT_EQ(actions[0].id, "bool.union");
    EXPECT_EQ(actions[1].id, "bool.diff");
    EXPECT_EQ(actions[2].id, "bool.isect");
}

// ── Orchestrator can switch to new modes ───────────────────────────────

TEST(ModeTools, OrchestratorSwitchesToNewModes)
{
    CadDocument doc;
    CadSelection sel;
    AppContext ctx;
    ctx.document = &doc;
    ctx.selection = &sel;
    ModeOrchestrator orch(ctx);
    orch.registerBuiltinModes();

    EXPECT_TRUE(orch.switchTo("bevel"));
    EXPECT_EQ(orch.activeModeId(), "bevel");

    EXPECT_TRUE(orch.switchTo("lathe"));
    EXPECT_EQ(orch.activeModeId(), "lathe");

    EXPECT_TRUE(orch.switchTo("array"));
    EXPECT_EQ(orch.activeModeId(), "array");

    EXPECT_TRUE(orch.switchTo("merge"));
    EXPECT_EQ(orch.activeModeId(), "merge");
}

// ── CadSelection multi-selection tests ──────────────────────────────────

TEST(CadSelection, SingleFeatureSelection)
{
    CadSelection sel;
    EXPECT_TRUE(sel.empty());
    EXPECT_EQ(sel.count(), 0u);

    sel.setSingleFeature(42);
    EXPECT_FALSE(sel.empty());
    EXPECT_EQ(sel.count(), 1u);
    EXPECT_TRUE(sel.hasFeature(42));
    EXPECT_FALSE(sel.hasFeature(99));

    auto ids = sel.featureIds();
    ASSERT_EQ(ids.size(), 1u);
    EXPECT_EQ(ids[0], 42u);
}

TEST(CadSelection, MultiFeatureSelection)
{
    CadSelection sel;
    sel.selectFeature(10, false); // single set
    sel.selectFeature(20, true);  // shift-add
    sel.selectFeature(30, true);  // shift-add
    EXPECT_EQ(sel.count(), 3u);
    EXPECT_TRUE(sel.hasFeature(10));
    EXPECT_TRUE(sel.hasFeature(20));
    EXPECT_TRUE(sel.hasFeature(30));

    auto ids = sel.featureIds();
    EXPECT_EQ(ids.size(), 3u);
}

TEST(CadSelection, ToggleFeature)
{
    CadSelection sel;
    sel.addFeature(55);
    EXPECT_TRUE(sel.hasFeature(55));
    sel.toggleFeature(55);
    EXPECT_FALSE(sel.hasFeature(55));
    EXPECT_TRUE(sel.empty());

    sel.toggleFeature(55);
    EXPECT_TRUE(sel.hasFeature(55));
    EXPECT_EQ(sel.count(), 1u);
}

TEST(CadSelection, ClearSelection)
{
    CadSelection sel;
    sel.addFeature(1);
    sel.addFeature(2);
    sel.addFeature(3);
    EXPECT_EQ(sel.count(), 3u);
    sel.clear();
    EXPECT_TRUE(sel.empty());
    EXPECT_EQ(sel.count(), 0u);
}

TEST(CadSelection, RemoveFeature)
{
    CadSelection sel;
    sel.addFeature(100);
    sel.addFeature(200);
    EXPECT_TRUE(sel.hasFeature(100));
    sel.removeFeature(100);
    EXPECT_FALSE(sel.hasFeature(100));
    EXPECT_TRUE(sel.hasFeature(200));
    EXPECT_EQ(sel.count(), 1u);
}

TEST(CadSelection, SelectFeatureClearsPrevious)
{
    CadSelection sel;
    sel.addFeature(1);
    sel.addFeature(2);
    sel.selectFeature(3, false); // no shift = replace
    EXPECT_EQ(sel.count(), 1u);
    EXPECT_TRUE(sel.hasFeature(3));
    EXPECT_FALSE(sel.hasFeature(1));
}

TEST(CadSelection, DuplicateFeatureIgnored)
{
    CadSelection sel;
    sel.addFeature(42);
    sel.addFeature(42); // duplicate
    EXPECT_EQ(sel.count(), 1u);
}

TEST(CadSelection, SelectionItemsReflectFeatures)
{
    CadSelection sel;
    sel.addFeature(7);
    sel.addFeature(13);
    const auto& items = sel.items();
    ASSERT_EQ(items.size(), 2u);
    EXPECT_EQ(items[0].kind, SelectionKind::Feature);
    EXPECT_EQ(items[0].index, 7u);
    EXPECT_EQ(items[1].kind, SelectionKind::Feature);
    EXPECT_EQ(items[1].index, 13u);
}

// ── CadSelection sub-object selection (backward compat) ─────────────────

TEST(CadSelection, SubObjectSelection)
{
    CadSelection sel;
    sel.addFace(0);
    sel.addEdge(5);
    sel.addVertex(10);
    sel.addSketchEntity(2);
    EXPECT_EQ(sel.count(), 4u);
    EXPECT_EQ(sel.items()[0].kind, SelectionKind::Face);
    EXPECT_EQ(sel.items()[1].kind, SelectionKind::Edge);
    EXPECT_EQ(sel.items()[2].kind, SelectionKind::Vertex);
    EXPECT_EQ(sel.items()[3].kind, SelectionKind::SketchEntity);
}

// ── Mode lifecycle ─────────────────────────────────────────────────────

TEST(ModeTools, NewModesDeactivateGracefully)
{
    CadDocument doc;
    CadSelection sel;
    AppContext ctx;
    ctx.document = &doc;
    ctx.selection = &sel;
    ModeOrchestrator orch(ctx);
    orch.registerBuiltinModes();

    orch.switchTo("bevel");
    EXPECT_TRUE(orch.switchTo("select")); // deactivate bevel, activates select
    EXPECT_EQ(orch.activeModeId(), "select");
}
