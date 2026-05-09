#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <set>
#include <string>
#include <vector>

namespace nexus::testing {

namespace fs = std::filesystem;

namespace {

std::vector<std::string> readManifest(const fs::path& path)
{
    std::ifstream in(path);
    std::vector<std::string> lines;
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) {
            continue;
        }
        lines.push_back(line);
    }
    std::sort(lines.begin(), lines.end());
    return lines;
}

std::vector<std::string> scanPublicHeaders(const fs::path& repoRoot)
{
    const fs::path includeRoot = repoRoot / "src" / "kernel" / "include" / "nexus";
    std::vector<std::string> headers;
    for (const auto& entry : fs::recursive_directory_iterator(includeRoot)) {
        if (!entry.is_regular_file()) {
            continue;
        }
        if (entry.path().extension() != ".h") {
            continue;
        }
        headers.push_back(fs::relative(entry.path(), repoRoot).generic_string());
    }
    std::sort(headers.begin(), headers.end());
    return headers;
}

} // namespace

TEST(ApiFreezeAudit, PublicHeaderManifestMatchesWorkspace)
{
    const fs::path testsRoot = fs::path(NEXUS_TESTS_ROOT);
    const fs::path repoRoot = testsRoot.parent_path();
    const fs::path manifestPath = testsRoot / "kernel" / "fixtures" / "api_surface_manifest_alpha_v1.txt";

    ASSERT_TRUE(fs::exists(manifestPath)) << "missing manifest: " << manifestPath;

    const std::vector<std::string> manifest = readManifest(manifestPath);
    const std::vector<std::string> actual = scanPublicHeaders(repoRoot);

    std::vector<std::string> missing;
    std::vector<std::string> unexpected;

    std::set_difference(manifest.begin(), manifest.end(),
                        actual.begin(), actual.end(),
                        std::back_inserter(missing));
    std::set_difference(actual.begin(), actual.end(),
                        manifest.begin(), manifest.end(),
                        std::back_inserter(unexpected));

    EXPECT_TRUE(missing.empty()) << "public headers missing from workspace scan";
    EXPECT_TRUE(unexpected.empty()) << "public headers added without manifest update";

    if (!missing.empty() || !unexpected.empty()) {
        for (const auto& path : missing) {
            ADD_FAILURE() << "missing header: " << path;
        }
        for (const auto& path : unexpected) {
            ADD_FAILURE() << "unexpected header: " << path;
        }
    }

    EXPECT_EQ(actual.size(), manifest.size())
        << "public API header count drifted; update manifest intentionally if additive";
}

} // namespace nexus::testing
