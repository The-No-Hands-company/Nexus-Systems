#include <nexus/asset/SceneAsset.h>
#include <nexus/geometry/GeometryKernel.h>
#include <nexus/geometry/Mesh.h>

#include <gtest/gtest.h>

#include <cstdio>
#include <cmath>
#include <filesystem>
#include <string>
#include <vector>

#ifndef NEXUS_TESTS_ROOT
#define NEXUS_TESTS_ROOT "tests"
#endif

namespace nexus::asset::testing {

using namespace nexus::geometry::primitives;
namespace fs = std::filesystem;

// ─────────────────────────────────────────────────────────────────────────────
//  Helpers
// ─────────────────────────────────────────────────────────────────────────────
static std::string tmpPath(const std::string& name)
{
    return (fs::temp_directory_path() / ("nexus_test_" + name)).string();
}

static std::string fixturePath(const std::string& name)
{
    return (fs::path(NEXUS_TESTS_ROOT) / "kernel" / "fixtures" / name).string();
}

static SceneMeshEntry makeBoxEntry(const std::string& name)
{
    SceneMeshEntry e;
    e.name       = name;
    e.sourcePath = "test://box";
    e.mesh       = makeBox(1.f, 1.f, 1.f);
    return e;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Construction and entry management
// ─────────────────────────────────────────────────────────────────────────────
TEST(SceneAsset, EmptyAssetHasZeroEntries)
{
    SceneAsset asset;
    EXPECT_EQ(asset.entryCount(), 0u);
}

TEST(SceneAsset, AddEntryIncreasesCount)
{
    SceneAsset asset;
    asset.addEntry(makeBoxEntry("box_a"));
    EXPECT_EQ(asset.entryCount(), 1u);
    asset.addEntry(makeBoxEntry("box_b"));
    EXPECT_EQ(asset.entryCount(), 2u);
}

TEST(SceneAsset, FindByNameReturnsCorrectEntry)
{
    SceneAsset asset;
    asset.addEntry(makeBoxEntry("alpha"));
    asset.addEntry(makeBoxEntry("beta"));
    const SceneMeshEntry* e = asset.findByName("beta");
    ASSERT_NE(e, nullptr);
    EXPECT_EQ(e->name, "beta");
}

TEST(SceneAsset, FindByNameReturnsNullForMissingEntry)
{
    SceneAsset asset;
    asset.addEntry(makeBoxEntry("alpha"));
    EXPECT_EQ(asset.findByName("gamma"), nullptr);
}

TEST(SceneAsset, RemoveEntryDecreasesCount)
{
    SceneAsset asset;
    asset.addEntry(makeBoxEntry("a"));
    asset.addEntry(makeBoxEntry("b"));
    asset.removeEntry(0u);
    EXPECT_EQ(asset.entryCount(), 1u);
}

TEST(SceneAsset, ClearRemovesAllEntries)
{
    SceneAsset asset;
    asset.addEntry(makeBoxEntry("a"));
    asset.addEntry(makeBoxEntry("b"));
    asset.clear();
    EXPECT_EQ(asset.entryCount(), 0u);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Round-trip binary I/O
// ─────────────────────────────────────────────────────────────────────────────
TEST(SceneAsset, SaveAndLoadRoundTrip)
{
    const std::string path = tmpPath("roundtrip.nxas");

    SceneAsset saved;
    saved.setSceneName("test_scene");
    SceneMeshEntry e = makeBoxEntry("box_rt");
    e.transform.translation = {1.f, 2.f, 3.f};
    e.transform.scale       = {2.f, 2.f, 2.f};
    e.visible               = true;
    saved.addEntry(std::move(e));

    const SceneAssetIOReport saveReport = saved.save(path);
    ASSERT_TRUE(saveReport.valid) << saveReport.messages.front();

    SceneAsset loaded;
    const SceneAssetIOReport loadReport = loaded.load(path);
    ASSERT_TRUE(loadReport.valid) << loadReport.messages.front();

    EXPECT_EQ(loaded.entryCount(), saved.entryCount());
    EXPECT_EQ(loaded.sceneName(),  "test_scene");

    const SceneMeshEntry* le = loaded.findByName("box_rt");
    ASSERT_NE(le, nullptr);
    EXPECT_FLOAT_EQ(le->transform.translation.x, 1.f);
    EXPECT_FLOAT_EQ(le->transform.translation.y, 2.f);
    EXPECT_FLOAT_EQ(le->transform.translation.z, 3.f);
    EXPECT_FLOAT_EQ(le->transform.scale.x, 2.f);
    EXPECT_TRUE(le->visible);

    // Mesh geometry round-trip
    const SceneMeshEntry* se = saved.findByName("box_rt");
    ASSERT_NE(se, nullptr);
    EXPECT_EQ(le->mesh.attributes().vertexCount(),
              se->mesh.attributes().vertexCount());
    EXPECT_EQ(le->mesh.topology().faceCount(),
              se->mesh.topology().faceCount());

    std::remove(path.c_str());
}

TEST(SceneAsset, RoundTripPreservesVertexPositions)
{
    const std::string path = tmpPath("positions.nxas");

    SceneAsset saved;
    saved.addEntry(makeBoxEntry("box_pos"));

    ASSERT_TRUE(saved.save(path).valid);

    SceneAsset loaded;
    ASSERT_TRUE(loaded.load(path).valid);

    const auto& pSaved  = saved.entry(0).mesh.attributes().positions();
    const auto& pLoaded = loaded.entry(0).mesh.attributes().positions();
    ASSERT_EQ(pSaved.size(), pLoaded.size());
    for (size_t i = 0; i < pSaved.size(); ++i) {
        EXPECT_FLOAT_EQ(pSaved[i].x, pLoaded[i].x);
        EXPECT_FLOAT_EQ(pSaved[i].y, pLoaded[i].y);
        EXPECT_FLOAT_EQ(pSaved[i].z, pLoaded[i].z);
    }

    std::remove(path.c_str());
}

TEST(SceneAsset, RoundTripMultipleEntries)
{
    const std::string path = tmpPath("multi.nxas");

    SceneAsset saved;
    for (int i = 0; i < 4; ++i) {
        SceneMeshEntry e = makeBoxEntry("box_" + std::to_string(i));
        e.transform.translation = {static_cast<float>(i), 0.f, 0.f};
        saved.addEntry(std::move(e));
    }
    ASSERT_TRUE(saved.save(path).valid);

    SceneAsset loaded;
    const SceneAssetIOReport rep = loaded.load(path);
    ASSERT_TRUE(rep.valid);
    EXPECT_EQ(rep.entryCount, 4u);
    EXPECT_EQ(loaded.entryCount(), 4u);

    for (int i = 0; i < 4; ++i) {
        const std::string name = "box_" + std::to_string(i);
        const SceneMeshEntry* le = loaded.findByName(name);
        ASSERT_NE(le, nullptr) << "Missing: " << name;
        EXPECT_FLOAT_EQ(le->transform.translation.x, static_cast<float>(i));
    }

    std::remove(path.c_str());
}

// ─────────────────────────────────────────────────────────────────────────────
//  Error handling
// ─────────────────────────────────────────────────────────────────────────────
TEST(SceneAsset, LoadRejectsEmptyPath)
{
    SceneAsset asset;
    const SceneAssetIOReport rep = asset.load("");
    EXPECT_FALSE(rep.valid);
    EXPECT_TRUE(hasDiagnostic(rep.diagnostic, SceneAssetDiagnostic::FileOpenFailed));
}

TEST(SceneAsset, SaveRejectsEmptyPath)
{
    SceneAsset asset;
    const SceneAssetIOReport rep = asset.save("");
    EXPECT_FALSE(rep.valid);
    EXPECT_TRUE(hasDiagnostic(rep.diagnostic, SceneAssetDiagnostic::FileOpenFailed));
}

TEST(SceneAsset, IOMessagesAreDeterministicAndSorted)
{
    SceneAsset asset;

    const SceneAssetIOReport r1 = asset.save("");
    const SceneAssetIOReport r2 = asset.save("");

    EXPECT_FALSE(r1.valid);
    EXPECT_EQ(r1.messages, r2.messages);

    auto sorted = r1.messages;
    std::sort(sorted.begin(), sorted.end());
    EXPECT_EQ(r1.messages, sorted);
}

TEST(SceneAsset, LoadRejectsNonExistentFile)
{
    SceneAsset asset;
    const SceneAssetIOReport rep = asset.load("/tmp/nexus_nonexistent_aabbcc.nxas");
    EXPECT_FALSE(rep.valid);
    EXPECT_TRUE(hasDiagnostic(rep.diagnostic, SceneAssetDiagnostic::FileOpenFailed));
}

TEST(SceneAsset, LoadRejectsCorruptMagic)
{
    const std::string path = tmpPath("corrupt_magic.nxas");
    // Write a file with wrong magic
    std::FILE* fp = std::fopen(path.c_str(), "wb");
    ASSERT_NE(fp, nullptr);
    const uint32_t badMagic = 0xDEADBEEFu;
    std::fwrite(&badMagic, sizeof(badMagic), 1, fp);
    std::fclose(fp);

    SceneAsset asset;
    const SceneAssetIOReport rep = asset.load(path);
    EXPECT_FALSE(rep.valid);
    EXPECT_TRUE(hasDiagnostic(rep.diagnostic, SceneAssetDiagnostic::InvalidMagic));

    std::remove(path.c_str());
}

TEST(SceneAsset, LoadRejectsFutureVersion)
{
    const std::string path = tmpPath("future_version.nxas");
    std::FILE* fp = std::fopen(path.c_str(), "wb");
    ASSERT_NE(fp, nullptr);
    const uint32_t magic   = kSceneAssetMagic;
    const uint32_t version = kSceneAssetVersionCurrent + 99u;
    std::fwrite(&magic,   sizeof(magic),   1, fp);
    std::fwrite(&version, sizeof(version), 1, fp);
    std::fclose(fp);

    SceneAsset asset;
    const SceneAssetIOReport rep = asset.load(path);
    EXPECT_FALSE(rep.valid);
    EXPECT_TRUE(hasDiagnostic(rep.diagnostic, SceneAssetDiagnostic::UnsupportedVersion));

    std::remove(path.c_str());
}

TEST(SceneAsset, MigrationChainUpgradesOldVersion)
{
    const std::string path = tmpPath("migration_chain.nxas");

    SceneAsset saved;
    saved.setSceneName("migration_scene");
    saved.addEntry(makeBoxEntry("box_old"));
    ASSERT_TRUE(saved.save(path).valid);

    // Simulate a v0 asset by patching the version field in-file.
    std::FILE* fp = std::fopen(path.c_str(), "rb+");
    ASSERT_NE(fp, nullptr);
    ASSERT_EQ(std::fseek(fp, static_cast<long>(sizeof(uint32_t)), SEEK_SET), 0);
    const uint32_t v0 = 0u;
    ASSERT_EQ(std::fwrite(&v0, sizeof(v0), 1, fp), 1u);
    std::fclose(fp);

    bool migrationCalled = false;
    ASSERT_TRUE(SceneAsset::registerMigration(
        0u,
        [&migrationCalled](uint32_t fromVersion, std::vector<uint8_t>& rawBytes) {
            migrationCalled = true;
            return fromVersion == 0u && !rawBytes.empty();
        }));

    SceneAsset loaded;
    const SceneAssetIOReport rep = loaded.load(path);
    EXPECT_TRUE(rep.valid);
    EXPECT_TRUE(migrationCalled);
    EXPECT_EQ(rep.version, kSceneAssetVersionCurrent);

    std::remove(path.c_str());
}

TEST(SceneAsset, MigrationFailureReturnsError)
{
    const std::string path = tmpPath("migration_fail.nxas");

    SceneAsset saved;
    saved.addEntry(makeBoxEntry("box_old"));
    ASSERT_TRUE(saved.save(path).valid);

    std::FILE* fp = std::fopen(path.c_str(), "rb+");
    ASSERT_NE(fp, nullptr);
    ASSERT_EQ(std::fseek(fp, static_cast<long>(sizeof(uint32_t)), SEEK_SET), 0);
    const uint32_t v0 = 0u;
    ASSERT_EQ(std::fwrite(&v0, sizeof(v0), 1, fp), 1u);
    std::fclose(fp);

    ASSERT_TRUE(SceneAsset::registerMigration(
        0u,
        [](uint32_t, std::vector<uint8_t>&) {
            return false;
        }));

    SceneAsset loaded;
    const SceneAssetIOReport rep = loaded.load(path);
    EXPECT_FALSE(rep.valid);
    EXPECT_TRUE(hasDiagnostic(rep.diagnostic, SceneAssetDiagnostic::MigrationFailed));

    std::remove(path.c_str());
}

TEST(SceneAsset, NoMigrationNeededForCurrentVersion)
{
    const std::string path = tmpPath("migration_not_needed.nxas");

    SceneAsset saved;
    saved.addEntry(makeBoxEntry("box_current"));
    ASSERT_TRUE(saved.save(path).valid);

    bool migrationCalled = false;
    ASSERT_TRUE(SceneAsset::registerMigration(
        kSceneAssetVersionCurrent,
        [&migrationCalled](uint32_t, std::vector<uint8_t>&) {
            migrationCalled = true;
            return false;
        }));

    SceneAsset loaded;
    const SceneAssetIOReport rep = loaded.load(path);
    EXPECT_TRUE(rep.valid);
    EXPECT_FALSE(migrationCalled);

    std::remove(path.c_str());
}

TEST(SceneAsset, LoadsLegacyV0FixtureViaProductionMigration)
{
    const std::string path = fixturePath("scene_asset_v0_minimal.nxas");
    ASSERT_TRUE(fs::exists(path)) << "Missing fixture: " << path;

    // Other migration tests overwrite the v0 slot with test lambdas.
    // Restore production migrations so this test exercises the real transform.
    SceneAsset::resetBuiltinMigrations();

    SceneAsset loaded;
    const SceneAssetIOReport rep = loaded.load(path);

    ASSERT_TRUE(rep.valid) << (rep.messages.empty() ? "" : rep.messages.front());
    EXPECT_EQ(rep.version, kSceneAssetVersionCurrent);
    ASSERT_EQ(loaded.entryCount(), 1u);
    EXPECT_EQ(loaded.sceneName(), "legacy_scene");

    const SceneMeshEntry* entry = loaded.findByName("legacy_box");
    ASSERT_NE(entry, nullptr);
    EXPECT_EQ(entry->sourcePath, "legacy://box");
    EXPECT_TRUE(entry->visible);
    EXPECT_EQ(entry->mesh.attributes().vertexCount(), 3u);
    EXPECT_EQ(entry->mesh.topology().faceCount(), 1u);
    EXPECT_TRUE(entry->mesh.isValid());
}

// ─────────────────────────────────────────────────────────────────────────────
//  Text export
// ─────────────────────────────────────────────────────────────────────────────
TEST(SceneAsset, TextExportProducesFile)
{
    const std::string path = tmpPath("export_text.txt");

    SceneAsset asset;
    asset.setSceneName("my_scene");
    asset.addEntry(makeBoxEntry("box_text"));

    const SceneAssetIOReport rep = asset.exportText(path);
    ASSERT_TRUE(rep.valid);
    EXPECT_TRUE(fs::exists(path));
    EXPECT_GT(fs::file_size(path), 0u);

    std::remove(path.c_str());
}

TEST(SceneAsset, TextExportRejectsEmptyPath)
{
    SceneAsset asset;
    const SceneAssetIOReport rep = asset.exportText("");
    EXPECT_FALSE(rep.valid);
    EXPECT_TRUE(hasDiagnostic(rep.diagnostic, SceneAssetDiagnostic::FileOpenFailed));
}

// ─────────────────────────────────────────────────────────────────────────────
//  Determinism
// ─────────────────────────────────────────────────────────────────────────────
TEST(SceneAsset, DeterministicSave)
{
    const std::string pathA = tmpPath("det_a.nxas");
    const std::string pathB = tmpPath("det_b.nxas");

    auto makeAsset = []() {
        SceneAsset a;
        a.setSceneName("det_scene");
        a.addEntry(makeBoxEntry("box1"));
        a.addEntry(makeBoxEntry("box2"));
        return a;
    };

    SceneAsset assetA = makeAsset();
    SceneAsset assetB = makeAsset();

    ASSERT_TRUE(assetA.save(pathA).valid);
    ASSERT_TRUE(assetB.save(pathB).valid);

    // Compare file sizes — byte-for-byte identical content implies identical sizes at minimum.
    EXPECT_EQ(fs::file_size(pathA), fs::file_size(pathB));

    std::remove(pathA.c_str());
    std::remove(pathB.c_str());
}

TEST(SceneAsset, LoadPackageResolvesDependenciesDeterministically)
{
    const std::string pathA = tmpPath("pkg_a.nxas");
    const std::string pathB = tmpPath("pkg_b.nxas");
    const std::string pathC = tmpPath("pkg_c.nxas");

    SceneAsset a;
    a.setSceneName("A");
    a.addEntry(makeBoxEntry("a"));
    ASSERT_TRUE(a.save(pathA).valid);

    SceneAsset b;
    b.setSceneName("B");
    b.addEntry(makeBoxEntry("b"));
    ASSERT_TRUE(b.save(pathB).valid);

    SceneAsset c;
    c.setSceneName("C");
    c.addEntry(makeBoxEntry("c"));
    ASSERT_TRUE(c.save(pathC).valid);

    std::vector<SceneAssetPackageEntry> packageEntries;
    packageEntries.push_back(SceneAssetPackageEntry{pathC, "C", {pathA, pathB}});
    packageEntries.push_back(SceneAssetPackageEntry{pathB, "B", {pathA}});
    packageEntries.push_back(SceneAssetPackageEntry{pathA, "A", {}});

    std::map<std::string, SceneAsset> loaded;
    const SceneAssetPackageReport report = SceneAsset::loadPackage(packageEntries, loaded);

    ASSERT_TRUE(report.valid);
    ASSERT_TRUE(report.dependencyReport.valid());
    ASSERT_EQ(report.loadOrder.size(), 3u);
    EXPECT_EQ(report.loadOrder[0], pathA);
    EXPECT_EQ(report.loadOrder[1], pathB);
    EXPECT_EQ(report.loadOrder[2], pathC);

    ASSERT_EQ(loaded.size(), 3u);
    EXPECT_EQ(loaded.at(pathA).sceneName(), "A");
    EXPECT_EQ(loaded.at(pathB).sceneName(), "B");
    EXPECT_EQ(loaded.at(pathC).sceneName(), "C");

    std::remove(pathA.c_str());
    std::remove(pathB.c_str());
    std::remove(pathC.c_str());
}

TEST(SceneAsset, LoadPackageFailsOnDependencyCycleBeforeFileLoads)
{
    const std::string pathA = tmpPath("pkg_cycle_a.nxas");
    const std::string pathB = tmpPath("pkg_cycle_b.nxas");

    SceneAsset a;
    a.setSceneName("A");
    a.addEntry(makeBoxEntry("a"));
    ASSERT_TRUE(a.save(pathA).valid);

    SceneAsset b;
    b.setSceneName("B");
    b.addEntry(makeBoxEntry("b"));
    ASSERT_TRUE(b.save(pathB).valid);

    std::vector<SceneAssetPackageEntry> packageEntries;
    packageEntries.push_back(SceneAssetPackageEntry{pathA, "A", {pathB}});
    packageEntries.push_back(SceneAssetPackageEntry{pathB, "B", {pathA}});

    std::map<std::string, SceneAsset> loaded;
    const SceneAssetPackageReport report = SceneAsset::loadPackage(packageEntries, loaded);

    EXPECT_FALSE(report.valid);
    EXPECT_EQ(report.dependencyReport.status, DependencyResolutionStatus::CycleDetected);
    EXPECT_TRUE(loaded.empty());
    EXPECT_TRUE(report.loadReports.empty());

    std::remove(pathA.c_str());
    std::remove(pathB.c_str());
}

TEST(SceneAsset, LoadPackageMessagesAreLexicographicallySorted)
{
    std::vector<SceneAssetPackageEntry> packageEntries;
    packageEntries.push_back(SceneAssetPackageEntry{"", "invalid", {"missing_a.nxas"}});
    packageEntries.push_back(SceneAssetPackageEntry{"dup.nxas", "dup1", {"missing_b.nxas"}});
    packageEntries.push_back(SceneAssetPackageEntry{"dup.nxas", "dup2", {"missing_c.nxas"}});

    std::map<std::string, SceneAsset> loaded;
    const SceneAssetPackageReport report = SceneAsset::loadPackage(packageEntries, loaded);

    EXPECT_FALSE(report.messages.empty());
    auto sorted = report.messages;
    std::sort(sorted.begin(), sorted.end());
    EXPECT_EQ(report.messages, sorted);
}

TEST(SceneAsset, SaveAndLoadPackageManifestRoundTrip)
{
    const std::string path = tmpPath("pkg_manifest_roundtrip.nxpk");

    SceneAssetPackageDescriptor pkg;
    pkg.entries = {
        SceneAssetPackageEntry{"/tmp/a.nxas", "A", {}},
        SceneAssetPackageEntry{"/tmp/b.nxas", "B", {"/tmp/a.nxas"}},
        SceneAssetPackageEntry{"/tmp/c.nxas", "C", {"/tmp/a.nxas", "/tmp/b.nxas"}},
    };

    const SceneAssetPackageIOReport saveRep = SceneAsset::savePackageManifest(pkg, path);
    ASSERT_TRUE(saveRep.valid);
    EXPECT_EQ(saveRep.version, kSceneAssetPackageVersionCurrent);
    EXPECT_EQ(saveRep.entryCount, 3u);

    SceneAssetPackageDescriptor loaded;
    const SceneAssetPackageIOReport loadRep = SceneAsset::loadPackageManifest(path, loaded);
    ASSERT_TRUE(loadRep.valid);
    EXPECT_EQ(loadRep.version, kSceneAssetPackageVersionCurrent);
    ASSERT_EQ(loaded.entries.size(), pkg.entries.size());

    for (size_t i = 0; i < pkg.entries.size(); ++i) {
        EXPECT_EQ(loaded.entries[i].path, pkg.entries[i].path);
        EXPECT_EQ(loaded.entries[i].name, pkg.entries[i].name);
        EXPECT_EQ(loaded.entries[i].dependsOn, pkg.entries[i].dependsOn);
    }

    std::remove(path.c_str());
}

TEST(SceneAsset, SavePackageManifestRejectsDuplicateEntryPaths)
{
    const std::string path = tmpPath("pkg_manifest_duplicate_entry_paths.nxpk");

    SceneAssetPackageDescriptor pkg;
    pkg.entries = {
        SceneAssetPackageEntry{"/tmp/dup_entry_a.nxas", "A", {}},
        SceneAssetPackageEntry{"/tmp/dup_entry_a.nxas", "A_DUP", {}},
    };

    const SceneAssetPackageIOReport rep = SceneAsset::savePackageManifest(pkg, path);
    EXPECT_FALSE(rep.valid);
    EXPECT_TRUE(hasPackageDiagnostic(rep.diagnostic, SceneAssetPackageDiagnostic::WriteError));
}

TEST(SceneAsset, SavePackageManifestRejectsInvalidDependencyFields)
{
    const std::string path = tmpPath("pkg_manifest_invalid_dependency_fields.nxpk");

    SceneAssetPackageDescriptor pkg;
    pkg.entries = {
        SceneAssetPackageEntry{"/tmp/self_dep.nxas", "self_dep", {"/tmp/self_dep.nxas"}},
    };

    const SceneAssetPackageIOReport rep = SceneAsset::savePackageManifest(pkg, path);
    EXPECT_FALSE(rep.valid);
    EXPECT_TRUE(hasPackageDiagnostic(rep.diagnostic, SceneAssetPackageDiagnostic::WriteError));
}

TEST(SceneAsset, LoadPackageManifestRejectsEmptyAliasInPayload)
{
    const std::string path = tmpPath("pkg_manifest_empty_alias_payload.nxpk");

    std::FILE* fp = std::fopen(path.c_str(), "wb");
    ASSERT_NE(fp, nullptr);
    const auto writeU32 = [&](uint32_t v) {
        return std::fwrite(&v, sizeof(v), 1, fp) == 1u;
    };
    const auto writeString = [&](const std::string& s) {
        const uint32_t len = static_cast<uint32_t>(s.size()) + 1u;
        if (!writeU32(len)) {
            return false;
        }
        return std::fwrite(s.c_str(), 1, len, fp) == len;
    };

    ASSERT_TRUE(writeU32(kSceneAssetPackageMagic));
    ASSERT_TRUE(writeU32(kSceneAssetPackageVersionCurrent));
    ASSERT_TRUE(writeU32(kSceneAssetPackageVersionCurrent));
    ASSERT_TRUE(writeU32(static_cast<uint32_t>(SceneAssetPackageMigrationModeFlags::Strict)));
    ASSERT_TRUE(writeU32(1u));
    ASSERT_TRUE(writeString("/tmp/empty_alias_a.nxas"));
    ASSERT_TRUE(writeString(""));
    ASSERT_TRUE(writeU32(0u));
    std::fclose(fp);

    SceneAssetPackageDescriptor loaded;
    const SceneAssetPackageIOReport rep = SceneAsset::loadPackageManifest(path, loaded);
    EXPECT_FALSE(rep.valid);
    EXPECT_TRUE(hasPackageDiagnostic(rep.diagnostic, SceneAssetPackageDiagnostic::InvalidData));

    std::remove(path.c_str());
}

} // namespace nexus::asset::testing
