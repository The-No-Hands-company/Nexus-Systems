#include <nexus/automation/AutomationScript.h>

#include <gtest/gtest.h>

#include <algorithm>
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
    EXPECT_TRUE(harness.registry().hasCommand("script.list_commands"));
    EXPECT_TRUE(harness.registry().hasCommand("script.commands_hash"));
    EXPECT_TRUE(harness.registry().hasCommand("script.assert_command"));
    EXPECT_TRUE(harness.registry().hasCommand("script.export_replay_bundle"));
    EXPECT_TRUE(harness.registry().hasCommand("script.verify_replay_bundle"));
    EXPECT_TRUE(harness.registry().hasCommand("mesh.make_triangle"));
    EXPECT_TRUE(harness.registry().hasCommand("mesh.hash"));
    EXPECT_TRUE(harness.registry().hasCommand("scene.save"));
    EXPECT_TRUE(harness.registry().hasCommand("scene.export_bundle"));
    EXPECT_TRUE(harness.registry().hasCommand("render.create_null_context"));
    EXPECT_TRUE(harness.registry().hasCommand("animation.add_bone"));
    EXPECT_TRUE(harness.registry().hasCommand("animation.state_hash"));
    EXPECT_TRUE(harness.registry().hasCommand("sim.cross_solver_hash"));
    EXPECT_TRUE(harness.registry().hasCommand("parametric.hash"));
}

TEST(AutomationScript, ScriptListCommandsContainsKnownEntries)
{
    ScriptBatchHarness harness;
    ScriptContext context;
    const ScriptRunReport report = harness.runScript("script.list_commands\n", context);
    ASSERT_TRUE(report.valid);
    ASSERT_EQ(report.steps.size(), 1u);
    ASSERT_FALSE(report.steps[0].messages.empty());

    const auto& msgs = report.steps[0].messages;
    const bool hasMesh = std::any_of(msgs.begin(), msgs.end(), [](const std::string& m) {
        return m == "command=mesh.make_triangle";
    });
    const bool hasRigid = std::any_of(msgs.begin(), msgs.end(), [](const std::string& m) {
        return m == "command=sim.rigid.expect_hash";
    });
    const bool hasScript = std::any_of(msgs.begin(), msgs.end(), [](const std::string& m) {
        return m == "command=script.assert_command";
    });
    EXPECT_TRUE(hasMesh);
    EXPECT_TRUE(hasRigid);
    EXPECT_TRUE(hasScript);
}

TEST(AutomationScript, ScriptAssertCommandPassesAndFailsDeterministically)
{
    ScriptBatchHarness harness;
    ScriptContext context;
    const ScriptRunReport report = harness.runScript(
        "script.assert_command name=sim.rigid.create\n"
        "script.assert_command name=does.not.exist\n",
        context);

    EXPECT_FALSE(report.valid);
    ASSERT_EQ(report.steps.size(), 2u);
    EXPECT_TRUE(report.steps[0].success);
    EXPECT_FALSE(report.steps[1].success);
    ASSERT_FALSE(report.steps[1].messages.empty());
    EXPECT_NE(report.steps[1].messages[0].find("missing"), std::string::npos);
}

TEST(AutomationScript, ScriptCommandsHashIsDeterministic)
{
    ScriptBatchHarness harness;
    ScriptContext context;
    const ScriptRunReport a = harness.runScript("script.commands_hash\n", context);
    const ScriptRunReport b = harness.runScript("script.commands_hash\n", context);
    ASSERT_TRUE(a.valid);
    ASSERT_TRUE(b.valid);
    ASSERT_EQ(a.steps.size(), 1u);
    ASSERT_EQ(b.steps.size(), 1u);
    ASSERT_FALSE(a.steps[0].messages.empty());
    ASSERT_FALSE(b.steps[0].messages.empty());
    EXPECT_EQ(a.steps[0].messages[0], b.steps[0].messages[0]);
    EXPECT_NE(a.steps[0].messages[0].find("commands hash=0x"), std::string::npos);
}

TEST(AutomationScript, ScriptExportReportWritesDeterministicJsonLikeDiagnostics)
{
    const fs::path outDir = tempPath("report_export");
    const fs::path reportPath = outDir / "diagnostics.json";
    fs::create_directories(outDir);

    ScriptBatchHarness harness;
    ScriptContext context;
    const ScriptRunReport report = harness.runScript(
        "sim.rigid.create gy=0\n"
        "sim.rigid.add_body mass=1\n"
        "sim.rigid.set_baseline\n"
        "sim.cloth.create gy=0\n"
        "sim.cloth.add_node mass=1\n"
        "sim.cloth.set_baseline\n"
        "sim.fluid.create gy=0\n"
        "sim.fluid.add_particle mass=1\n"
        "sim.fluid.set_baseline\n"
        "script.export_report path=" + reportPath.string() + "\n",
        context);

    EXPECT_TRUE(report.valid);
    EXPECT_TRUE(fs::exists(reportPath));

    std::ifstream in(reportPath);
    ASSERT_TRUE(in.good());
    std::string contents((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    EXPECT_NE(contents.find("\"command_surface\""), std::string::npos);
    EXPECT_NE(contents.find("\"rigid\""), std::string::npos);
    EXPECT_NE(contents.find("\"cloth\""), std::string::npos);
    EXPECT_NE(contents.find("\"fluid\""), std::string::npos);
    EXPECT_NE(contents.find("\"hash\""), std::string::npos);

    fs::remove(reportPath);
    fs::remove_all(outDir);
}

TEST(AutomationScript, ScriptExportReplayBundleWritesCompactDeterministicSummary)
{
    const fs::path outDir = tempPath("replay_bundle_export");
    const fs::path bundleAPath = outDir / "replay_bundle_a.json";
    const fs::path bundleBPath = outDir / "replay_bundle_b.json";
    fs::create_directories(outDir);

    ScriptBatchHarness harness;
    ScriptContext context;
    const ScriptRunReport report = harness.runScript(
        "sim.rigid.create gy=0\n"
        "sim.rigid.add_body mass=1\n"
        "sim.rigid.set_baseline\n"
        "sim.cloth.create gy=0\n"
        "sim.cloth.add_node mass=1\n"
        "sim.cloth.set_baseline\n"
        "sim.fluid.create gy=0\n"
        "sim.fluid.add_particle mass=1\n"
        "sim.fluid.set_baseline\n"
        "script.export_replay_bundle path=" + bundleAPath.string() + "\n"
        "script.export_replay_bundle path=" + bundleBPath.string() + "\n",
        context);

    EXPECT_TRUE(report.valid);
    EXPECT_TRUE(fs::exists(bundleAPath));
    EXPECT_TRUE(fs::exists(bundleBPath));

    std::ifstream inA(bundleAPath);
    std::ifstream inB(bundleBPath);
    ASSERT_TRUE(inA.good());
    ASSERT_TRUE(inB.good());
    std::string contentsA((std::istreambuf_iterator<char>(inA)), std::istreambuf_iterator<char>());
    std::string contentsB((std::istreambuf_iterator<char>(inB)), std::istreambuf_iterator<char>());

    EXPECT_EQ(contentsA, contentsB);
    EXPECT_NE(contentsA.find("\"command_surface\""), std::string::npos);
    EXPECT_NE(contentsA.find("\"replay_hash\""), std::string::npos);
    EXPECT_NE(contentsA.find("\"replay\""), std::string::npos);
    EXPECT_NE(contentsA.find("\"rigid\""), std::string::npos);
    EXPECT_NE(contentsA.find("\"cloth\""), std::string::npos);
    EXPECT_NE(contentsA.find("\"fluid\""), std::string::npos);
    EXPECT_NE(contentsA.find("\"current_hash\""), std::string::npos);
    EXPECT_NE(contentsA.find("\"changed_bytes\""), std::string::npos);
    EXPECT_EQ(contentsA.find("\"commands\":"), std::string::npos);
    EXPECT_EQ(contentsA.find("\"body_count\":"), std::string::npos);

    fs::remove(bundleAPath);
    fs::remove(bundleBPath);
    fs::remove_all(outDir);
}

TEST(AutomationScript, ScriptVerifyReplayBundlePassesForMatchingState)
{
    const fs::path outDir = tempPath("replay_bundle_verify_ok");
    const fs::path bundlePath = outDir / "replay_bundle.json";
    fs::create_directories(outDir);

    ScriptBatchHarness harness;
    ScriptContext context;
    const ScriptRunReport report = harness.runScript(
        "sim.rigid.create gy=0\n"
        "sim.rigid.add_body mass=1\n"
        "sim.rigid.set_baseline\n"
        "sim.cloth.create gy=0\n"
        "sim.cloth.add_node mass=1\n"
        "sim.cloth.set_baseline\n"
        "sim.fluid.create gy=0\n"
        "sim.fluid.add_particle mass=1\n"
        "sim.fluid.set_baseline\n"
        "script.export_replay_bundle path=" + bundlePath.string() + "\n"
        "script.verify_replay_bundle path=" + bundlePath.string() + "\n",
        context);

    EXPECT_TRUE(report.valid);
    ASSERT_EQ(report.steps.size(), 11u);
    EXPECT_TRUE(report.steps.back().success);
    ASSERT_FALSE(report.steps.back().messages.empty());
    EXPECT_NE(report.steps.back().messages.front().find("match:"), std::string::npos);

    fs::remove(bundlePath);
    fs::remove_all(outDir);
}

TEST(AutomationScript, ScriptVerifyReplayBundleFailsAfterStateMutation)
{
    const fs::path outDir = tempPath("replay_bundle_verify_mismatch");
    const fs::path bundlePath = outDir / "replay_bundle.json";
    fs::create_directories(outDir);

    ScriptBatchHarness harness;
    ScriptContext context;
    const ScriptRunReport report = harness.runScript(
        "sim.rigid.create gy=0\n"
        "sim.rigid.add_body mass=1\n"
        "sim.rigid.set_baseline\n"
        "sim.cloth.create gy=0\n"
        "sim.cloth.add_node mass=1\n"
        "sim.cloth.set_baseline\n"
        "sim.fluid.create gy=0\n"
        "sim.fluid.add_particle mass=1\n"
        "sim.fluid.set_baseline\n"
        "script.export_replay_bundle path=" + bundlePath.string() + "\n"
        "sim.rigid.step dt=0.1\n"
        "script.verify_replay_bundle path=" + bundlePath.string() + "\n",
        context);

    EXPECT_FALSE(report.valid);
    ASSERT_EQ(report.steps.size(), 12u);
    EXPECT_FALSE(report.steps.back().success);
    ASSERT_FALSE(report.steps.back().messages.empty());
    EXPECT_NE(report.steps.back().messages.front().find("mismatch expected="), std::string::npos);

    fs::remove(bundlePath);
    fs::remove_all(outDir);
}

TEST(AutomationScript, RegistryListCommandsIsDeterministicAndComplete)
{
    ScriptBatchHarness harness;
    const auto commandsA = harness.registry().listCommands();
    const auto commandsB = harness.registry().listCommands();

    EXPECT_EQ(commandsA, commandsB);
    EXPECT_TRUE(std::is_sorted(commandsA.begin(), commandsA.end()));
    EXPECT_NE(std::find(commandsA.begin(), commandsA.end(), "sim.rigid.expect_hash"), commandsA.end());
    EXPECT_NE(std::find(commandsA.begin(), commandsA.end(), "sim.cloth.set_baseline"), commandsA.end());
    EXPECT_NE(std::find(commandsA.begin(), commandsA.end(), "sim.fluid.diff_state"), commandsA.end());
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

TEST(AutomationScript, ReplayHashAssertionWorksFromScriptFile)
{
    const fs::path outDir = tempPath("replay_hash_pipeline");
    const fs::path scriptA = outDir / "capture_hash.nxscript";
    const fs::path scriptB = outDir / "assert_hash.nxscript";
    fs::create_directories(outDir);

    writeScriptFile(scriptA,
        "sim.rigid.create gy=0\n"
        "sim.rigid.add_body mass=1 tx=0 ty=0 tz=0\n"
        "sim.rigid.step dt=0.01\n"
        "sim.rigid.state_hash\n");

    ScriptBatchHarness harness;
    ScriptContext context;
    const ScriptRunReport capture = harness.runScriptFile(scriptA, context);
    ASSERT_TRUE(capture.valid);
    ASSERT_FALSE(capture.steps.empty());
    const auto& hashMsg = capture.steps.back().messages.front();
    const auto pos = hashMsg.find("hash=");
    ASSERT_NE(pos, std::string::npos);
    const std::string expectedHash = hashMsg.substr(pos + 5, hashMsg.find(' ', pos + 5) - (pos + 5));

    writeScriptFile(scriptB,
        "sim.rigid.expect_hash hash=" + expectedHash + "\n"
        "sim.rigid.step dt=0.01\n"
        "sim.rigid.expect_hash hash=" + expectedHash + "\n");

    const ScriptRunReport replay = harness.runScriptFile(scriptB, context);
    EXPECT_FALSE(replay.valid);
    ASSERT_EQ(replay.steps.size(), 3u);
    EXPECT_TRUE(replay.steps[0].success);
    EXPECT_TRUE(replay.steps[1].success);
    EXPECT_FALSE(replay.steps[2].success);

    fs::remove(scriptA);
    fs::remove(scriptB);
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

TEST(AutomationScript, RunReportMessagesAreDeterministicAndSorted)
{
    ScriptBatchHarness harness;
    ScriptContext context;
    // three unknown commands in intentionally non-lexicographic order to prove sorting
    const ScriptRunReport report = harness.runScript(
        "zzz_unknown\nbbb_unknown\naaa_unknown\n",
        context);
    EXPECT_FALSE(report.valid);
    ASSERT_GE(report.messages.size(), 3u);
    EXPECT_TRUE(std::is_sorted(report.messages.begin(), report.messages.end()));
}

TEST(AutomationScript, MissingScriptFileMessagesAreDeterministicAndSorted)
{
    ScriptBatchHarness harness;
    ScriptContext context;
    const ScriptRunReport report = harness.runScriptFile(
        "/nonexistent/path/missing.nxscript", context);
    EXPECT_FALSE(report.valid);
    ASSERT_FALSE(report.messages.empty());
    EXPECT_TRUE(std::is_sorted(report.messages.begin(), report.messages.end()));
}

// ── sim.rigid.* commands ──────────────────────────────────────────────────────

TEST(AutomationScript, RigidSimCommandsAreRegistered)
{
    ScriptBatchHarness harness;
    EXPECT_TRUE(harness.registry().hasCommand("sim.rigid.create"));
    EXPECT_TRUE(harness.registry().hasCommand("sim.rigid.add_body"));
    EXPECT_TRUE(harness.registry().hasCommand("sim.rigid.apply_force"));
    EXPECT_TRUE(harness.registry().hasCommand("sim.rigid.step"));
    EXPECT_TRUE(harness.registry().hasCommand("sim.rigid.capture_state"));
    EXPECT_TRUE(harness.registry().hasCommand("sim.rigid.restore_state"));
    EXPECT_TRUE(harness.registry().hasCommand("sim.rigid.export_state"));
    EXPECT_TRUE(harness.registry().hasCommand("sim.rigid.import_state"));
    EXPECT_TRUE(harness.registry().hasCommand("sim.rigid.state_hash"));
    EXPECT_TRUE(harness.registry().hasCommand("sim.rigid.state_summary"));
    EXPECT_TRUE(harness.registry().hasCommand("sim.rigid.diff_state"));
    EXPECT_TRUE(harness.registry().hasCommand("sim.rigid.set_baseline"));
    EXPECT_TRUE(harness.registry().hasCommand("sim.rigid.expect_hash"));
}

TEST(AutomationScript, RigidSimPipelineRunsDeterministically)
{
    ScriptBatchHarness harness;
    ScriptContext context;
    const ScriptRunReport report = harness.runScript(
        "sim.rigid.create gy=-9.81\n"
        "sim.rigid.add_body mass=1 tx=0 ty=10 tz=0\n"
        "sim.rigid.add_body mass=0 tx=0 ty=0 tz=0\n"   // static body
        "sim.rigid.step dt=0.016\n"
        "sim.rigid.capture_state\n"
        "sim.rigid.step dt=0.016\n"
        "sim.rigid.restore_state\n",
        context);

    EXPECT_TRUE(report.valid) << [&]() {
        std::string msg;
        for (const auto& m : report.messages) msg += m + "\n";
        return msg;
    }();
    ASSERT_TRUE(context.hasRigidSolver);
    EXPECT_EQ(context.rigidSolver->bodyCount(), 2u);
    // Capture includes all bodies (dynamic + static); only integration excludes static.
    EXPECT_EQ(context.rigidLastState.bodies.size(), 2u);
}

TEST(AutomationScript, RigidSimStepWithInvalidDtFails)
{
    ScriptBatchHarness harness;
    ScriptContext context;
    const ScriptRunReport report = harness.runScript(
        "sim.rigid.create\n"
        "sim.rigid.add_body mass=1\n"
        "sim.rigid.step dt=-1\n",
        context);
    EXPECT_FALSE(report.valid);
}

// ── sim.cloth.* commands ──────────────────────────────────────────────────────

TEST(AutomationScript, ClothSimCommandsAreRegistered)
{
    ScriptBatchHarness harness;
    EXPECT_TRUE(harness.registry().hasCommand("sim.cloth.create"));
    EXPECT_TRUE(harness.registry().hasCommand("sim.cloth.add_node"));
    EXPECT_TRUE(harness.registry().hasCommand("sim.cloth.add_edge"));
    EXPECT_TRUE(harness.registry().hasCommand("sim.cloth.step"));
    EXPECT_TRUE(harness.registry().hasCommand("sim.cloth.capture_state"));
    EXPECT_TRUE(harness.registry().hasCommand("sim.cloth.restore_state"));
    EXPECT_TRUE(harness.registry().hasCommand("sim.cloth.export_state"));
    EXPECT_TRUE(harness.registry().hasCommand("sim.cloth.import_state"));
    EXPECT_TRUE(harness.registry().hasCommand("sim.cloth.state_hash"));
    EXPECT_TRUE(harness.registry().hasCommand("sim.cloth.state_summary"));
    EXPECT_TRUE(harness.registry().hasCommand("sim.cloth.diff_state"));
    EXPECT_TRUE(harness.registry().hasCommand("sim.cloth.set_baseline"));
    EXPECT_TRUE(harness.registry().hasCommand("sim.cloth.expect_hash"));
}

TEST(AutomationScript, ClothSimPipelineRunsDeterministically)
{
    ScriptBatchHarness harness;
    ScriptContext context;
    const ScriptRunReport report = harness.runScript(
        "sim.cloth.create gy=-9.81\n"
        "sim.cloth.add_node mass=1 tx=0 ty=5 tz=0\n"
        "sim.cloth.add_node mass=0 tx=0 ty=0 tz=0\n"   // pinned node
        "sim.cloth.step dt=0.016\n"
        "sim.cloth.capture_state\n"
        "sim.cloth.step dt=0.016\n"
        "sim.cloth.restore_state\n",
        context);

    EXPECT_TRUE(report.valid) << [&]() {
        std::string msg;
        for (const auto& m : report.messages) msg += m + "\n";
        return msg;
    }();
    ASSERT_TRUE(context.hasClothSolver);
    EXPECT_EQ(context.clothSolver->nodeCount(), 2u);
}

TEST(AutomationScript, ClothSimRequiresCreateFirst)
{
    ScriptBatchHarness harness;
    ScriptContext context;
    const ScriptRunReport report = harness.runScript("sim.cloth.add_node mass=1\n", context);
    EXPECT_FALSE(report.valid);
}

// ── sim.fluid.* commands ──────────────────────────────────────────────────────

TEST(AutomationScript, FluidSimCommandsAreRegistered)
{
    ScriptBatchHarness harness;
    EXPECT_TRUE(harness.registry().hasCommand("sim.fluid.create"));
    EXPECT_TRUE(harness.registry().hasCommand("sim.fluid.add_particle"));
    EXPECT_TRUE(harness.registry().hasCommand("sim.fluid.step"));
    EXPECT_TRUE(harness.registry().hasCommand("sim.fluid.capture_state"));
    EXPECT_TRUE(harness.registry().hasCommand("sim.fluid.restore_state"));
    EXPECT_TRUE(harness.registry().hasCommand("sim.fluid.export_state"));
    EXPECT_TRUE(harness.registry().hasCommand("sim.fluid.import_state"));
    EXPECT_TRUE(harness.registry().hasCommand("sim.fluid.state_hash"));
    EXPECT_TRUE(harness.registry().hasCommand("sim.fluid.state_summary"));
    EXPECT_TRUE(harness.registry().hasCommand("sim.fluid.diff_state"));
    EXPECT_TRUE(harness.registry().hasCommand("sim.fluid.set_baseline"));
    EXPECT_TRUE(harness.registry().hasCommand("sim.fluid.expect_hash"));
}

TEST(AutomationScript, FluidSimPipelineRunsDeterministically)
{
    ScriptBatchHarness harness;
    ScriptContext context;
    const ScriptRunReport report = harness.runScript(
        "sim.fluid.create gy=-9.81 h=0.15 k=250\n"
        "sim.fluid.add_particle mass=1 tx=0 ty=2 tz=0 density=1000\n"
        "sim.fluid.add_particle mass=0 tx=0 ty=0 tz=0 density=1000\n"
        "sim.fluid.step dt=0.016\n"
        "sim.fluid.capture_state\n"
        "sim.fluid.step dt=0.016\n"
        "sim.fluid.restore_state\n",
        context);

    EXPECT_TRUE(report.valid) << [&]() {
        std::string msg;
        for (const auto& m : report.messages) msg += m + "\n";
        return msg;
    }();
    ASSERT_TRUE(context.hasFluidSolver);
    EXPECT_EQ(context.fluidSolver->particleCount(), 2u);
    EXPECT_EQ(context.fluidLastState.particles.size(), 2u);
}

TEST(AutomationScript, FluidSimStepWithInvalidDtFails)
{
    ScriptBatchHarness harness;
    ScriptContext context;
    const ScriptRunReport report = harness.runScript(
        "sim.fluid.create\n"
        "sim.fluid.add_particle mass=1\n"
        "sim.fluid.step dt=-1\n",
        context);
    EXPECT_FALSE(report.valid);
}

// ── deterministic state import/export ────────────────────────────────────────

TEST(AutomationScript, RigidStateCanExportAndImport)
{
    const fs::path statePath = tempPath("rigid_state.bin");
    ScriptBatchHarness harness;
    ScriptContext context;
    const ScriptRunReport report = harness.runScript(
        "sim.rigid.create\n"
        "sim.rigid.add_body mass=1 ty=10\n"
        "sim.rigid.step dt=0.02\n"
        "sim.rigid.export_state path=" + statePath.string() + "\n"
        "sim.rigid.step dt=0.02\n"
        "sim.rigid.import_state path=" + statePath.string() + "\n"
        "sim.rigid.capture_state\n",
        context);

    EXPECT_TRUE(report.valid);
    ASSERT_TRUE(context.hasRigidSolver);
    EXPECT_TRUE(fs::exists(statePath));
    EXPECT_NEAR(context.rigidLastState.simulationTime, 0.02, 1e-9);
    fs::remove(statePath);
}

TEST(AutomationScript, ClothStateCanExportAndImport)
{
    const fs::path statePath = tempPath("cloth_state.bin");
    ScriptBatchHarness harness;
    ScriptContext context;
    const ScriptRunReport report = harness.runScript(
        "sim.cloth.create\n"
        "sim.cloth.add_node mass=1 ty=3\n"
        "sim.cloth.step dt=0.01\n"
        "sim.cloth.export_state path=" + statePath.string() + "\n"
        "sim.cloth.step dt=0.01\n"
        "sim.cloth.import_state path=" + statePath.string() + "\n"
        "sim.cloth.capture_state\n",
        context);

    EXPECT_TRUE(report.valid);
    ASSERT_TRUE(context.hasClothSolver);
    EXPECT_TRUE(fs::exists(statePath));
    EXPECT_NEAR(context.clothLastState.simulationTime, 0.01, 1e-9);
    fs::remove(statePath);
}

TEST(AutomationScript, FluidStateCanExportAndImport)
{
    const fs::path statePath = tempPath("fluid_state.bin");
    ScriptBatchHarness harness;
    ScriptContext context;
    const ScriptRunReport report = harness.runScript(
        "sim.fluid.create\n"
        "sim.fluid.add_particle mass=1 ty=1 density=1000\n"
        "sim.fluid.step dt=0.01\n"
        "sim.fluid.export_state path=" + statePath.string() + "\n"
        "sim.fluid.step dt=0.01\n"
        "sim.fluid.import_state path=" + statePath.string() + "\n"
        "sim.fluid.capture_state\n",
        context);

    EXPECT_TRUE(report.valid);
    ASSERT_TRUE(context.hasFluidSolver);
    EXPECT_TRUE(fs::exists(statePath));
    EXPECT_NEAR(context.fluidLastState.simulationTime, 0.01, 1e-9);
    fs::remove(statePath);
}

TEST(AutomationScript, FluidStateImportRejectsMalformedBlob)
{
    const fs::path badPath = tempPath("fluid_state_bad.bin");
    {
        std::ofstream out(badPath, std::ios::binary | std::ios::trunc);
        ASSERT_TRUE(out.good());
        const char raw[5] = {0x01, 0x02, 0x03, 0x04, 0x05};
        out.write(raw, 5);
    }

    ScriptBatchHarness harness;
    ScriptContext context;
    const ScriptRunReport report = harness.runScript(
        "sim.fluid.create\n"
        "sim.fluid.add_particle mass=1\n"
        "sim.fluid.import_state path=" + badPath.string() + "\n",
        context);

    EXPECT_FALSE(report.valid);
    fs::remove(badPath);
}

TEST(AutomationScript, RigidApplyForceInfluencesState)
{
    ScriptBatchHarness harness;
    ScriptContext context;
    const ScriptRunReport report = harness.runScript(
        "sim.rigid.create gy=0\n"
        "sim.rigid.add_body mass=1 tx=0 ty=0 tz=0\n"
        "sim.rigid.apply_force id=1 fx=10 fy=0 fz=0\n"
        "sim.rigid.step dt=0.1\n"
        "sim.rigid.capture_state\n",
        context);

    EXPECT_TRUE(report.valid);
    ASSERT_EQ(context.rigidLastState.bodies.size(), 1u);
    EXPECT_GT(context.rigidLastState.bodies[0].position.x, 0.0f);
}

TEST(AutomationScript, ClothAddEdgeInfluencesState)
{
    ScriptBatchHarness harness;
    ScriptContext context;
    const ScriptRunReport report = harness.runScript(
        "sim.cloth.create gy=0\n"
        "sim.cloth.add_node mass=1 tx=-1 ty=0 tz=0\n"
        "sim.cloth.add_node mass=1 tx=1 ty=0 tz=0\n"
        "sim.cloth.add_edge a=1 b=2 rest=1 stiff=100\n"
        "sim.cloth.step dt=0.016\n"
        "sim.cloth.capture_state\n",
        context);

    EXPECT_TRUE(report.valid);
    ASSERT_EQ(context.clothLastState.nodes.size(), 2u);
    EXPECT_GT(context.clothLastState.nodes[0].position.x, -1.0f);
    EXPECT_LT(context.clothLastState.nodes[1].position.x, 1.0f);
}

TEST(AutomationScript, SnapshotDiffReportingUsesDeterministicHashSummary)
{
    ScriptBatchHarness harness;
    ScriptContext context;
    const ScriptRunReport report = harness.runScript(
        "sim.rigid.create gy=0\n"
        "sim.rigid.add_body mass=1 tx=0 ty=0 tz=0\n"
        "sim.rigid.capture_state\n"
        "sim.rigid.state_hash\n"
        "sim.rigid.state_summary\n"
        "sim.rigid.diff_state\n"
        "sim.rigid.step dt=0.01\n"
        "sim.rigid.diff_state\n",
        context);

    EXPECT_TRUE(report.valid);

    auto findStep = [&](const std::string& name, size_t nth) -> const ScriptStepReport* {
        size_t seen = 0;
        for (const auto& step : report.steps) {
            if (step.command == name) {
                if (seen == nth) {
                    return &step;
                }
                ++seen;
            }
        }
        return nullptr;
    };

    const ScriptStepReport* hashStep = findStep("sim.rigid.state_hash", 0);
    ASSERT_NE(hashStep, nullptr);
    ASSERT_FALSE(hashStep->messages.empty());
    EXPECT_NE(hashStep->messages[0].find("rigid hash=0x"), std::string::npos);

    const ScriptStepReport* summaryStep = findStep("sim.rigid.state_summary", 0);
    ASSERT_NE(summaryStep, nullptr);
    ASSERT_FALSE(summaryStep->messages.empty());
    EXPECT_NE(summaryStep->messages[0].find("rigid summary"), std::string::npos);

    const ScriptStepReport* diffSameStep = findStep("sim.rigid.diff_state", 0);
    ASSERT_NE(diffSameStep, nullptr);
    ASSERT_FALSE(diffSameStep->messages.empty());
    EXPECT_NE(diffSameStep->messages[0].find("equal=1"), std::string::npos);

    const ScriptStepReport* diffChangedStep = findStep("sim.rigid.diff_state", 1);
    ASSERT_NE(diffChangedStep, nullptr);
    ASSERT_FALSE(diffChangedStep->messages.empty());
    EXPECT_NE(diffChangedStep->messages[0].find("equal=0"), std::string::npos);
}

TEST(AutomationScript, RigidSetBaselineAndExpectHash)
{
    ScriptBatchHarness harness;
    ScriptContext context;

    const ScriptRunReport first = harness.runScript(
        "sim.rigid.create gy=0\n"
        "sim.rigid.add_body mass=1\n"
        "sim.rigid.set_baseline\n"
        "sim.rigid.state_hash\n",
        context);
    ASSERT_TRUE(first.valid);

    const auto hashMsg = first.steps.back().messages.front();
    const auto pos = hashMsg.find("hash=");
    ASSERT_NE(pos, std::string::npos);
    const std::string expectedHash = hashMsg.substr(pos + 5, hashMsg.find(' ', pos + 5) - (pos + 5));

    const ScriptRunReport second = harness.runScript(
        "sim.rigid.expect_hash hash=" + expectedHash + "\n"
        "sim.rigid.step dt=0.01\n"
        "sim.rigid.expect_hash hash=" + expectedHash + "\n",
        context);

    EXPECT_FALSE(second.valid);
    ASSERT_EQ(second.steps.size(), 3u);
    EXPECT_TRUE(second.steps[0].success);
    EXPECT_TRUE(second.steps[1].success);
    EXPECT_FALSE(second.steps[2].success);
}

TEST(AutomationScript, ClothSetBaselineAndExpectHash)
{
    ScriptBatchHarness harness;
    ScriptContext context;

    const ScriptRunReport first = harness.runScript(
        "sim.cloth.create gy=0\n"
        "sim.cloth.add_node mass=1\n"
        "sim.cloth.set_baseline\n"
        "sim.cloth.state_hash\n",
        context);
    ASSERT_TRUE(first.valid);

    const auto hashMsg = first.steps.back().messages.front();
    const auto pos = hashMsg.find("hash=");
    ASSERT_NE(pos, std::string::npos);
    const std::string expectedHash = hashMsg.substr(pos + 5, hashMsg.find(' ', pos + 5) - (pos + 5));

    const ScriptRunReport second = harness.runScript(
        "sim.cloth.expect_hash hash=" + expectedHash + "\n"
        "sim.cloth.step dt=0.01\n"
        "sim.cloth.expect_hash hash=" + expectedHash + "\n",
        context);

    EXPECT_FALSE(second.valid);
    ASSERT_EQ(second.steps.size(), 3u);
    EXPECT_TRUE(second.steps[0].success);
    EXPECT_TRUE(second.steps[1].success);
    EXPECT_FALSE(second.steps[2].success);
}

TEST(AutomationScript, FluidSetBaselineAndExpectHash)
{
    ScriptBatchHarness harness;
    ScriptContext context;

    const ScriptRunReport first = harness.runScript(
        "sim.fluid.create gy=0\n"
        "sim.fluid.add_particle mass=1 density=1000\n"
        "sim.fluid.set_baseline\n"
        "sim.fluid.state_hash\n",
        context);
    ASSERT_TRUE(first.valid);

    const auto hashMsg = first.steps.back().messages.front();
    const auto pos = hashMsg.find("hash=");
    ASSERT_NE(pos, std::string::npos);
    const std::string expectedHash = hashMsg.substr(pos + 5, hashMsg.find(' ', pos + 5) - (pos + 5));

    const ScriptRunReport second = harness.runScript(
        "sim.fluid.expect_hash hash=" + expectedHash + "\n"
        "sim.fluid.step dt=0.01\n"
        "sim.fluid.expect_hash hash=" + expectedHash + "\n",
        context);

    EXPECT_FALSE(second.valid);
    ASSERT_EQ(second.steps.size(), 3u);
    EXPECT_TRUE(second.steps[0].success);
    EXPECT_TRUE(second.steps[1].success);
    EXPECT_FALSE(second.steps[2].success);
}

// ── gaussian.* commands ───────────────────────────────────────────────────────

TEST(AutomationScript, GaussianCommandsAreRegistered)
{
    ScriptBatchHarness harness;
    EXPECT_TRUE(harness.registry().hasCommand("gaussian.load_ply"));
    EXPECT_TRUE(harness.registry().hasCommand("gaussian.describe"));
}

TEST(AutomationScript, GaussianDescribeRequiresLoadFirst)
{
    ScriptBatchHarness harness;
    ScriptContext context;
    const ScriptRunReport report = harness.runScript("gaussian.describe\n", context);
    EXPECT_FALSE(report.valid);
}

TEST(AutomationScript, GaussianLoadAndDescribePipeline)
{
    // Write a minimal PLY file to tmp, then run gaussian.load_ply + gaussian.describe.
    const fs::path plyPath = tempPath("gaussian_test.ply");

    // Build a 2-splat PLY in memory and write to disk.
    {
        std::ostringstream hdr;
        hdr << "ply\n"
            << "format binary_little_endian 1.0\n"
            << "element vertex 2\n"
            << "property float x\n"
            << "property float y\n"
            << "property float z\n"
            << "property float f_dc_0\n"
            << "property float f_dc_1\n"
            << "property float f_dc_2\n"
            << "property float opacity\n"
            << "property float scale_0\n"
            << "property float scale_1\n"
            << "property float scale_2\n"
            << "property float rot_0\n"
            << "property float rot_1\n"
            << "property float rot_2\n"
            << "property float rot_3\n"
            << "end_header\n";

        std::ofstream out(plyPath, std::ios::binary | std::ios::trunc);
        ASSERT_TRUE(out.good());
        const std::string hdrStr = hdr.str();
        out.write(hdrStr.data(), static_cast<std::streamsize>(hdrStr.size()));
        for (int i = 0; i < 2; ++i) {
            float data[14] = {
                static_cast<float>(i), 0.f, 0.f,
                0.5f, 0.5f, 0.5f,
                0.9f,
                -1.f, -1.f, -1.f,
                1.f, 0.f, 0.f, 0.f   // rot w,x,y,z
            };
            out.write(reinterpret_cast<const char*>(data), sizeof(data));
        }
    }

    ScriptBatchHarness harness;
    ScriptContext context;
    const ScriptRunReport report = harness.runScript(
        "gaussian.load_ply path=" + plyPath.string() + "\n"
        "gaussian.describe\n",
        context);

    EXPECT_TRUE(report.valid) << [&]() {
        std::string msg;
        for (const auto& m : report.messages) msg += m + "\n";
        return msg;
    }();
    EXPECT_TRUE(context.hasGaussianCloud);
    EXPECT_EQ(context.gaussianCloud.splatCount(), 2u);

    fs::remove(plyPath);
}

TEST(AutomationScript, GaussianLoadMissingFileFailsDeterministically)
{
    ScriptBatchHarness harness;
    ScriptContext context;
    const ScriptRunReport report = harness.runScript(
        "gaussian.load_ply path=/nonexistent/does_not_exist.ply\n", context);
    EXPECT_FALSE(report.valid);
    EXPECT_FALSE(context.hasGaussianCloud);
}

// ── mesh.hash / mesh.set_baseline / mesh.diff / mesh.expect_hash ─────────────

TEST(AutomationScript, MeshHashCommandsAreRegistered)
{
    ScriptBatchHarness harness;
    EXPECT_TRUE(harness.registry().hasCommand("mesh.hash"));
    EXPECT_TRUE(harness.registry().hasCommand("mesh.set_baseline"));
    EXPECT_TRUE(harness.registry().hasCommand("mesh.diff"));
    EXPECT_TRUE(harness.registry().hasCommand("mesh.expect_hash"));
    EXPECT_TRUE(harness.registry().hasCommand("mesh.export_bundle"));
    EXPECT_TRUE(harness.registry().hasCommand("mesh.verify_bundle"));
}

TEST(AutomationScript, MeshHashIsDeterministicAndDiffDetectsChange)
{
    ScriptBatchHarness harness;
    ScriptContext context;
    const ScriptRunReport report = harness.runScript(
        "mesh.make_triangle size=1\n"
        "mesh.hash\n"
        "mesh.hash\n"
        "mesh.set_baseline\n"
        "mesh.transform tx=1\n"
        "mesh.diff\n",
        context);

    EXPECT_TRUE(report.valid);
    ASSERT_EQ(report.steps.size(), 6u);
    ASSERT_FALSE(report.steps[1].messages.empty());
    ASSERT_FALSE(report.steps[2].messages.empty());
    // Hash is deterministic — two calls on the same mesh produce identical output.
    EXPECT_EQ(report.steps[1].messages.front(), report.steps[2].messages.front());
    EXPECT_NE(report.steps[1].messages.front().find("mesh hash=0x"), std::string::npos);
    // Diff detects position change from mesh.transform.
    ASSERT_FALSE(report.steps[5].messages.empty());
    EXPECT_NE(report.steps[5].messages.front().find("equal=0"), std::string::npos);
}

TEST(AutomationScript, MeshExpectHashDetectsMismatch)
{
    ScriptBatchHarness harness;
    ScriptContext context;
    const ScriptRunReport report = harness.runScript(
        "mesh.make_triangle size=1\n"
        "mesh.expect_hash hash=0x1\n",
        context);

    EXPECT_FALSE(report.valid);
    ASSERT_EQ(report.steps.size(), 2u);
    EXPECT_FALSE(report.steps[1].success);
    ASSERT_FALSE(report.steps[1].messages.empty());
    EXPECT_NE(report.steps[1].messages.front().find("mismatch"), std::string::npos);
}

TEST(AutomationScript, MeshExportAndVerifyBundleRoundTrip)
{
    const fs::path outDir = tempPath("mesh_bundle_verify");
    const fs::path bundlePath = outDir / "mesh_bundle.json";
    fs::create_directories(outDir);

    ScriptBatchHarness harness;
    ScriptContext context;
    const ScriptRunReport report = harness.runScript(
        "mesh.make_triangle size=1\n"
        "mesh.export_bundle path=" + bundlePath.string() + "\n"
        "mesh.verify_bundle path=" + bundlePath.string() + "\n",
        context);

    EXPECT_TRUE(report.valid);
    ASSERT_EQ(report.steps.size(), 3u);
    EXPECT_TRUE(report.steps.back().success);
    ASSERT_FALSE(report.steps.back().messages.empty());
    EXPECT_NE(report.steps.back().messages.front().find("match:"), std::string::npos);

    fs::remove(bundlePath);
    fs::remove_all(outDir);
}

TEST(AutomationScript, MeshVerifyBundleRejectsMissingOrInvalidHash)
{
    const fs::path outDir = tempPath("mesh_bundle_verify_malformed");
    const fs::path bundlePath = outDir / "mesh_bundle.json";
    fs::create_directories(outDir);

    {
        std::ofstream out(bundlePath, std::ios::trunc);
        ASSERT_TRUE(out.good());
        out << "{\"vertex_count\":3,\"face_count\":1}";
        ASSERT_TRUE(out.good());
    }

    ScriptBatchHarness harness;
    ScriptContext context;
    const ScriptRunReport missingHashReport = harness.runScript(
        "mesh.make_triangle size=1\n"
        "mesh.verify_bundle path=" + bundlePath.string() + "\n",
        context);

    EXPECT_FALSE(missingHashReport.valid);
    ASSERT_EQ(missingHashReport.steps.size(), 2u);
    EXPECT_FALSE(missingHashReport.steps.back().success);
    ASSERT_FALSE(missingHashReport.steps.back().messages.empty());
    EXPECT_NE(missingHashReport.steps.back().messages.front().find("missing mesh_hash"), std::string::npos);

    {
        std::ofstream out(bundlePath, std::ios::trunc);
        ASSERT_TRUE(out.good());
        out << "{\"mesh_hash\":123,\"vertex_count\":3,\"face_count\":1}";
        ASSERT_TRUE(out.good());
    }

    ScriptContext invalidContext;
    const ScriptRunReport invalidHashReport = harness.runScript(
        "mesh.make_triangle size=1\n"
        "mesh.verify_bundle path=" + bundlePath.string() + "\n",
        invalidContext);

    EXPECT_FALSE(invalidHashReport.valid);
    ASSERT_EQ(invalidHashReport.steps.size(), 2u);
    EXPECT_FALSE(invalidHashReport.steps.back().success);
    ASSERT_FALSE(invalidHashReport.steps.back().messages.empty());
    EXPECT_NE(invalidHashReport.steps.back().messages.front().find("missing mesh_hash"), std::string::npos);

    fs::remove(bundlePath);
    fs::remove_all(outDir);
}

TEST(AutomationScript, MeshVerifyBundleFailsAfterMutation)
{
    const fs::path outDir = tempPath("mesh_bundle_verify_fail");
    const fs::path bundlePath = outDir / "mesh_bundle.json";
    fs::create_directories(outDir);

    ScriptBatchHarness harness;
    ScriptContext context;
    const ScriptRunReport report = harness.runScript(
        "mesh.make_triangle size=1\n"
        "mesh.export_bundle path=" + bundlePath.string() + "\n"
        "mesh.transform tx=1\n"
        "mesh.verify_bundle path=" + bundlePath.string() + "\n",
        context);

    EXPECT_FALSE(report.valid);
    ASSERT_EQ(report.steps.size(), 4u);
    EXPECT_FALSE(report.steps.back().success);
    ASSERT_FALSE(report.steps.back().messages.empty());
    EXPECT_NE(report.steps.back().messages.front().find("mismatch"), std::string::npos);

    fs::remove(bundlePath);
    fs::remove_all(outDir);
}

// ── scene.hash / scene.set_baseline / scene.diff / scene.export_bundle / scene.verify_bundle ─

TEST(AutomationScript, SceneHashCommandsAreRegistered)
{
    ScriptBatchHarness harness;
    EXPECT_TRUE(harness.registry().hasCommand("scene.hash"));
    EXPECT_TRUE(harness.registry().hasCommand("scene.set_baseline"));
    EXPECT_TRUE(harness.registry().hasCommand("scene.diff"));
    EXPECT_TRUE(harness.registry().hasCommand("scene.rename_entry"));
    EXPECT_TRUE(harness.registry().hasCommand("scene.remove_entry"));
    EXPECT_TRUE(harness.registry().hasCommand("scene.export_bundle"));
    EXPECT_TRUE(harness.registry().hasCommand("scene.verify_bundle"));
}

TEST(AutomationScript, SceneRenameEntryMutatesSceneState)
{
    ScriptBatchHarness harness;
    ScriptContext context;
    const ScriptRunReport report = harness.runScript(
        "mesh.make_triangle size=1\n"
        "scene.new name=test_scene\n"
        "scene.add_mesh_entry name=tri\n"
        "scene.set_baseline\n"
        "scene.rename_entry from=tri to=tri_renamed\n"
        "scene.diff\n",
        context);

    EXPECT_TRUE(report.valid);
    ASSERT_EQ(report.steps.size(), 6u);
    ASSERT_FALSE(report.steps[4].messages.empty());
    EXPECT_NE(report.steps[4].messages.front().find("scene entry renamed:"), std::string::npos);
    ASSERT_FALSE(report.steps[5].messages.empty());
    EXPECT_NE(report.steps[5].messages.front().find("equal=0"), std::string::npos);
}

TEST(AutomationScript, SceneRemoveEntryMutatesSceneState)
{
    ScriptBatchHarness harness;
    ScriptContext context;
    const ScriptRunReport report = harness.runScript(
        "mesh.make_triangle size=1\n"
        "scene.new name=test_scene\n"
        "scene.add_mesh_entry name=tri\n"
        "scene.set_baseline\n"
        "scene.remove_entry name=tri\n"
        "scene.diff\n",
        context);

    EXPECT_TRUE(report.valid);
    ASSERT_EQ(report.steps.size(), 6u);
    ASSERT_FALSE(report.steps[4].messages.empty());
    EXPECT_NE(report.steps[4].messages.front().find("scene entry removed:"), std::string::npos);
    ASSERT_FALSE(report.steps[5].messages.empty());
    EXPECT_NE(report.steps[5].messages.front().find("equal=0"), std::string::npos);
}

TEST(AutomationScript, SceneRenameEntrySupportsIndexVariant)
{
    ScriptBatchHarness harness;
    ScriptContext context;
    const ScriptRunReport report = harness.runScript(
        "mesh.make_triangle size=1\n"
        "scene.new name=test_scene\n"
        "scene.add_mesh_entry name=first\n"
        "mesh.make_triangle size=2\n"
        "scene.add_mesh_entry name=second\n"
        "scene.set_baseline\n"
        "scene.rename_entry index=0 to=renamed\n"
        "scene.diff\n",
        context);

    EXPECT_TRUE(report.valid);
    ASSERT_EQ(report.steps.size(), 8u);
    ASSERT_FALSE(report.steps[6].messages.empty());
    EXPECT_NE(report.steps[6].messages.front().find("scene entry renamed: first -> renamed"), std::string::npos);
    ASSERT_FALSE(report.steps[7].messages.empty());
    EXPECT_NE(report.steps[7].messages.front().find("equal=0"), std::string::npos);
}

TEST(AutomationScript, SceneRemoveEntrySupportsIndexVariant)
{
    ScriptBatchHarness harness;
    ScriptContext context;
    const ScriptRunReport report = harness.runScript(
        "mesh.make_triangle size=1\n"
        "scene.new name=test_scene\n"
        "scene.add_mesh_entry name=first\n"
        "mesh.make_triangle size=2\n"
        "scene.add_mesh_entry name=second\n"
        "scene.remove_entry index=0\n"
        "scene.hash\n",
        context);

    EXPECT_TRUE(report.valid);
    ASSERT_EQ(report.steps.size(), 7u);
    ASSERT_FALSE(report.steps[5].messages.empty());
    EXPECT_NE(report.steps[5].messages.front().find("index=0"), std::string::npos);
    ASSERT_FALSE(report.steps[6].messages.empty());
    EXPECT_NE(report.steps[6].messages.front().find("scene hash=0x"), std::string::npos);
}

TEST(AutomationScript, SceneRenameAndRemoveRejectMissingEntry)
{
    ScriptBatchHarness harness;
    ScriptContext context;
    const ScriptRunReport renameReport = harness.runScript(
        "scene.new name=test_scene\n"
        "scene.rename_entry from=missing to=renamed\n",
        context);

    EXPECT_FALSE(renameReport.valid);
    ASSERT_EQ(renameReport.steps.size(), 2u);
    EXPECT_FALSE(renameReport.steps.back().success);
    ASSERT_FALSE(renameReport.steps.back().messages.empty());
    EXPECT_NE(renameReport.steps.back().messages.front().find("missing entry"), std::string::npos);

    ScriptContext renameInvalidIndexContext;
    const ScriptRunReport renameInvalidIndexReport = harness.runScript(
        "scene.new name=test_scene\n"
        "scene.rename_entry index=-1 to=renamed\n",
        renameInvalidIndexContext);

    EXPECT_FALSE(renameInvalidIndexReport.valid);
    ASSERT_EQ(renameInvalidIndexReport.steps.size(), 2u);
    EXPECT_FALSE(renameInvalidIndexReport.steps.back().success);
    ASSERT_FALSE(renameInvalidIndexReport.steps.back().messages.empty());
    EXPECT_NE(renameInvalidIndexReport.steps.back().messages.front().find("invalid index"), std::string::npos);

    ScriptContext renameOutOfRangeContext;
    const ScriptRunReport renameOutOfRangeReport = harness.runScript(
        "scene.new name=test_scene\n"
        "scene.rename_entry index=0 to=renamed\n",
        renameOutOfRangeContext);

    EXPECT_FALSE(renameOutOfRangeReport.valid);
    ASSERT_EQ(renameOutOfRangeReport.steps.size(), 2u);
    EXPECT_FALSE(renameOutOfRangeReport.steps.back().success);
    ASSERT_FALSE(renameOutOfRangeReport.steps.back().messages.empty());
    EXPECT_NE(renameOutOfRangeReport.steps.back().messages.front().find("out of range"), std::string::npos);

    ScriptContext removeContext;
    const ScriptRunReport removeReport = harness.runScript(
        "scene.new name=test_scene\n"
        "scene.remove_entry name=missing\n",
        removeContext);

    EXPECT_FALSE(removeReport.valid);
    ASSERT_EQ(removeReport.steps.size(), 2u);
    EXPECT_FALSE(removeReport.steps.back().success);
    ASSERT_FALSE(removeReport.steps.back().messages.empty());
    EXPECT_NE(removeReport.steps.back().messages.front().find("missing entry"), std::string::npos);

    ScriptContext invalidIndexContext;
    const ScriptRunReport invalidIndexReport = harness.runScript(
        "scene.new name=test_scene\n"
        "scene.remove_entry index=-1\n",
        invalidIndexContext);

    EXPECT_FALSE(invalidIndexReport.valid);
    ASSERT_EQ(invalidIndexReport.steps.size(), 2u);
    EXPECT_FALSE(invalidIndexReport.steps.back().success);
    ASSERT_FALSE(invalidIndexReport.steps.back().messages.empty());
    EXPECT_NE(invalidIndexReport.steps.back().messages.front().find("invalid index"), std::string::npos);

    ScriptContext outOfRangeContext;
    const ScriptRunReport outOfRangeReport = harness.runScript(
        "scene.new name=test_scene\n"
        "scene.remove_entry index=0\n",
        outOfRangeContext);

    EXPECT_FALSE(outOfRangeReport.valid);
    ASSERT_EQ(outOfRangeReport.steps.size(), 2u);
    EXPECT_FALSE(outOfRangeReport.steps.back().success);
    ASSERT_FALSE(outOfRangeReport.steps.back().messages.empty());
    EXPECT_NE(outOfRangeReport.steps.back().messages.front().find("out of range"), std::string::npos);
}

TEST(AutomationScript, SceneHashIsDeterministicAndDiffDetectsChange)
{
    ScriptBatchHarness harness;
    ScriptContext context;
    const ScriptRunReport report = harness.runScript(
        "mesh.make_triangle size=1\n"
        "scene.new name=test_scene\n"
        "scene.add_mesh_entry name=tri\n"
        "scene.hash\n"
        "scene.hash\n"
        "scene.set_baseline\n"
        "scene.diff\n"
        "scene.add_mesh_entry name=tri2\n"
        "scene.diff\n",
        context);

    EXPECT_TRUE(report.valid);
    ASSERT_EQ(report.steps.size(), 9u);
    // Hash is deterministic — two calls on same scene state produce identical output.
    ASSERT_FALSE(report.steps[3].messages.empty());
    ASSERT_FALSE(report.steps[4].messages.empty());
    EXPECT_EQ(report.steps[3].messages.front(), report.steps[4].messages.front());
    EXPECT_NE(report.steps[3].messages.front().find("scene hash=0x"), std::string::npos);
    // First diff is against unchanged baseline.
    ASSERT_FALSE(report.steps[6].messages.empty());
    EXPECT_NE(report.steps[6].messages.front().find("equal=1"), std::string::npos);
    // Second diff detects a scene mutation.
    ASSERT_FALSE(report.steps[8].messages.empty());
    EXPECT_NE(report.steps[8].messages.front().find("equal=0"), std::string::npos);
}

TEST(AutomationScript, SceneDiffRequiresBaseline)
{
    ScriptBatchHarness harness;
    ScriptContext context;
    const ScriptRunReport report = harness.runScript(
        "mesh.make_triangle size=1\n"
        "scene.new name=test_scene\n"
        "scene.add_mesh_entry name=tri\n"
        "scene.diff\n",
        context);

    EXPECT_FALSE(report.valid);
    ASSERT_EQ(report.steps.size(), 4u);
    EXPECT_FALSE(report.steps.back().success);
    ASSERT_FALSE(report.steps.back().messages.empty());
    EXPECT_NE(report.steps.back().messages.front().find("scene.set_baseline"), std::string::npos);
}

TEST(AutomationScript, SceneExportAndVerifyBundleRoundTrip)
{
    const fs::path outDir = tempPath("scene_bundle_verify");
    const fs::path bundlePath = outDir / "scene_bundle.json";
    fs::create_directories(outDir);

    ScriptBatchHarness harness;
    ScriptContext context;
    const ScriptRunReport report = harness.runScript(
        "mesh.make_triangle size=1\n"
        "scene.new name=test_scene\n"
        "scene.add_mesh_entry name=tri\n"
        "scene.export_bundle path=" + bundlePath.string() + "\n"
        "scene.verify_bundle path=" + bundlePath.string() + "\n",
        context);

    EXPECT_TRUE(report.valid);
    ASSERT_EQ(report.steps.size(), 5u);
    EXPECT_TRUE(report.steps.back().success);
    ASSERT_FALSE(report.steps.back().messages.empty());
    EXPECT_NE(report.steps.back().messages.front().find("match:"), std::string::npos);

    fs::remove(bundlePath);
    fs::remove_all(outDir);
}

TEST(AutomationScript, SceneVerifyBundleRejectsMissingOrInvalidSceneHash)
{
    const fs::path outDir = tempPath("scene_bundle_verify_malformed");
    const fs::path bundlePath = outDir / "scene_bundle.json";
    fs::create_directories(outDir);

    {
        std::ofstream out(bundlePath, std::ios::trunc);
        ASSERT_TRUE(out.good());
        out << "{\"entry_count\":1,\"scene_name\":\"test_scene\"}";
        ASSERT_TRUE(out.good());
    }

    ScriptBatchHarness harness;
    ScriptContext context;
    const ScriptRunReport missingHashReport = harness.runScript(
        "mesh.make_triangle size=1\n"
        "scene.new name=test_scene\n"
        "scene.add_mesh_entry name=tri\n"
        "scene.verify_bundle path=" + bundlePath.string() + "\n",
        context);

    EXPECT_FALSE(missingHashReport.valid);
    ASSERT_EQ(missingHashReport.steps.size(), 4u);
    EXPECT_FALSE(missingHashReport.steps.back().success);
    ASSERT_FALSE(missingHashReport.steps.back().messages.empty());
    EXPECT_NE(missingHashReport.steps.back().messages.front().find("missing scene_hash"), std::string::npos);

    {
        std::ofstream out(bundlePath, std::ios::trunc);
        ASSERT_TRUE(out.good());
        out << "{\"scene_hash\":123,\"entry_count\":1,\"scene_name\":\"test_scene\"}";
        ASSERT_TRUE(out.good());
    }

    ScriptContext invalidContext;
    const ScriptRunReport invalidHashReport = harness.runScript(
        "mesh.make_triangle size=1\n"
        "scene.new name=test_scene\n"
        "scene.add_mesh_entry name=tri\n"
        "scene.verify_bundle path=" + bundlePath.string() + "\n",
        invalidContext);

    EXPECT_FALSE(invalidHashReport.valid);
    ASSERT_EQ(invalidHashReport.steps.size(), 4u);
    EXPECT_FALSE(invalidHashReport.steps.back().success);
    ASSERT_FALSE(invalidHashReport.steps.back().messages.empty());
    EXPECT_NE(invalidHashReport.steps.back().messages.front().find("missing scene_hash"), std::string::npos);

    fs::remove(bundlePath);
    fs::remove_all(outDir);
}

TEST(AutomationScript, SceneVerifyBundleFailsAfterEntryAdded)
{
    const fs::path outDir = tempPath("scene_bundle_verify_fail");
    const fs::path bundlePath = outDir / "scene_bundle.json";
    fs::create_directories(outDir);

    ScriptBatchHarness harness;
    ScriptContext context;
    const ScriptRunReport report = harness.runScript(
        "mesh.make_triangle size=1\n"
        "scene.new name=test_scene\n"
        "scene.add_mesh_entry name=tri\n"
        "scene.export_bundle path=" + bundlePath.string() + "\n"
        "scene.add_mesh_entry name=tri2\n"
        "scene.verify_bundle path=" + bundlePath.string() + "\n",
        context);

    EXPECT_FALSE(report.valid);
    ASSERT_EQ(report.steps.size(), 6u);
    EXPECT_FALSE(report.steps.back().success);
    ASSERT_FALSE(report.steps.back().messages.empty());
    EXPECT_NE(report.steps.back().messages.front().find("mismatch"), std::string::npos);

    fs::remove(bundlePath);
    fs::remove_all(outDir);
}

TEST(AutomationScript, SceneVerifyBundleFailsAfterEntryRenamedByIndex)
{
    const fs::path outDir = tempPath("scene_bundle_verify_rename_index_fail");
    const fs::path bundlePath = outDir / "scene_bundle.json";
    fs::create_directories(outDir);

    ScriptBatchHarness harness;
    ScriptContext context;
    const ScriptRunReport report = harness.runScript(
        "mesh.make_triangle size=1\n"
        "scene.new name=test_scene\n"
        "scene.add_mesh_entry name=tri\n"
        "scene.export_bundle path=" + bundlePath.string() + "\n"
        "scene.rename_entry index=0 to=tri_renamed\n"
        "scene.verify_bundle path=" + bundlePath.string() + "\n",
        context);

    EXPECT_FALSE(report.valid);
    ASSERT_EQ(report.steps.size(), 6u);
    EXPECT_FALSE(report.steps.back().success);
    ASSERT_FALSE(report.steps.back().messages.empty());
    EXPECT_NE(report.steps.back().messages.front().find("mismatch"), std::string::npos);

    fs::remove(bundlePath);
    fs::remove_all(outDir);
}

TEST(AutomationScript, SceneVerifyBundleFailsAfterEntryRenamed)
{
    const fs::path outDir = tempPath("scene_bundle_verify_rename_fail");
    const fs::path bundlePath = outDir / "scene_bundle.json";
    fs::create_directories(outDir);

    ScriptBatchHarness harness;
    ScriptContext context;
    const ScriptRunReport report = harness.runScript(
        "mesh.make_triangle size=1\n"
        "scene.new name=test_scene\n"
        "scene.add_mesh_entry name=tri\n"
        "scene.export_bundle path=" + bundlePath.string() + "\n"
        "scene.rename_entry from=tri to=tri_renamed\n"
        "scene.verify_bundle path=" + bundlePath.string() + "\n",
        context);

    EXPECT_FALSE(report.valid);
    ASSERT_EQ(report.steps.size(), 6u);
    EXPECT_FALSE(report.steps.back().success);
    ASSERT_FALSE(report.steps.back().messages.empty());
    EXPECT_NE(report.steps.back().messages.front().find("mismatch"), std::string::npos);

    fs::remove(bundlePath);
    fs::remove_all(outDir);
}

TEST(AutomationScript, SceneVerifyBundleFailsAfterEntryRemovedByIndex)
{
    const fs::path outDir = tempPath("scene_bundle_verify_remove_index_fail");
    const fs::path bundlePath = outDir / "scene_bundle.json";
    fs::create_directories(outDir);

    ScriptBatchHarness harness;
    ScriptContext context;
    const ScriptRunReport report = harness.runScript(
        "mesh.make_triangle size=1\n"
        "scene.new name=test_scene\n"
        "scene.add_mesh_entry name=first\n"
        "mesh.make_triangle size=2\n"
        "scene.add_mesh_entry name=second\n"
        "scene.export_bundle path=" + bundlePath.string() + "\n"
        "scene.remove_entry index=0\n"
        "scene.verify_bundle path=" + bundlePath.string() + "\n",
        context);

    EXPECT_FALSE(report.valid);
    ASSERT_EQ(report.steps.size(), 8u);
    EXPECT_FALSE(report.steps.back().success);
    ASSERT_FALSE(report.steps.back().messages.empty());
    EXPECT_NE(report.steps.back().messages.front().find("mismatch"), std::string::npos);

    fs::remove(bundlePath);
    fs::remove_all(outDir);
}

TEST(AutomationScript, SceneHashIsIndependentOfEntryInsertionOrder)
{
    ScriptBatchHarness firstHarness;
    ScriptContext firstContext;
    const ScriptRunReport firstReport = firstHarness.runScript(
        "scene.new name=test_scene\n"
        "mesh.make_triangle size=1\n"
        "scene.add_mesh_entry name=beta\n"
        "mesh.make_triangle size=2\n"
        "scene.add_mesh_entry name=alpha\n"
        "scene.hash\n",
        firstContext);

    ScriptBatchHarness secondHarness;
    ScriptContext secondContext;
    const ScriptRunReport secondReport = secondHarness.runScript(
        "scene.new name=test_scene\n"
        "mesh.make_triangle size=2\n"
        "scene.add_mesh_entry name=alpha\n"
        "mesh.make_triangle size=1\n"
        "scene.add_mesh_entry name=beta\n"
        "scene.hash\n",
        secondContext);

    ASSERT_TRUE(firstReport.valid);
    ASSERT_TRUE(secondReport.valid);
    ASSERT_EQ(firstReport.steps.size(), 6u);
    ASSERT_EQ(secondReport.steps.size(), 6u);
    ASSERT_FALSE(firstReport.steps.back().messages.empty());
    ASSERT_FALSE(secondReport.steps.back().messages.empty());
    EXPECT_EQ(firstReport.steps.back().messages.front(), secondReport.steps.back().messages.front());
}

// ── animation.state_hash / animation.set_baseline / animation.diff / animation.expect_hash ─

TEST(AutomationScript, AnimationHashCommandsAreRegistered)
{
    ScriptBatchHarness harness;
    EXPECT_TRUE(harness.registry().hasCommand("animation.state_hash"));
    EXPECT_TRUE(harness.registry().hasCommand("animation.set_baseline"));
    EXPECT_TRUE(harness.registry().hasCommand("animation.diff"));
    EXPECT_TRUE(harness.registry().hasCommand("animation.expect_hash"));
    EXPECT_TRUE(harness.registry().hasCommand("animation.export_bundle"));
    EXPECT_TRUE(harness.registry().hasCommand("animation.verify_bundle"));
}

TEST(AutomationScript, AnimationHashIsDeterministicAndDiffRuns)
{
    ScriptBatchHarness harness;
    ScriptContext context;
    const ScriptRunReport report = harness.runScript(
        "animation.add_bone name=root parent=-1\n"
        "animation.add_bone name=child parent=0 tx=0 ty=1 tz=0\n"
        "animation.sample_bind_pose\n"
        "animation.state_hash\n"
        "animation.state_hash\n"
        "animation.set_baseline\n"
        "animation.reset_pose\n"
        "animation.diff\n",
        context);

    EXPECT_TRUE(report.valid);
    ASSERT_EQ(report.steps.size(), 8u);
    // Hash is deterministic — two consecutive calls on the same skeleton+pose match.
    ASSERT_FALSE(report.steps[3].messages.empty());
    ASSERT_FALSE(report.steps[4].messages.empty());
    EXPECT_EQ(report.steps[3].messages.front(), report.steps[4].messages.front());
    EXPECT_NE(report.steps[3].messages.front().find("animation hash=0x"), std::string::npos);
    // diff command runs without error.
    ASSERT_FALSE(report.steps[7].messages.empty());
    EXPECT_NE(report.steps[7].messages.front().find("animation diff"), std::string::npos);
}

TEST(AutomationScript, AnimationExpectHashDetectsMismatch)
{
    ScriptBatchHarness harness;
    ScriptContext context;
    const ScriptRunReport report = harness.runScript(
        "animation.add_bone name=root parent=-1\n"
        "animation.sample_bind_pose\n"
        "animation.expect_hash hash=0x1\n",
        context);

    EXPECT_FALSE(report.valid);
    ASSERT_EQ(report.steps.size(), 3u);
    EXPECT_FALSE(report.steps[2].success);
    ASSERT_FALSE(report.steps[2].messages.empty());
    EXPECT_NE(report.steps[2].messages.front().find("mismatch"), std::string::npos);
}

TEST(AutomationScript, AnimationExportAndVerifyBundleRoundTrip)
{
    const fs::path outDir = tempPath("animation_bundle_verify");
    const fs::path bundlePath = outDir / "animation_bundle.json";
    fs::create_directories(outDir);

    ScriptBatchHarness harness;
    ScriptContext context;
    const ScriptRunReport report = harness.runScript(
        "animation.add_bone name=root parent=-1\n"
        "animation.sample_bind_pose\n"
        "animation.export_bundle path=" + bundlePath.string() + "\n"
        "animation.verify_bundle path=" + bundlePath.string() + "\n",
        context);

    EXPECT_TRUE(report.valid);
    ASSERT_EQ(report.steps.size(), 4u);
    EXPECT_TRUE(report.steps.back().success);
    ASSERT_FALSE(report.steps.back().messages.empty());
    EXPECT_NE(report.steps.back().messages.front().find("match:"), std::string::npos);

    fs::remove(bundlePath);
    fs::remove_all(outDir);
}

TEST(AutomationScript, AnimationVerifyBundleRejectsMissingOrInvalidHash)
{
    const fs::path outDir = tempPath("animation_bundle_verify_malformed");
    const fs::path bundlePath = outDir / "animation_bundle.json";
    fs::create_directories(outDir);

    {
        std::ofstream out(bundlePath, std::ios::trunc);
        ASSERT_TRUE(out.good());
        out << "{\"bone_count\":1}";
        ASSERT_TRUE(out.good());
    }

    ScriptBatchHarness harness;
    ScriptContext context;
    const ScriptRunReport missingHashReport = harness.runScript(
        "animation.add_bone name=root parent=-1\n"
        "animation.sample_bind_pose\n"
        "animation.verify_bundle path=" + bundlePath.string() + "\n",
        context);

    EXPECT_FALSE(missingHashReport.valid);
    ASSERT_EQ(missingHashReport.steps.size(), 3u);
    EXPECT_FALSE(missingHashReport.steps.back().success);
    ASSERT_FALSE(missingHashReport.steps.back().messages.empty());
    EXPECT_NE(missingHashReport.steps.back().messages.front().find("missing animation_hash"), std::string::npos);

    {
        std::ofstream out(bundlePath, std::ios::trunc);
        ASSERT_TRUE(out.good());
        out << "{\"animation_hash\":123,\"bone_count\":1}";
        ASSERT_TRUE(out.good());
    }

    ScriptContext invalidContext;
    const ScriptRunReport invalidHashReport = harness.runScript(
        "animation.add_bone name=root parent=-1\n"
        "animation.sample_bind_pose\n"
        "animation.verify_bundle path=" + bundlePath.string() + "\n",
        invalidContext);

    EXPECT_FALSE(invalidHashReport.valid);
    ASSERT_EQ(invalidHashReport.steps.size(), 3u);
    EXPECT_FALSE(invalidHashReport.steps.back().success);
    ASSERT_FALSE(invalidHashReport.steps.back().messages.empty());
    EXPECT_NE(invalidHashReport.steps.back().messages.front().find("missing animation_hash"), std::string::npos);

    fs::remove(bundlePath);
    fs::remove_all(outDir);
}

TEST(AutomationScript, AnimationVerifyBundleFailsAfterSkeletonMutation)
{
    const fs::path outDir = tempPath("animation_bundle_verify_fail");
    const fs::path bundlePath = outDir / "animation_bundle.json";
    fs::create_directories(outDir);

    ScriptBatchHarness harness;
    ScriptContext context;
    const ScriptRunReport report = harness.runScript(
        "animation.add_bone name=root parent=-1\n"
        "animation.sample_bind_pose\n"
        "animation.export_bundle path=" + bundlePath.string() + "\n"
        "animation.add_bone name=child parent=0 tx=0 ty=1 tz=0\n"
        "animation.verify_bundle path=" + bundlePath.string() + "\n",
        context);

    EXPECT_FALSE(report.valid);
    ASSERT_EQ(report.steps.size(), 5u);
    EXPECT_FALSE(report.steps.back().success);
    ASSERT_FALSE(report.steps.back().messages.empty());
    EXPECT_NE(report.steps.back().messages.front().find("mismatch"), std::string::npos);

    fs::remove(bundlePath);
    fs::remove_all(outDir);
}

// ── sim.cross_solver_hash / sim.export_cross_solver_bundle / sim.verify_cross_solver_bundle ─

TEST(AutomationScript, CrossSolverHashCommandsAreRegistered)
{
    ScriptBatchHarness harness;
    EXPECT_TRUE(harness.registry().hasCommand("sim.cross_solver_hash"));
    EXPECT_TRUE(harness.registry().hasCommand("sim.export_cross_solver_bundle"));
    EXPECT_TRUE(harness.registry().hasCommand("sim.verify_cross_solver_bundle"));
}

TEST(AutomationScript, CrossSolverExportAndVerifyRoundTrip)
{
    const fs::path outDir = tempPath("cross_solver_verify");
    const fs::path bundlePath = outDir / "cross_solver_bundle.json";
    fs::create_directories(outDir);

    ScriptBatchHarness harness;
    ScriptContext context;
    const ScriptRunReport report = harness.runScript(
        "sim.rigid.create gy=0\n"
        "sim.rigid.add_body mass=1\n"
        "sim.cloth.create gy=0\n"
        "sim.cloth.add_node mass=1\n"
        "sim.fluid.create gy=0\n"
        "sim.fluid.add_particle mass=1 density=1000\n"
        "sim.cross_solver_hash\n"
        "sim.export_cross_solver_bundle path=" + bundlePath.string() + "\n"
        "sim.verify_cross_solver_bundle path=" + bundlePath.string() + "\n",
        context);

    EXPECT_TRUE(report.valid);
    ASSERT_EQ(report.steps.size(), 9u);
    EXPECT_TRUE(report.steps.back().success);
    ASSERT_FALSE(report.steps.back().messages.empty());
    EXPECT_NE(report.steps.back().messages.front().find("match:"), std::string::npos);

    fs::remove(bundlePath);
    fs::remove_all(outDir);
}

TEST(AutomationScript, CrossSolverVerifyBundleFailsAfterSolverStep)
{
    const fs::path outDir = tempPath("cross_solver_verify_fail");
    const fs::path bundlePath = outDir / "cross_solver_bundle.json";
    fs::create_directories(outDir);

    ScriptBatchHarness harness;
    ScriptContext context;
    const ScriptRunReport report = harness.runScript(
        "sim.rigid.create gy=0\n"
        "sim.rigid.add_body mass=1\n"
        "sim.cloth.create gy=0\n"
        "sim.cloth.add_node mass=1\n"
        "sim.export_cross_solver_bundle path=" + bundlePath.string() + "\n"
        "sim.rigid.step dt=0.1\n"
        "sim.verify_cross_solver_bundle path=" + bundlePath.string() + "\n",
        context);

    EXPECT_FALSE(report.valid);
    ASSERT_EQ(report.steps.size(), 7u);
    EXPECT_FALSE(report.steps.back().success);
    ASSERT_FALSE(report.steps.back().messages.empty());
    EXPECT_NE(report.steps.back().messages.front().find("mismatch"), std::string::npos);

    fs::remove(bundlePath);
    fs::remove_all(outDir);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Month 11 — Mesh Primitive Commands
// ─────────────────────────────────────────────────────────────────────────────

TEST(AutomationScript, MeshPrimitivesCommandsAreRegistered)
{
    ScriptBatchHarness harness;
    const std::vector<std::string> required = {
        "mesh.make_sphere", "mesh.make_cylinder", "mesh.make_cone", "mesh.make_capsule"
    };
    for (const auto& n : required) {
        EXPECT_TRUE(harness.registry().hasCommand(n)) << "missing: " << n;
    }
}

TEST(AutomationScript, MeshMakeSphereCreatesValidMesh)
{
    ScriptBatchHarness harness;
    ScriptContext context;
    const ScriptRunReport report = harness.runScript(
        "mesh.make_sphere radius=1 lat_segs=8 lon_segs=8\n"
        "mesh.hash\n",
        context);

    EXPECT_TRUE(report.valid);
    ASSERT_EQ(report.steps.size(), 2u);
    EXPECT_TRUE(report.steps[0].success);
    ASSERT_FALSE(report.steps[0].messages.empty());
    EXPECT_NE(report.steps[0].messages.front().find("mesh created: sphere"), std::string::npos);
    EXPECT_NE(report.steps[0].messages.front().find("vertices="), std::string::npos);
    ASSERT_FALSE(report.steps[1].messages.empty());
    EXPECT_NE(report.steps[1].messages.front().find("mesh hash=0x"), std::string::npos);
}

TEST(AutomationScript, MeshMakeCylinderCreatesValidMesh)
{
    ScriptBatchHarness harness;
    ScriptContext context;
    const ScriptRunReport report = harness.runScript(
        "mesh.make_cylinder radius=1 height=2 segs=8\n"
        "mesh.compute_normals\n"
        "mesh.hash\n",
        context);

    EXPECT_TRUE(report.valid);
    ASSERT_EQ(report.steps.size(), 3u);
    EXPECT_TRUE(report.steps[0].success);
    ASSERT_FALSE(report.steps[0].messages.empty());
    EXPECT_NE(report.steps[0].messages.front().find("mesh created: cylinder"), std::string::npos);
    ASSERT_FALSE(report.steps[2].messages.empty());
    EXPECT_NE(report.steps[2].messages.front().find("mesh hash=0x"), std::string::npos);
}

TEST(AutomationScript, MeshMakeConeCreatesValidMesh)
{
    ScriptBatchHarness harness;
    ScriptContext context;
    const ScriptRunReport report = harness.runScript(
        "mesh.make_cone radius=1 height=2 segs=8\n"
        "mesh.hash\n",
        context);

    EXPECT_TRUE(report.valid);
    ASSERT_EQ(report.steps.size(), 2u);
    EXPECT_TRUE(report.steps[0].success);
    ASSERT_FALSE(report.steps[0].messages.empty());
    EXPECT_NE(report.steps[0].messages.front().find("mesh created: cone"), std::string::npos);
}

TEST(AutomationScript, MeshMakeCapsuleCreatesValidMesh)
{
    ScriptBatchHarness harness;
    ScriptContext context;
    const ScriptRunReport report = harness.runScript(
        "mesh.make_capsule radius=0.5 height=1 segs=8 ring_segs=4\n"
        "mesh.hash\n",
        context);

    EXPECT_TRUE(report.valid);
    ASSERT_EQ(report.steps.size(), 2u);
    EXPECT_TRUE(report.steps[0].success);
    ASSERT_FALSE(report.steps[0].messages.empty());
    EXPECT_NE(report.steps[0].messages.front().find("mesh created: capsule"), std::string::npos);
}

TEST(AutomationScript, MeshPrimitivesAreDeterministic)
{
    // Two identical sphere calls produce the same hash
    ScriptBatchHarness harness;
    ScriptContext ctx1;
    ScriptContext ctx2;
    const auto r1 = harness.runScript("mesh.make_sphere radius=1 lat_segs=8 lon_segs=8\nmesh.hash\n", ctx1);
    const auto r2 = harness.runScript("mesh.make_sphere radius=1 lat_segs=8 lon_segs=8\nmesh.hash\n", ctx2);

    EXPECT_TRUE(r1.valid);
    EXPECT_TRUE(r2.valid);
    ASSERT_FALSE(r1.steps[1].messages.empty());
    ASSERT_FALSE(r2.steps[1].messages.empty());
    EXPECT_EQ(r1.steps[1].messages.front(), r2.steps[1].messages.front());
}

// ─────────────────────────────────────────────────────────────────────────────
//  Month 11 — Parametric Commands
// ─────────────────────────────────────────────────────────────────────────────

TEST(AutomationScript, ParametricCommandsAreRegistered)
{
    ScriptBatchHarness harness;
    const std::vector<std::string> required = {
        "parametric.new", "parametric.add_point", "parametric.remove_entity",
        "parametric.add_distance_constraint", "parametric.add_coincident_constraint",
        "parametric.add_axis_aligned_distance_constraint",
        "parametric.solve", "parametric.get_point", "parametric.describe",
        "parametric.hash", "parametric.set_baseline", "parametric.diff",
        "parametric.expect_hash", "parametric.serialize", "parametric.load",
        "parametric.export_bundle", "parametric.verify_bundle"
    };
    for (const auto& n : required) {
        EXPECT_TRUE(harness.registry().hasCommand(n)) << "missing: " << n;
    }
}

TEST(AutomationScript, ParametricAddPointAndDescribe)
{
    ScriptBatchHarness harness;
    ScriptContext context;
    const ScriptRunReport report = harness.runScript(
        "parametric.new\n"
        "parametric.add_point x=0 y=0 z=0\n"
        "parametric.add_point x=3 y=4 z=0\n"
        "parametric.describe\n",
        context);

    EXPECT_TRUE(report.valid);
    ASSERT_EQ(report.steps.size(), 4u);
    EXPECT_TRUE(report.steps[0].success);
    ASSERT_FALSE(report.steps[1].messages.empty());
    EXPECT_NE(report.steps[1].messages.front().find("parametric entity id="), std::string::npos);
    ASSERT_FALSE(report.steps[3].messages.empty());
    EXPECT_NE(report.steps[3].messages.front().find("entities=2"), std::string::npos);
}

TEST(AutomationScript, ParametricSolvePipelineRunsDeterministically)
{
    ScriptBatchHarness harness;
    ScriptContext context;
    // Two points with a distance constraint of 5 — should converge.
    const ScriptRunReport report = harness.runScript(
        "parametric.new\n"
        "parametric.add_point x=0 y=0 z=0\n"
        "parametric.add_point x=3 y=4 z=0\n"
        "parametric.add_distance_constraint a=1 b=2 dist=5\n"
        "parametric.solve\n"
        "parametric.describe\n",
        context);

    EXPECT_TRUE(report.valid);
    ASSERT_EQ(report.steps.size(), 6u);
    ASSERT_FALSE(report.steps[4].messages.empty());
    EXPECT_NE(report.steps[4].messages.front().find("converged=1"), std::string::npos);
    ASSERT_FALSE(report.steps[5].messages.empty());
    EXPECT_NE(report.steps[5].messages.front().find("distance_constraints=1"), std::string::npos);
}

TEST(AutomationScript, ParametricGetPointReturnsCoordinates)
{
    ScriptBatchHarness harness;
    ScriptContext context;
    const ScriptRunReport report = harness.runScript(
        "parametric.new\n"
        "parametric.add_point x=1.5 y=2.5 z=3.5\n"
        "parametric.get_point id=1\n",
        context);

    EXPECT_TRUE(report.valid);
    ASSERT_EQ(report.steps.size(), 3u);
    ASSERT_FALSE(report.steps[2].messages.empty());
    EXPECT_NE(report.steps[2].messages.front().find("parametric point x="), std::string::npos);
}

TEST(AutomationScript, ParametricGetPointFailsForMissingId)
{
    ScriptBatchHarness harness;
    ScriptContext context;
    const ScriptRunReport report = harness.runScript(
        "parametric.new\n"
        "parametric.get_point id=99\n",
        context);

    EXPECT_FALSE(report.valid);
    ASSERT_EQ(report.steps.size(), 2u);
    EXPECT_FALSE(report.steps[1].success);
    ASSERT_FALSE(report.steps[1].messages.empty());
    EXPECT_NE(report.steps[1].messages.front().find("not found"), std::string::npos);
}

TEST(AutomationScript, ParametricRequiresNewFirst)
{
    ScriptBatchHarness harness;
    ScriptContext context;
    const ScriptRunReport report = harness.runScript(
        "parametric.add_point x=0 y=0 z=0\n",
        context);

    EXPECT_FALSE(report.valid);
    ASSERT_EQ(report.steps.size(), 1u);
    EXPECT_FALSE(report.steps[0].success);
    ASSERT_FALSE(report.steps[0].messages.empty());
    EXPECT_NE(report.steps[0].messages.front().find("requires parametric.new first"), std::string::npos);
}

TEST(AutomationScript, ParametricSerializeAndLoadRoundTrip)
{
    const fs::path outDir = tempPath("parametric_serialize_roundtrip");
    const fs::path graphPath = outDir / "graph.nparam";
    fs::create_directories(outDir);

    ScriptBatchHarness harness;

    // Build and serialize
    ScriptContext buildCtx;
    const ScriptRunReport buildReport = harness.runScript(
        "parametric.new\n"
        "parametric.add_point x=0 y=0 z=0\n"
        "parametric.add_point x=5 y=0 z=0\n"
        "parametric.add_distance_constraint a=1 b=2 dist=5\n"
        "parametric.serialize path=" + graphPath.string() + "\n",
        buildCtx);

    EXPECT_TRUE(buildReport.valid);
    ASSERT_FALSE(buildReport.steps[4].messages.empty());
    EXPECT_NE(buildReport.steps[4].messages.front().find("entities=2"), std::string::npos);

    // Load and describe
    ScriptContext loadCtx;
    const ScriptRunReport loadReport = harness.runScript(
        "parametric.load path=" + graphPath.string() + "\n"
        "parametric.describe\n",
        loadCtx);

    EXPECT_TRUE(loadReport.valid);
    ASSERT_FALSE(loadReport.steps[0].messages.empty());
    EXPECT_NE(loadReport.steps[0].messages.front().find("entities=2"), std::string::npos);
    ASSERT_FALSE(loadReport.steps[1].messages.empty());
    EXPECT_NE(loadReport.steps[1].messages.front().find("distance_constraints=1"), std::string::npos);

    fs::remove(graphPath);
    fs::remove_all(outDir);
}

TEST(AutomationScript, ParametricCoincidentConstraintIsTracked)
{
    ScriptBatchHarness harness;
    ScriptContext context;
    const ScriptRunReport report = harness.runScript(
        "parametric.new\n"
        "parametric.add_point x=0 y=0 z=0\n"
        "parametric.add_point x=1 y=1 z=0\n"
        "parametric.add_coincident_constraint a=1 b=2\n"
        "parametric.describe\n",
        context);

    EXPECT_TRUE(report.valid);
    ASSERT_EQ(report.steps.size(), 5u);
    ASSERT_FALSE(report.steps[3].messages.empty());
    EXPECT_NE(report.steps[3].messages.front().find("parametric coincident constraint id="), std::string::npos);
    ASSERT_FALSE(report.steps[4].messages.empty());
    EXPECT_NE(report.steps[4].messages.front().find("coincident_constraints=1"), std::string::npos);
}

TEST(AutomationScript, ParametricAxisAlignedConstraintIsTracked)
{
    ScriptBatchHarness harness;
    ScriptContext context;
    const ScriptRunReport report = harness.runScript(
        "parametric.new\n"
        "parametric.add_point x=0 y=0 z=0\n"
        "parametric.add_point x=1 y=2 z=3\n"
        "parametric.add_axis_aligned_distance_constraint a=1 b=2 axis=x dist=2\n"
        "parametric.describe\n",
        context);

    EXPECT_TRUE(report.valid);
    ASSERT_EQ(report.steps.size(), 5u);
    ASSERT_FALSE(report.steps[3].messages.empty());
    EXPECT_NE(report.steps[3].messages.front().find("parametric axis constraint id="), std::string::npos);
    ASSERT_FALSE(report.steps[4].messages.empty());
    EXPECT_NE(report.steps[4].messages.front().find("axis_constraints=1"), std::string::npos);
}

TEST(AutomationScript, ParametricAxisAlignedConstraintRejectsInvalidAxis)
{
    ScriptBatchHarness harness;
    ScriptContext context;
    const ScriptRunReport report = harness.runScript(
        "parametric.new\n"
        "parametric.add_point x=0 y=0 z=0\n"
        "parametric.add_point x=1 y=2 z=3\n"
        "parametric.add_axis_aligned_distance_constraint a=1 b=2 axis=q dist=2\n",
        context);

    EXPECT_FALSE(report.valid);
    ASSERT_EQ(report.steps.size(), 4u);
    EXPECT_FALSE(report.steps[3].success);
    ASSERT_FALSE(report.steps[3].messages.empty());
    EXPECT_NE(report.steps[3].messages.front().find("invalid axis"), std::string::npos);
}

TEST(AutomationScript, ParametricHashIsDeterministicAndDiffDetectsChange)
{
    ScriptBatchHarness harness;
    ScriptContext hashContext;
    const ScriptRunReport hashReport = harness.runScript(
        "parametric.new\n"
        "parametric.add_point x=0 y=0 z=0\n"
        "parametric.hash\n"
        "parametric.hash\n",
        hashContext);

    EXPECT_TRUE(hashReport.valid);
    ASSERT_EQ(hashReport.steps.size(), 4u);
    ASSERT_FALSE(hashReport.steps[2].messages.empty());
    ASSERT_FALSE(hashReport.steps[3].messages.empty());
    EXPECT_EQ(hashReport.steps[2].messages.front(), hashReport.steps[3].messages.front());

    ScriptContext diffContext;
    const ScriptRunReport diffReport = harness.runScript(
        "parametric.new\n"
        "parametric.add_point x=0 y=0 z=0\n"
        "parametric.set_baseline\n"
        "parametric.add_point x=1 y=0 z=0\n"
        "parametric.diff\n",
        diffContext);

    EXPECT_TRUE(diffReport.valid);
    ASSERT_EQ(diffReport.steps.size(), 5u);
    ASSERT_FALSE(diffReport.steps[4].messages.empty());
    EXPECT_NE(diffReport.steps[4].messages.front().find("equal=0"), std::string::npos);
}

TEST(AutomationScript, ParametricExpectHashDetectsMismatch)
{
    ScriptBatchHarness harness;
    ScriptContext context;
    const ScriptRunReport report = harness.runScript(
        "parametric.new\n"
        "parametric.add_point x=0 y=0 z=0\n"
        "parametric.expect_hash hash=0x1\n",
        context);

    EXPECT_FALSE(report.valid);
    ASSERT_EQ(report.steps.size(), 3u);
    EXPECT_FALSE(report.steps.back().success);
    ASSERT_FALSE(report.steps.back().messages.empty());
    EXPECT_NE(report.steps.back().messages.front().find("mismatch"), std::string::npos);
}

TEST(AutomationScript, ParametricExportAndVerifyBundleRoundTrip)
{
    const fs::path outDir = tempPath("parametric_bundle_verify");
    const fs::path bundlePath = outDir / "parametric_bundle.json";
    fs::create_directories(outDir);

    ScriptBatchHarness harness;
    ScriptContext context;
    const ScriptRunReport report = harness.runScript(
        "parametric.new\n"
        "parametric.add_point x=0 y=0 z=0\n"
        "parametric.add_point x=1 y=0 z=0\n"
        "parametric.add_distance_constraint a=1 b=2 dist=1\n"
        "parametric.export_bundle path=" + bundlePath.string() + "\n"
        "parametric.verify_bundle path=" + bundlePath.string() + "\n",
        context);

    EXPECT_TRUE(report.valid);
    ASSERT_EQ(report.steps.size(), 6u);
    EXPECT_TRUE(report.steps.back().success);
    ASSERT_FALSE(report.steps.back().messages.empty());
    EXPECT_NE(report.steps.back().messages.front().find("match:"), std::string::npos);

    fs::remove(bundlePath);
    fs::remove_all(outDir);
}

TEST(AutomationScript, ParametricVerifyBundleRejectsMissingOrInvalidHash)
{
    const fs::path outDir = tempPath("parametric_bundle_verify_malformed");
    const fs::path bundlePath = outDir / "parametric_bundle.json";
    fs::create_directories(outDir);

    {
        std::ofstream out(bundlePath, std::ios::trunc);
        ASSERT_TRUE(out.good());
        out << "{\"entity_count\":1}";
        ASSERT_TRUE(out.good());
    }

    ScriptBatchHarness harness;
    ScriptContext context;
    const ScriptRunReport missingHashReport = harness.runScript(
        "parametric.new\n"
        "parametric.add_point x=0 y=0 z=0\n"
        "parametric.verify_bundle path=" + bundlePath.string() + "\n",
        context);

    EXPECT_FALSE(missingHashReport.valid);
    ASSERT_EQ(missingHashReport.steps.size(), 3u);
    EXPECT_FALSE(missingHashReport.steps.back().success);
    ASSERT_FALSE(missingHashReport.steps.back().messages.empty());
    EXPECT_NE(missingHashReport.steps.back().messages.front().find("missing parametric_hash"), std::string::npos);

    {
        std::ofstream out(bundlePath, std::ios::trunc);
        ASSERT_TRUE(out.good());
        out << "{\"parametric_hash\":123}";
        ASSERT_TRUE(out.good());
    }

    ScriptContext invalidContext;
    const ScriptRunReport invalidHashReport = harness.runScript(
        "parametric.new\n"
        "parametric.add_point x=0 y=0 z=0\n"
        "parametric.verify_bundle path=" + bundlePath.string() + "\n",
        invalidContext);

    EXPECT_FALSE(invalidHashReport.valid);
    ASSERT_EQ(invalidHashReport.steps.size(), 3u);
    EXPECT_FALSE(invalidHashReport.steps.back().success);
    ASSERT_FALSE(invalidHashReport.steps.back().messages.empty());
    EXPECT_NE(invalidHashReport.steps.back().messages.front().find("missing parametric_hash"), std::string::npos);

    fs::remove(bundlePath);
    fs::remove_all(outDir);
}

TEST(AutomationScript, ParametricVerifyBundleFailsAfterGraphMutation)
{
    const fs::path outDir = tempPath("parametric_bundle_verify_fail");
    const fs::path bundlePath = outDir / "parametric_bundle.json";
    fs::create_directories(outDir);

    ScriptBatchHarness harness;
    ScriptContext context;
    const ScriptRunReport report = harness.runScript(
        "parametric.new\n"
        "parametric.add_point x=0 y=0 z=0\n"
        "parametric.export_bundle path=" + bundlePath.string() + "\n"
        "parametric.add_point x=1 y=0 z=0\n"
        "parametric.verify_bundle path=" + bundlePath.string() + "\n",
        context);

    EXPECT_FALSE(report.valid);
    ASSERT_EQ(report.steps.size(), 5u);
    EXPECT_FALSE(report.steps.back().success);
    ASSERT_FALSE(report.steps.back().messages.empty());
    EXPECT_NE(report.steps.back().messages.front().find("mismatch"), std::string::npos);

    fs::remove(bundlePath);
    fs::remove_all(outDir);
}

TEST(AutomationScript, CrossDomainParametricOutputDrivesMeshGenerationPipeline)
{
    ScriptBatchHarness harness;

    ScriptContext parametricContext;
    const ScriptRunReport parametricReport = harness.runScript(
        "parametric.new\n"
        "parametric.add_point x=0 y=0 z=0\n"
        "parametric.add_point x=2 y=0 z=0\n"
        "parametric.add_distance_constraint a=1 b=2 dist=3\n"
        "parametric.solve\n"
        "parametric.get_point id=2\n",
        parametricContext);

    EXPECT_TRUE(parametricReport.valid);
    ASSERT_EQ(parametricReport.steps.size(), 6u);
    ASSERT_FALSE(parametricReport.steps[5].messages.empty());

    const std::string pointMessage = parametricReport.steps[5].messages.front();
    const size_t xPos = pointMessage.find("x=");
    ASSERT_NE(xPos, std::string::npos);
    const size_t yPos = pointMessage.find(" y=", xPos);
    ASSERT_NE(yPos, std::string::npos);
    const std::string xText = pointMessage.substr(xPos + 2, yPos - (xPos + 2));
    const double drivenHeight = std::max(0.5, std::abs(std::stod(xText)));

    ScriptContext meshContext;
    const ScriptRunReport meshReport = harness.runScript(
        "mesh.make_cylinder radius=1 height=" + std::to_string(drivenHeight) + " segs=8\n"
        "mesh.hash\n",
        meshContext);

    EXPECT_TRUE(meshReport.valid);
    ASSERT_EQ(meshReport.steps.size(), 2u);
    ASSERT_FALSE(meshReport.steps[1].messages.empty());
    EXPECT_NE(meshReport.steps[1].messages.front().find("mesh hash=0x"), std::string::npos);
}

} // namespace nexus::automation::testing
