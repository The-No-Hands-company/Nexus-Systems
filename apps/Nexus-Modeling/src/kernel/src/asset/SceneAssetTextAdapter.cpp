#include <nexus/asset/SceneAssetTextAdapter.h>

#include <algorithm>
#include <bit>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <string>
#include <vector>

namespace nexus::asset {

namespace {

bool isFiniteFloat(float value) noexcept
{
    const std::uint32_t bits = std::bit_cast<std::uint32_t>(value);
    return (bits & 0x7F800000u) != 0x7F800000u;
}

void sortMessages(SceneAssetIOReport& report)
{
    std::sort(report.messages.begin(), report.messages.end());
}

bool parseUInt32(const std::string& text, uint32_t& out) noexcept
{
    errno = 0;
    char* end = nullptr;
    const unsigned long value = std::strtoul(text.c_str(), &end, 10);
    if (errno != 0 || end == text.c_str() || *end != '\0') {
        return false;
    }
    if (value > std::numeric_limits<uint32_t>::max()) {
        return false;
    }
    out = static_cast<uint32_t>(value);
    return true;
}

bool parseFloat(const std::string& text, float& out) noexcept
{
    errno = 0;
    char* end = nullptr;
    out = std::strtof(text.c_str(), &end);
    if (errno != 0 || end == text.c_str() || *end != '\0') {
        return false;
    }
    if (!isFiniteFloat(out)) {
        return false;
    }
    return true;
}

std::vector<std::string> splitTabs(const std::string& line)
{
    std::vector<std::string> tokens;
    size_t start = 0;
    while (start <= line.size()) {
        const size_t pos = line.find('\t', start);
        if (pos == std::string::npos) {
            tokens.push_back(line.substr(start));
            break;
        }
        tokens.push_back(line.substr(start, pos - start));
        start = pos + 1;
    }
    return tokens;
}

std::string escapeText(const std::string& input)
{
    std::string out;
    out.reserve(input.size());
    for (char c : input) {
        switch (c) {
        case '\\': out += "\\\\"; break;
        case '\t': out += "\\t"; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        default: out.push_back(c); break;
        }
    }
    return out;
}

bool unescapeText(const std::string& input, std::string& out)
{
    out.clear();
    out.reserve(input.size());

    for (size_t i = 0; i < input.size(); ++i) {
        const char c = input[i];
        if (c != '\\') {
            out.push_back(c);
            continue;
        }
        if (i + 1 >= input.size()) {
            return false;
        }
        const char n = input[++i];
        switch (n) {
        case '\\': out.push_back('\\'); break;
        case 't': out.push_back('\t'); break;
        case 'n': out.push_back('\n'); break;
        case 'r': out.push_back('\r'); break;
        default: return false;
        }
    }

    return true;
}

} // namespace

SceneAssetIOReport SceneAssetTextAdapter::exportScene(const SceneAsset& scene,
                                                      const std::string& path) noexcept
{
    SceneAssetIOReport report{};

    if (path.empty()) {
        report.diagnostic = SceneAssetDiagnostic::FileOpenFailed;
        report.messages.push_back("Path is empty");
        sortMessages(report);
        return report;
    }

    std::FILE* fp = std::fopen(path.c_str(), "w");
    if (!fp) {
        report.diagnostic = SceneAssetDiagnostic::FileOpenFailed;
        report.messages.push_back("Failed to open file for writing: " + path);
        sortMessages(report);
        return report;
    }

    auto fail = [&](const std::string& msg) {
        std::fclose(fp);
        report.diagnostic = SceneAssetDiagnostic::WriteError;
        report.messages.push_back(msg);
        sortMessages(report);
        return report;
    };

    if (std::fprintf(fp, "%s\n", kSceneAssetTextFormatMagic) < 0) {
        return fail("Write error: text format magic");
    }
    if (std::fprintf(fp, "scene_name\t%s\n", escapeText(scene.sceneName()).c_str()) < 0) {
        return fail("Write error: scene name");
    }
    if (std::fprintf(fp, "entry_count\t%zu\n", scene.entryCount()) < 0) {
        return fail("Write error: entry count");
    }

    for (size_t ei = 0; ei < scene.entryCount(); ++ei) {
        const SceneMeshEntry& entry = scene.entry(ei);
        const auto& positions = entry.mesh.attributes().positions();
        const size_t faceCount = entry.mesh.topology().faceCount();

        if (std::fprintf(
                fp,
                "entry\t%s\t%s\t%u\t%.9g\t%.9g\t%.9g\t%.9g\t%.9g\t%.9g\t%.9g\t%.9g\t%.9g\t%.9g\t%zu\t%zu\n",
                escapeText(entry.name).c_str(),
                escapeText(entry.sourcePath).c_str(),
                entry.visible ? 1u : 0u,
                entry.transform.translation.x,
                entry.transform.translation.y,
                entry.transform.translation.z,
                entry.transform.rotation.x,
                entry.transform.rotation.y,
                entry.transform.rotation.z,
                entry.transform.rotation.w,
                entry.transform.scale.x,
                entry.transform.scale.y,
                entry.transform.scale.z,
                positions.size(),
                faceCount) < 0) {
            return fail("Write error: entry record");
        }

        for (const auto& p : positions) {
            if (std::fprintf(fp, "v\t%.9g\t%.9g\t%.9g\n", p.x, p.y, p.z) < 0) {
                return fail("Write error: vertex record");
            }
        }

        for (size_t fi = 0; fi < faceCount; ++fi) {
            const auto& face = entry.mesh.topology().face(fi);
            if (std::fprintf(fp, "f\t%zu", face.indices.size()) < 0) {
                return fail("Write error: face header");
            }
            for (uint32_t idx : face.indices) {
                if (std::fprintf(fp, "\t%u", idx) < 0) {
                    return fail("Write error: face index");
                }
            }
            if (std::fputc('\n', fp) == EOF) {
                return fail("Write error: face newline");
            }
        }
    }

    std::fclose(fp);
    report.valid = true;
    report.version = kSceneAssetVersionCurrent;
    report.entryCount = static_cast<uint32_t>(scene.entryCount());
    sortMessages(report);
    return report;
}

SceneAssetIOReport SceneAssetTextAdapter::importScene(const std::string& path,
                                                      SceneAsset& outScene) noexcept
{
    SceneAssetIOReport report{};
    outScene.clear();
    outScene.setSceneName("");

    if (path.empty()) {
        report.diagnostic = SceneAssetDiagnostic::FileOpenFailed;
        report.messages.push_back("Path is empty");
        sortMessages(report);
        return report;
    }

    std::FILE* fp = std::fopen(path.c_str(), "r");
    if (!fp) {
        report.diagnostic = SceneAssetDiagnostic::FileOpenFailed;
        report.messages.push_back("Failed to open file for reading: " + path);
        sortMessages(report);
        return report;
    }

    std::vector<std::string> lines;
    {
        char buf[4096];
        while (std::fgets(buf, static_cast<int>(sizeof(buf)), fp)) {
            std::string line(buf);
            while (!line.empty() && (line.back() == '\n' || line.back() == '\r')) {
                line.pop_back();
            }
            lines.push_back(std::move(line));
        }
    }
    std::fclose(fp);

    auto fail = [&](SceneAssetDiagnostic diag, const std::string& msg) {
        report.diagnostic = diag;
        report.messages.push_back(msg);
        sortMessages(report);
        return report;
    };

    if (lines.empty()) {
        return fail(SceneAssetDiagnostic::ReadError, "Read error: incomplete scene text payload");
    }

    if (lines[0] != kSceneAssetTextFormatMagic) {
        return fail(SceneAssetDiagnostic::UnsupportedVersion,
                    "Unsupported scene text format magic");
    }

    if (lines.size() < 3u) {
        return fail(SceneAssetDiagnostic::ReadError, "Read error: incomplete scene text payload");
    }

    auto nameTokens = splitTabs(lines[1]);
    if (nameTokens.size() != 2u || nameTokens[0] != "scene_name") {
        return fail(SceneAssetDiagnostic::ReadError, "Read error: scene_name record");
    }

    std::string sceneName;
    if (!unescapeText(nameTokens[1], sceneName)) {
        return fail(SceneAssetDiagnostic::InvalidData, "Invalid escaped scene name");
    }
    outScene.setSceneName(std::move(sceneName));

    auto countTokens = splitTabs(lines[2]);
    if (countTokens.size() != 2u || countTokens[0] != "entry_count") {
        return fail(SceneAssetDiagnostic::ReadError, "Read error: entry_count record");
    }

    uint32_t expectedEntryCount = 0u;
    if (!parseUInt32(countTokens[1], expectedEntryCount)) {
        return fail(SceneAssetDiagnostic::InvalidData, "Invalid entry_count value");
    }

    size_t lineIndex = 3u;
    for (uint32_t ei = 0; ei < expectedEntryCount; ++ei) {
        if (lineIndex >= lines.size()) {
            return fail(SceneAssetDiagnostic::ReadError, "Read error: missing entry record");
        }

        const auto entryTokens = splitTabs(lines[lineIndex++]);
        if (entryTokens.size() != 16u || entryTokens[0] != "entry") {
            return fail(SceneAssetDiagnostic::ReadError, "Read error: malformed entry record");
        }

        SceneMeshEntry entry;
        if (!unescapeText(entryTokens[1], entry.name)) {
            return fail(SceneAssetDiagnostic::InvalidData, "Invalid escaped entry name");
        }
        if (!unescapeText(entryTokens[2], entry.sourcePath)) {
            return fail(SceneAssetDiagnostic::InvalidData, "Invalid escaped source path");
        }

        uint32_t visible = 0u;
        if (!parseUInt32(entryTokens[3], visible)) {
            return fail(SceneAssetDiagnostic::InvalidData, "Invalid visible flag");
        }
        entry.visible = (visible != 0u);

        if (!parseFloat(entryTokens[4], entry.transform.translation.x) ||
            !parseFloat(entryTokens[5], entry.transform.translation.y) ||
            !parseFloat(entryTokens[6], entry.transform.translation.z) ||
            !parseFloat(entryTokens[7], entry.transform.rotation.x) ||
            !parseFloat(entryTokens[8], entry.transform.rotation.y) ||
            !parseFloat(entryTokens[9], entry.transform.rotation.z) ||
            !parseFloat(entryTokens[10], entry.transform.rotation.w) ||
            !parseFloat(entryTokens[11], entry.transform.scale.x) ||
            !parseFloat(entryTokens[12], entry.transform.scale.y) ||
            !parseFloat(entryTokens[13], entry.transform.scale.z)) {
            return fail(SceneAssetDiagnostic::InvalidData, "Invalid transform scalar in entry record");
        }

        uint32_t vertexCount = 0u;
        uint32_t faceCount = 0u;
        if (!parseUInt32(entryTokens[14], vertexCount) ||
            !parseUInt32(entryTokens[15], faceCount)) {
            return fail(SceneAssetDiagnostic::InvalidData, "Invalid topology counts in entry record");
        }

        std::vector<nexus::render::Vec3> positions;
        positions.reserve(vertexCount);
        for (uint32_t vi = 0; vi < vertexCount; ++vi) {
            if (lineIndex >= lines.size()) {
                return fail(SceneAssetDiagnostic::ReadError, "Read error: missing vertex record");
            }
            const auto vTokens = splitTabs(lines[lineIndex++]);
            if (vTokens.size() != 4u || vTokens[0] != "v") {
                return fail(SceneAssetDiagnostic::ReadError, "Read error: malformed vertex record");
            }

            nexus::render::Vec3 p{};
            if (!parseFloat(vTokens[1], p.x) || !parseFloat(vTokens[2], p.y) || !parseFloat(vTokens[3], p.z)) {
                return fail(SceneAssetDiagnostic::InvalidData, "Invalid vertex scalar");
            }
            positions.push_back(p);
        }
        if (!positions.empty()) {
            entry.mesh.attributes().setPositions(std::move(positions));
        }

        for (uint32_t fi = 0; fi < faceCount; ++fi) {
            if (lineIndex >= lines.size()) {
                return fail(SceneAssetDiagnostic::ReadError, "Read error: missing face record");
            }
            const auto fTokens = splitTabs(lines[lineIndex++]);
            if (fTokens.size() < 2u || fTokens[0] != "f") {
                return fail(SceneAssetDiagnostic::ReadError, "Read error: malformed face record");
            }

            uint32_t iCount = 0u;
            if (!parseUInt32(fTokens[1], iCount)) {
                return fail(SceneAssetDiagnostic::InvalidData, "Invalid face index count");
            }
            if (fTokens.size() != static_cast<size_t>(iCount) + 2u) {
                return fail(SceneAssetDiagnostic::InvalidData, "Face index count does not match payload");
            }

            nexus::geometry::Face face;
            face.indices.resize(iCount);
            for (uint32_t j = 0u; j < iCount; ++j) {
                if (!parseUInt32(fTokens[static_cast<size_t>(j) + 2u], face.indices[j])) {
                    return fail(SceneAssetDiagnostic::InvalidData, "Invalid face index value");
                }
            }
            entry.mesh.topology().addFace(std::move(face));
        }

        outScene.addEntry(std::move(entry));
    }

    if (lineIndex != lines.size()) {
        return fail(SceneAssetDiagnostic::InvalidData, "Unexpected trailing text payload");
    }

    report.valid = true;
    report.version = kSceneAssetVersionCurrent;
    report.entryCount = static_cast<uint32_t>(outScene.entryCount());
    sortMessages(report);
    return report;
}

} // namespace nexus::asset
