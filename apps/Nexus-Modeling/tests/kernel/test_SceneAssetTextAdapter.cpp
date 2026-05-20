#include <nexus/asset/SceneAssetTextAdapter.h>
#include <nexus/geometry/GeometryKernel.h>

#include <gtest/gtest.h>

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

namespace nexus::asset::testing {

namespace fs = std::filesystem;
using namespace nexus::geometry::primitives;

namespace {

std::string tmpPath(const std::string& name)
{
    return (fs::temp_directory_path() / ("nexus_scene_text_adapter_" + name)).string();
}

SceneMeshEntry makeBoxEntry(const std::string& name)
{
    SceneMeshEntry e;
    e.name = name;
    e.sourcePath = "test://scene_text";
    e.mesh = makeBox(1.f, 1.f, 1.f);
    return e;
}

std::string readWholeFile(const std::string& path)
{
    std::ifstream in(path);
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

} // namespace

TEST(SceneAssetTextAdapter, RoundTripPreservesCoreSceneData)
{
    const std::string path = tmpPath("roundtrip.nxscene.txt");

    SceneAsset src;
    src.setSceneName("demo scene");

    SceneMeshEntry e = makeBoxEntry("box_a");
    e.transform.translation = {1.f, 2.f, 3.f};
    e.visible = false;
    src.addEntry(std::move(e));

    const SceneAssetIOReport exportRep = SceneAssetTextAdapter::exportScene(src, path);
    ASSERT_TRUE(exportRep.valid);

    SceneAsset dst;
    const SceneAssetIOReport importRep = SceneAssetTextAdapter::importScene(path, dst);
    ASSERT_TRUE(importRep.valid);

    EXPECT_EQ(dst.sceneName(), src.sceneName());
    ASSERT_EQ(dst.entryCount(), src.entryCount());

    const SceneMeshEntry* out = dst.findByName("box_a");
    ASSERT_NE(out, nullptr);
    EXPECT_FALSE(out->visible);
    EXPECT_FLOAT_EQ(out->transform.translation.x, 1.f);
    EXPECT_FLOAT_EQ(out->transform.translation.y, 2.f);
    EXPECT_FLOAT_EQ(out->transform.translation.z, 3.f);
    EXPECT_EQ(out->mesh.attributes().vertexCount(), src.entry(0).mesh.attributes().vertexCount());
    EXPECT_EQ(out->mesh.topology().faceCount(), src.entry(0).mesh.topology().faceCount());

    std::remove(path.c_str());
}

TEST(SceneAssetTextAdapter, ExportIsDeterministicForSameInput)
{
    const std::string pathA = tmpPath("det_a.nxscene.txt");
    const std::string pathB = tmpPath("det_b.nxscene.txt");

    SceneAsset asset;
    asset.setSceneName("det_scene");
    asset.addEntry(makeBoxEntry("box_1"));
    asset.addEntry(makeBoxEntry("box_2"));

    ASSERT_TRUE(SceneAssetTextAdapter::exportScene(asset, pathA).valid);
    ASSERT_TRUE(SceneAssetTextAdapter::exportScene(asset, pathB).valid);

    const std::string a = readWholeFile(pathA);
    const std::string b = readWholeFile(pathB);
    EXPECT_EQ(a, b);

    std::remove(pathA.c_str());
    std::remove(pathB.c_str());
}

TEST(SceneAssetTextAdapter, ImportRejectsInvalidMagic)
{
    const std::string path = tmpPath("bad_magic.nxscene.txt");
    {
        std::ofstream out(path);
        out << "BAD_MAGIC\n";
    }

    SceneAsset scene;
    const SceneAssetIOReport rep = SceneAssetTextAdapter::importScene(path, scene);
    EXPECT_FALSE(rep.valid);
    EXPECT_TRUE(hasDiagnostic(rep.diagnostic, SceneAssetDiagnostic::UnsupportedVersion));

    std::remove(path.c_str());
}

TEST(SceneAssetTextAdapter, ImportRejectsNonFiniteTransformScalars)
{
    const std::string path = tmpPath("non_finite_transform.nxscene.txt");
    {
        std::ofstream out(path);
        out << "NEXUS_SCENE_TEXT_V1\n";
        out << "scene_name\tbad_scene\n";
        out << "entry_count\t1\n";
        out << "entry\tmesh\tsource\t1\t0\t0\tnan\t0\t0\t0\t1\t1\t1\t1\t0\t0\n";
    }

    SceneAsset scene;
    const SceneAssetIOReport rep = SceneAssetTextAdapter::importScene(path, scene);

    EXPECT_FALSE(rep.valid);
    EXPECT_TRUE(hasDiagnostic(rep.diagnostic, SceneAssetDiagnostic::InvalidData));
    ASSERT_FALSE(rep.messages.empty());
    EXPECT_NE(rep.messages.front().find("Invalid transform scalar"), std::string::npos);

    std::remove(path.c_str());
}

} // namespace nexus::asset::testing
