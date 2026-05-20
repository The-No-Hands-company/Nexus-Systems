#include <nexus/asset/SceneAssetImporter.h>

#include <set>

namespace nexus::asset {

SceneAssetIOReport SceneAssetImporter::importScene(const std::string& path,
                                                   SceneAsset& outScene) noexcept
{
    return outScene.load(path);
}

SceneAssetPackageReport SceneAssetImporter::importScenes(
    const std::vector<SceneAssetPackageEntry>& packageEntries,
    std::map<std::string, SceneAsset>& outScenes,
    const SceneAssetImportOptions& options) noexcept
{
    if (options.dependencyDrivenMultiScene) {
        return SceneAsset::loadPackage(packageEntries, outScenes);
    }

    SceneAssetPackageReport report{};
    outScenes.clear();
    std::set<std::string> seenPaths;

    for (const SceneAssetPackageEntry& entry : packageEntries) {
        if (entry.path.empty()) {
            report.messages.push_back("Package entry has empty path");
            report.failedPaths.push_back(entry.path);
            continue;
        }
        if (!seenPaths.insert(entry.path).second) {
            report.messages.push_back("Duplicate package entry path: " + entry.path);
            report.failedPaths.push_back(entry.path);
            continue;
        }

        SceneAsset scene;
        const SceneAssetIOReport loadReport = scene.load(entry.path);
        report.loadReports[entry.path] = loadReport;
        report.loadOrder.push_back(entry.path);

        if (!loadReport.valid) {
            report.failedPaths.push_back(entry.path);
            continue;
        }

        report.loadedPaths.push_back(entry.path);
        outScenes.emplace(entry.path, std::move(scene));
    }

    report.valid = report.failedPaths.empty();
    if (!report.valid) {
        report.messages.push_back("One or more scene assets failed to load in package");
    }
    return report;
}

SceneAssetPackageReport SceneAssetImporter::importScenes(
    const std::vector<std::string>& paths,
    std::map<std::string, SceneAsset>& outScenes,
    const SceneAssetImportOptions& options) noexcept
{
    std::vector<SceneAssetPackageEntry> packageEntries;
    packageEntries.reserve(paths.size());

    for (const std::string& path : paths) {
        packageEntries.push_back(SceneAssetPackageEntry{path, path, {}});
    }

    return importScenes(packageEntries, outScenes, options);
}

SceneAssetPackageReport SceneAssetImporter::importPackageManifest(
    const std::string& manifestPath,
    std::map<std::string, SceneAsset>& outScenes,
    const SceneAssetImportOptions& options) noexcept
{
    SceneAssetPackageReport report{};
    outScenes.clear();

    SceneAssetPackageDescriptor descriptor;
    const SceneAssetPackageIOReport ioRep = SceneAsset::loadPackageManifest(
        manifestPath,
        descriptor,
        options.packageMigrationPolicy);
    if (!ioRep.valid) {
        report.valid = false;
        report.messages = ioRep.messages;
        report.failedPaths.push_back(manifestPath);
        return report;
    }

    return importScenes(descriptor.entries, outScenes, options);
}

} // namespace nexus::asset
