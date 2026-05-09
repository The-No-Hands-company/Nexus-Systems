#include <nexus/automation/AutomationScript.h>

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <sstream>

namespace nexus::automation::testing {

namespace fs = std::filesystem;

namespace {

[[nodiscard]] fs::path tempPath(const std::string& name)
{
    return fs::temp_directory_path() / ("nexus_automation_" + name);
}

void writeScriptFile(const fs::path& path, const std::string& script)
{
    std::ofstream out(path, std::ios::trunc);
    ASSERT_TRUE(out.good());
    out << script;
    ASSERT_TRUE(out.good());
}

} // namespace

TEST(AutomationScript, DefaultRegistryExposesBuiltins)
{
    ScriptBatchHarness harness;
    EXPECT_TRUE(harness.registry().hasCommand("mesh.make_triangle"));
    EXPECT_TRUE(harness.registry().hasCommand("scene.save"));
    EXPECT_TRUE(harness.registry().hasCommand("render.create_null_context"));
    EXPECT_TRUE(harness.registry().hasCommand("animation.add_bone"));
}

TEST(AutomationScript, UnknownCommandFailsDeterministically)
{
    ScriptBatchHarness harness;
    ScriptContext context;
    const ScriptRunReport report = harness.runScript("does_not_exist\n", context);
    EXPECT_FALSE(report.valid);
    ASSERT_FALSE(report.messages.empty());
    EXPECT_NE(report.messages.front().find("unknown command"), std::string::npos);
}

TEST(AutomationScript, ScriptPipelineRunsFromFile)
{
    const fs::path fixtureRoot = fs::path(NEXUS_TESTS_ROOT) / "kernel" / "fixtures";
    const fs::path sourceScene = fixtureRoot / "scene_asset_v0_minimal.nxas";

    const fs::path outDir = tempPath("pipeline");
    const fs::path outMesh = outDir / "script_triangle.obj";
    const fs::path outScene = outDir / "script_scene.nxas";
    const fs::path outText = outDir / "script_scene.txt";
    const fs::path scriptPath = outDir / "pipeline.nxscript";

    fs::create_directories(outDir);

    std::ostringstream script;
    script
        << "render.create_null_context width=640 height=360\n"
        << "render.describe\n"
        << "render.plan_frame\n"
        << "animation.add_bone name=root parent=-1 tx=0 ty=0 tz=0\n"
        << "animation.reset_pose\n"
        << "animation.sample_bind_pose\n"
        << "mesh.make_triangle size=2.0 name=script_triangle\n"
        << "mesh.compute_normals\n"
        << "mesh.transform tx=0 ty=0 tz=-2 sx=1 sy=1 sz=1\n"
        << "scene.new name=scripted_pipeline\n"
        << "scene.load path=" << sourceScene.string() << "\n"
        << "scene.add_mesh_entry name=script_triangle source=test://scripted\n"
        << "scene.save path=" << outScene.string() << "\n"
        << "scene.export_text path=" << outText.string() << "\n"
        << "mesh.export path=" << outMesh.string() << " format=obj\n";
    writeScriptFile(scriptPath, script.str());

    ScriptBatchHarness harness;
    ScriptContext context;
    const ScriptRunReport report = harness.runScriptFile(scriptPath, context);

    EXPECT_TRUE(report.valid);
    EXPECT_TRUE(context.renderContext != nullptr);
    EXPECT_TRUE(context.hasMesh);
    EXPECT_TRUE(context.hasScene);
    EXPECT_TRUE(context.hasSkeleton);
    EXPECT_EQ(context.scene.entryCount(), 2u);
    EXPECT_EQ(context.scene.sceneName(), "legacy_scene");
    EXPECT_EQ(context.skeleton.boneCount(), 1u);

    EXPECT_TRUE(fs::exists(outMesh));
    EXPECT_TRUE(fs::exists(outScene));
    EXPECT_TRUE(fs::exists(outText));

    std::ifstream sceneText(outText);
    ASSERT_TRUE(sceneText.good());
    std::string contents((std::istreambuf_iterator<char>(sceneText)), std::istreambuf_iterator<char>());
    EXPECT_NE(contents.find("legacy_scene"), std::string::npos);
    EXPECT_NE(contents.find("script_triangle"), std::string::npos);

    fs::remove(outMesh);
    fs::remove(outScene);
    fs::remove(outText);
    fs::remove(scriptPath);
    fs::remove_all(outDir);
}

TEST(AutomationScript, MeshTransformCommandAppliesTranslation)
{
    ScriptBatchHarness harness;
    ScriptContext context;
    const auto original = nexus::geometry::primitives::makeTriangle(1.f).attributes().positions();
    const ScriptRunReport report = harness.runScript(
        "mesh.make_triangle size=1\n"
        "mesh.transform tx=2 ty=3 tz=4\n",
        context);

    EXPECT_TRUE(report.valid);
    ASSERT_TRUE(context.hasMesh);
    const auto& positions = context.mesh.attributes().positions();
    ASSERT_FALSE(positions.empty());
    ASSERT_EQ(positions.size(), original.size());
    EXPECT_NEAR(positions[0].x - original[0].x, 2.f, 1e-4f);
    EXPECT_NEAR(positions[0].y - original[0].y, 3.f, 1e-4f);
    EXPECT_NEAR(positions[0].z - original[0].z, 4.f, 1e-4f);
}

} // namespace nexus::automation::testing
