#include <nexus/asset/SceneAssetImporter.h>
#include <nexus/geometry/GeometryKernel.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace nexus::asset::testing {

namespace fs = std::filesystem;
using namespace nexus::geometry::primitives;

namespace {

std::string tmpPath(const std::string& name)
{
    return (fs::temp_directory_path() / ("nexus_scene_importer_" + name)).string();
}

SceneMeshEntry makeBoxEntry(const std::string& name)
{
    SceneMeshEntry e;
    e.name = name;
    e.sourcePath = "test://importer";
    e.mesh = makeBox(1.f, 1.f, 1.f);
    return e;
}

void saveSceneFile(const std::string& path, const std::string& sceneName)
{
    SceneAsset scene;
    scene.setSceneName(sceneName);
    scene.addEntry(makeBoxEntry(sceneName + "_entry"));
    const SceneAssetIOReport rep = scene.save(path);
    ASSERT_TRUE(rep.valid);
}

} // namespace

TEST(SceneAssetImporter, ImportSceneLoadsSingleFile)
{
    const std::string path = tmpPath("single.nxas");
    saveSceneFile(path, "single_scene");

    SceneAsset loaded;
    const SceneAssetIOReport rep = SceneAssetImporter::importScene(path, loaded);

    ASSERT_TRUE(rep.valid);
    EXPECT_EQ(loaded.sceneName(), "single_scene");
    EXPECT_EQ(loaded.entryCount(), 1u);

    std::remove(path.c_str());
}

TEST(SceneAssetImporter, ImportScenesSequentialModeUsesInputOrder)
{
    const std::string pathA = tmpPath("seq_a.nxas");
    const std::string pathB = tmpPath("seq_b.nxas");
    const std::string pathC = tmpPath("seq_c.nxas");

    saveSceneFile(pathA, "A");
    saveSceneFile(pathB, "B");
    saveSceneFile(pathC, "C");

    std::vector<SceneAssetPackageEntry> packageEntries{
        SceneAssetPackageEntry{pathC, "C", {pathA, pathB}},
        SceneAssetPackageEntry{pathA, "A", {}},
        SceneAssetPackageEntry{pathB, "B", {pathA}},
    };

    std::map<std::string, SceneAsset> loaded;
    SceneAssetImportOptions options{};
    options.dependencyDrivenMultiScene = false;

    const SceneAssetPackageReport rep =
        SceneAssetImporter::importScenes(packageEntries, loaded, options);

    ASSERT_TRUE(rep.valid);
    ASSERT_EQ(rep.loadOrder.size(), 3u);
    EXPECT_EQ(rep.loadOrder[0], pathC);
    EXPECT_EQ(rep.loadOrder[1], pathA);
    EXPECT_EQ(rep.loadOrder[2], pathB);

    ASSERT_EQ(loaded.size(), 3u);
    EXPECT_EQ(loaded.at(pathA).sceneName(), "A");
    EXPECT_EQ(loaded.at(pathB).sceneName(), "B");
    EXPECT_EQ(loaded.at(pathC).sceneName(), "C");

    std::remove(pathA.c_str());
    std::remove(pathB.c_str());
    std::remove(pathC.c_str());
}

TEST(SceneAssetImporter, ImportScenesDependencyModeResolvesDependencyOrder)
{
    const std::string pathA = tmpPath("dep_a.nxas");
    const std::string pathB = tmpPath("dep_b.nxas");
    const std::string pathC = tmpPath("dep_c.nxas");

    saveSceneFile(pathA, "A");
    saveSceneFile(pathB, "B");
    saveSceneFile(pathC, "C");

    std::vector<SceneAssetPackageEntry> packageEntries{
        SceneAssetPackageEntry{pathC, "C", {pathA, pathB}},
        SceneAssetPackageEntry{pathB, "B", {pathA}},
        SceneAssetPackageEntry{pathA, "A", {}},
    };

    std::map<std::string, SceneAsset> loaded;
    SceneAssetImportOptions options{};
    options.dependencyDrivenMultiScene = true;

    const SceneAssetPackageReport rep =
        SceneAssetImporter::importScenes(packageEntries, loaded, options);

    ASSERT_TRUE(rep.valid);
    ASSERT_TRUE(rep.dependencyReport.valid());
    ASSERT_EQ(rep.loadOrder.size(), 3u);
    EXPECT_EQ(rep.loadOrder[0], pathA);
    EXPECT_EQ(rep.loadOrder[1], pathB);
    EXPECT_EQ(rep.loadOrder[2], pathC);

    std::remove(pathA.c_str());
    std::remove(pathB.c_str());
    std::remove(pathC.c_str());
}

TEST(SceneAssetImporter, ImportScenesPathListSequentialUsesInputOrder)
{
    const std::string pathA = tmpPath("paths_seq_a.nxas");
    const std::string pathB = tmpPath("paths_seq_b.nxas");
    const std::string pathC = tmpPath("paths_seq_c.nxas");

    saveSceneFile(pathA, "A");
    saveSceneFile(pathB, "B");
    saveSceneFile(pathC, "C");

    const std::vector<std::string> paths{pathC, pathA, pathB};

    std::map<std::string, SceneAsset> loaded;
    SceneAssetImportOptions options{};
    options.dependencyDrivenMultiScene = false;

    const SceneAssetPackageReport rep =
        SceneAssetImporter::importScenes(paths, loaded, options);

    ASSERT_TRUE(rep.valid);
    ASSERT_EQ(rep.loadOrder.size(), 3u);
    EXPECT_EQ(rep.loadOrder[0], pathC);
    EXPECT_EQ(rep.loadOrder[1], pathA);
    EXPECT_EQ(rep.loadOrder[2], pathB);

    ASSERT_EQ(loaded.size(), 3u);
    EXPECT_EQ(loaded.at(pathA).sceneName(), "A");
    EXPECT_EQ(loaded.at(pathB).sceneName(), "B");
    EXPECT_EQ(loaded.at(pathC).sceneName(), "C");

    std::remove(pathA.c_str());
    std::remove(pathB.c_str());
    std::remove(pathC.c_str());
}

TEST(SceneAssetImporter, ImportScenesPathListDependencyModeIsDeterministic)
{
    const std::string pathA = tmpPath("paths_dep_a.nxas");
    const std::string pathB = tmpPath("paths_dep_b.nxas");
    const std::string pathC = tmpPath("paths_dep_c.nxas");

    saveSceneFile(pathA, "A");
    saveSceneFile(pathB, "B");
    saveSceneFile(pathC, "C");

    const std::vector<std::string> paths{pathC, pathB, pathA};

    std::map<std::string, SceneAsset> loaded;
    SceneAssetImportOptions options{};
    options.dependencyDrivenMultiScene = true;

    const SceneAssetPackageReport rep =
        SceneAssetImporter::importScenes(paths, loaded, options);

    ASSERT_TRUE(rep.valid);
    ASSERT_TRUE(rep.dependencyReport.valid());
    ASSERT_EQ(rep.loadOrder.size(), 3u);
    EXPECT_EQ(rep.loadOrder[0], pathA);
    EXPECT_EQ(rep.loadOrder[1], pathB);
    EXPECT_EQ(rep.loadOrder[2], pathC);

    std::remove(pathA.c_str());
    std::remove(pathB.c_str());
    std::remove(pathC.c_str());
}

TEST(SceneAssetImporter, ImportScenesSequentialModeRejectsDuplicatePaths)
{
    const std::string pathA = tmpPath("dup_a.nxas");
    const std::string pathB = tmpPath("dup_b.nxas");

    saveSceneFile(pathA, "A");
    saveSceneFile(pathB, "B");

    std::vector<SceneAssetPackageEntry> packageEntries{
        SceneAssetPackageEntry{pathA, "A", {}},
        SceneAssetPackageEntry{pathA, "A_DUP", {}},
        SceneAssetPackageEntry{pathB, "B", {}},
    };

    std::map<std::string, SceneAsset> loaded;
    SceneAssetImportOptions options{};
    options.dependencyDrivenMultiScene = false;

    const SceneAssetPackageReport rep =
        SceneAssetImporter::importScenes(packageEntries, loaded, options);

    EXPECT_FALSE(rep.valid);
    ASSERT_EQ(rep.loadOrder.size(), 2u);
    EXPECT_EQ(rep.loadOrder[0], pathA);
    EXPECT_EQ(rep.loadOrder[1], pathB);
    ASSERT_EQ(rep.loadedPaths.size(), 2u);
    EXPECT_EQ(rep.loadedPaths[0], pathA);
    EXPECT_EQ(rep.loadedPaths[1], pathB);
    ASSERT_EQ(loaded.size(), 2u);
    EXPECT_TRUE(loaded.contains(pathA));
    EXPECT_TRUE(loaded.contains(pathB));
    EXPECT_TRUE(std::find(rep.failedPaths.begin(), rep.failedPaths.end(), pathA) != rep.failedPaths.end());

    std::remove(pathA.c_str());
    std::remove(pathB.c_str());
}

} // namespace nexus::asset::testing
