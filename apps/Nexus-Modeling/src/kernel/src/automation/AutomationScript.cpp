// ─────────────────────────────────────────────────────────────────────────────
//  Nexus Automation — Scripting and Batch Pipeline v0
// ─────────────────────────────────────────────────────────────────────────────
#include <nexus/automation/AutomationScript.h>

#include <nexus/gfx/GaussianSplatting.h>
#include <nexus/parametric/ConstraintGraph.h>
#include <nexus/parametric/ParametricSerialization.h>
#include <nexus/parametric/ParametricSolver.h>
#include <nexus/sim/ClothSolver.h>
#include <nexus/sim/FluidSolver.h>
#include <nexus/sim/SimulationCore.h>

#include <algorithm>
#include <charconv>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <sstream>
#include <utility>

namespace nexus::automation {
namespace {

[[nodiscard]] nexus::render::Mat4 makeTranslationMatrix(float x, float y, float z) noexcept
{
    nexus::render::Mat4 m = nexus::render::Mat4::identity();
    m.m[0][3] = x;
    m.m[1][3] = y;
    m.m[2][3] = z;
    return m;
}

[[nodiscard]] nexus::render::Mat4 makeScaleMatrix(float x, float y, float z) noexcept
{
    nexus::render::Mat4 m = nexus::render::Mat4::identity();
    m.m[0][0] = x;
    m.m[1][1] = y;
    m.m[2][2] = z;
    return m;
}

[[nodiscard]] std::string trimCopy(std::string_view text)
{
    size_t begin = 0;
    size_t end = text.size();
    while (begin < end && std::isspace(static_cast<unsigned char>(text[begin]))) {
        ++begin;
    }
    while (end > begin && std::isspace(static_cast<unsigned char>(text[end - 1]))) {
        --end;
    }
    return std::string(text.substr(begin, end - begin));
}

[[nodiscard]] std::vector<std::string> tokenizeLine(std::string_view line)
{
    std::vector<std::string> tokens;
    size_t i = 0;
    while (i < line.size()) {
        while (i < line.size() && std::isspace(static_cast<unsigned char>(line[i]))) {
            ++i;
        }
        if (i >= line.size()) {
            break;
        }
        if (line[i] == '#') {
            break;
        }

        std::string token;
        if (line[i] == '"') {
            ++i;
            while (i < line.size()) {
                const char c = line[i++];
                if (c == '"') {
                    break;
                }
                if (c == '\\' && i < line.size()) {
                    token.push_back(line[i++]);
                } else {
                    token.push_back(c);
                }
            }
        } else {
            while (i < line.size() && !std::isspace(static_cast<unsigned char>(line[i]))) {
                if (line[i] == '#') {
                    break;
                }
                token.push_back(line[i++]);
            }
        }

        if (!token.empty()) {
            tokens.push_back(std::move(token));
        }

        while (i < line.size() && !std::isspace(static_cast<unsigned char>(line[i]))) {
            if (line[i] == '#') {
                i = line.size();
                break;
            }
            ++i;
        }
    }
    return tokens;
}

[[nodiscard]] bool parseKeyValueToken(const std::string& token,
                                      std::string& key,
                                      std::string& value)
{
    const size_t eq = token.find('=');
    if (eq == std::string::npos || eq == 0 || eq + 1 >= token.size()) {
        return false;
    }
    key = token.substr(0, eq);
    value = token.substr(eq + 1);
    return true;
}

[[nodiscard]] std::optional<std::string> getArg(const ScriptCommand& command, std::string_view key)
{
    auto it = command.args.find(std::string(key));
    if (it == command.args.end()) {
        return std::nullopt;
    }
    return it->second;
}

[[nodiscard]] std::optional<float> parseFloatArg(const ScriptCommand& command, std::string_view key)
{
    const auto text = getArg(command, key);
    if (!text) {
        return std::nullopt;
    }
    char* end = nullptr;
    const float value = std::strtof(text->c_str(), &end);
    if (end == text->c_str() || *end != '\0') {
        return std::nullopt;
    }
    return value;
}

[[nodiscard]] std::optional<int32_t> parseIntArg(const ScriptCommand& command, std::string_view key)
{
    const auto text = getArg(command, key);
    if (!text) {
        return std::nullopt;
    }
    int32_t value = 0;
    const auto first = text->data();
    const auto last = text->data() + text->size();
    const auto result = std::from_chars(first, last, value);
    if (result.ec != std::errc{} || result.ptr != last) {
        return std::nullopt;
    }
    return value;
}

[[nodiscard]] std::optional<double> parseDoubleArg(const ScriptCommand& command, std::string_view key)
{
    const auto text = getArg(command, key);
    if (!text) {
        return std::nullopt;
    }
    char* end = nullptr;
    const double value = std::strtod(text->c_str(), &end);
    if (end == text->c_str() || *end != '\0') {
        return std::nullopt;
    }
    return value;
}

[[nodiscard]] std::string makePathString(const std::filesystem::path& path)
{
    return path.lexically_normal().string();
}

[[nodiscard]] bool writeBytesToFile(const std::filesystem::path& path,
                                    const std::vector<uint8_t>& bytes,
                                    std::vector<std::string>& messages)
{
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out.good()) {
        messages.push_back("failed to open output file: " + makePathString(path));
        return false;
    }
    if (!bytes.empty()) {
        out.write(reinterpret_cast<const char*>(bytes.data()),
                  static_cast<std::streamsize>(bytes.size()));
    }
    if (!out.good()) {
        messages.push_back("failed to write output file: " + makePathString(path));
        return false;
    }
    return true;
}

[[nodiscard]] bool readBytesFromFile(const std::filesystem::path& path,
                                     std::vector<uint8_t>& outBytes,
                                     std::vector<std::string>& messages)
{
    std::ifstream in(path, std::ios::binary);
    if (!in.good()) {
        messages.push_back("failed to open input file: " + makePathString(path));
        return false;
    }

    in.seekg(0, std::ios::end);
    const auto endPos = in.tellg();
    if (endPos < 0) {
        messages.push_back("failed to determine input file size: " + makePathString(path));
        return false;
    }
    const auto size = static_cast<size_t>(endPos);
    in.seekg(0, std::ios::beg);

    outBytes.resize(size);
    if (size > 0) {
        in.read(reinterpret_cast<char*>(outBytes.data()), static_cast<std::streamsize>(size));
        if (!in.good()) {
            messages.push_back("failed to read input file: " + makePathString(path));
            return false;
        }
    }
    return true;
}

[[nodiscard]] uint64_t hashBytesFnv1a64(const std::vector<uint8_t>& bytes) noexcept
{
    uint64_t h = 14695981039346656037ull;
    for (const uint8_t b : bytes) {
        h ^= static_cast<uint64_t>(b);
        h *= 1099511628211ull;
    }
    return h;
}

[[nodiscard]] std::string hashHex(uint64_t value)
{
    std::ostringstream oss;
    oss << "0x" << std::hex << std::uppercase << value;
    return oss.str();
}

[[nodiscard]] std::optional<std::string> extractJsonStringField(std::string_view text,
                                                                std::string_view key)
{
    std::string token;
    token.reserve(key.size() + 5);
    token += '"';
    token += key;
    token += "\":\"";

    const size_t keyPos = text.find(token);
    if (keyPos == std::string_view::npos) {
        return std::nullopt;
    }

    const size_t valueStart = keyPos + token.size();
    const size_t valueEnd = text.find('"', valueStart);
    if (valueEnd == std::string_view::npos || valueEnd <= valueStart) {
        return std::nullopt;
    }

    return std::string(text.substr(valueStart, valueEnd - valueStart));
}

[[nodiscard]] std::optional<uint64_t> parseExpectedHashArg(const ScriptCommand& command)
{
    const auto textOpt = getArg(command, "hash");
    if (!textOpt || textOpt->empty()) {
        return std::nullopt;
    }
    const std::string& text = *textOpt;
    uint64_t value = 0;
    if (text.size() > 2 && text[0] == '0' && (text[1] == 'x' || text[1] == 'X')) {
        const char* first = text.data() + 2;
        const char* last = text.data() + text.size();
        const auto res = std::from_chars(first, last, value, 16);
        if (res.ec != std::errc{} || res.ptr != last) {
            return std::nullopt;
        }
        return value;
    }

    const char* first = text.data();
    const char* last = text.data() + text.size();
    const auto res = std::from_chars(first, last, value, 10);
    if (res.ec != std::errc{} || res.ptr != last) {
        return std::nullopt;
    }
    return value;
}

[[nodiscard]] size_t byteDiffCount(const std::vector<uint8_t>& a,
                                   const std::vector<uint8_t>& b) noexcept;

[[nodiscard]] size_t firstByteDiff(const std::vector<uint8_t>& a,
                                   const std::vector<uint8_t>& b) noexcept;

[[nodiscard]] std::string jsonEscape(std::string_view text)
{
    std::string out;
    out.reserve(text.size() + 8);
    for (const char c : text) {
        switch (c) {
        case '\\': out += "\\\\"; break;
        case '"':  out += "\\\""; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        default:    out.push_back(c); break;
        }
    }
    return out;
}

[[nodiscard]] std::string joinCommandsJson(const std::vector<std::string>& commands)
{
    std::string out = "[";
    for (size_t i = 0; i < commands.size(); ++i) {
        if (i > 0) {
            out += ",";
        }
        out += '"';
        out += jsonEscape(commands[i]);
        out += '"';
    }
    out += "]";
    return out;
}

[[nodiscard]] std::string serializeCommandSurfaceJson(const std::vector<std::string>& commands)
{
    std::vector<uint8_t> bytes;
    for (const auto& name : commands) {
        bytes.insert(bytes.end(), name.begin(), name.end());
        bytes.push_back(static_cast<uint8_t>('\n'));
    }

    std::ostringstream oss;
    oss << "{\"count\":" << commands.size()
        << ",\"hash\":\"" << hashHex(hashBytesFnv1a64(bytes)) << "\""
        << ",\"commands\":" << joinCommandsJson(commands)
        << "}";
    return oss.str();
}

[[nodiscard]] std::string serializeCommandSurfaceCompactJson(const std::vector<std::string>& commands)
{
    std::vector<uint8_t> bytes;
    for (const auto& name : commands) {
        bytes.insert(bytes.end(), name.begin(), name.end());
        bytes.push_back(static_cast<uint8_t>('\n'));
    }

    std::ostringstream oss;
    oss << "{\"count\":" << commands.size()
        << ",\"hash\":\"" << hashHex(hashBytesFnv1a64(bytes)) << "\""
        << "}";
    return oss.str();
}


[[nodiscard]] std::string makeDiagnosticsReportJson(const ScriptContext& context,
                                                   const std::vector<std::string>& commands)
{
    std::ostringstream oss;
    oss << "{\"command_surface\":" << serializeCommandSurfaceJson(commands);

    if (context.hasRigidSolver) {
        const auto current = context.rigidSolver->captureState();
        const auto currentBytes = serializeSimState(current);
        oss << ",\"rigid\":{"
            << "\"active\":true"
            << ",\"current\":{\"body_count\":" << current.bodies.size()
            << ",\"time\":" << current.simulationTime
            << ",\"hash\":\"" << hashHex(hashBytesFnv1a64(currentBytes)) << "\"}"
            << ",\"baseline\":{\"present\":" << (context.hasRigidLastState ? "true" : "false");
        if (context.hasRigidLastState) {
            const auto baseBytes = serializeSimState(context.rigidLastState);
            const auto changed = byteDiffCount(baseBytes, currentBytes);
            const auto firstDiff = firstByteDiff(baseBytes, currentBytes);
            oss << ",\"body_count\":" << context.rigidLastState.bodies.size()
                << ",\"time\":" << context.rigidLastState.simulationTime
                << ",\"hash\":\"" << hashHex(hashBytesFnv1a64(baseBytes)) << "\""
                << ",\"diff\":{\"equal\":" << (changed == 0 ? "true" : "false")
                << ",\"changed_bytes\":" << changed
                << ",\"first_diff\":" << (firstDiff == static_cast<size_t>(-1) ? -1 : static_cast<int64_t>(firstDiff))
                << "}";
        }
        oss << "}}";
    } else {
        oss << ",\"rigid\":{\"active\":false}";
    }

    if (context.hasClothSolver) {
        const auto current = context.clothSolver->captureState();
        const auto currentBytes = serializeClothState(current);
        oss << ",\"cloth\":{"
            << "\"active\":true"
            << ",\"current\":{\"node_count\":" << current.nodes.size()
            << ",\"time\":" << current.simulationTime
            << ",\"hash\":\"" << hashHex(hashBytesFnv1a64(currentBytes)) << "\"}"
            << ",\"baseline\":{\"present\":" << (context.hasClothLastState ? "true" : "false");
        if (context.hasClothLastState) {
            const auto baseBytes = serializeClothState(context.clothLastState);
            const auto changed = byteDiffCount(baseBytes, currentBytes);
            const auto firstDiff = firstByteDiff(baseBytes, currentBytes);
            oss << ",\"node_count\":" << context.clothLastState.nodes.size()
                << ",\"time\":" << context.clothLastState.simulationTime
                << ",\"hash\":\"" << hashHex(hashBytesFnv1a64(baseBytes)) << "\""
                << ",\"diff\":{\"equal\":" << (changed == 0 ? "true" : "false")
                << ",\"changed_bytes\":" << changed
                << ",\"first_diff\":" << (firstDiff == static_cast<size_t>(-1) ? -1 : static_cast<int64_t>(firstDiff))
                << "}";
        }
        oss << "}}";
    } else {
        oss << ",\"cloth\":{\"active\":false}";
    }

    if (context.hasFluidSolver) {
        const auto current = context.fluidSolver->captureState();
        const auto currentBytes = serializeFluidState(current);
        oss << ",\"fluid\":{"
            << "\"active\":true"
            << ",\"current\":{\"particle_count\":" << current.particles.size()
            << ",\"time\":" << current.simulationTime
            << ",\"hash\":\"" << hashHex(hashBytesFnv1a64(currentBytes)) << "\"}"
            << ",\"baseline\":{\"present\":" << (context.hasFluidLastState ? "true" : "false");
        if (context.hasFluidLastState) {
            const auto baseBytes = serializeFluidState(context.fluidLastState);
            const auto changed = byteDiffCount(baseBytes, currentBytes);
            const auto firstDiff = firstByteDiff(baseBytes, currentBytes);
            oss << ",\"particle_count\":" << context.fluidLastState.particles.size()
                << ",\"time\":" << context.fluidLastState.simulationTime
                << ",\"hash\":\"" << hashHex(hashBytesFnv1a64(baseBytes)) << "\""
                << ",\"diff\":{\"equal\":" << (changed == 0 ? "true" : "false")
                << ",\"changed_bytes\":" << changed
                << ",\"first_diff\":" << (firstDiff == static_cast<size_t>(-1) ? -1 : static_cast<int64_t>(firstDiff))
                << "}";
        }
        oss << "}}";
    } else {
        oss << ",\"fluid\":{\"active\":false}";
    }

    oss << "}";
    return oss.str();
}

[[nodiscard]] std::string makeReplaySummaryBundleJson(const ScriptContext& context,
                                                      const std::vector<std::string>& commands)
{
    std::ostringstream replay;
    std::vector<uint8_t> replayBytes;
    const auto appendReplayLine = [&replayBytes](std::string_view line) {
        replayBytes.insert(replayBytes.end(), line.begin(), line.end());
        replayBytes.push_back(static_cast<uint8_t>('\n'));
    };

    if (context.hasRigidSolver) {
        const auto current = context.rigidSolver->captureState();
        const auto currentBytes = serializeSimState(current);
        const auto currentHash = hashHex(hashBytesFnv1a64(currentBytes));
        replay << "\"rigid\":{\"active\":true"
               << ",\"current_hash\":\"" << currentHash << "\""
               << ",\"baseline_present\":" << (context.hasRigidLastState ? "true" : "false");
        appendReplayLine("rigid.active=1");
        appendReplayLine("rigid.current_hash=" + currentHash);
        if (context.hasRigidLastState) {
            const auto baseBytes = serializeSimState(context.rigidLastState);
            const auto baseHash = hashHex(hashBytesFnv1a64(baseBytes));
            const auto changed = byteDiffCount(baseBytes, currentBytes);
            const auto firstDiff = firstByteDiff(baseBytes, currentBytes);
            replay << ",\"baseline_hash\":\"" << baseHash << "\""
                   << ",\"equal\":" << (changed == 0 ? "true" : "false")
                   << ",\"changed_bytes\":" << changed
                   << ",\"first_diff\":" << (firstDiff == static_cast<size_t>(-1) ? -1 : static_cast<int64_t>(firstDiff));
            appendReplayLine("rigid.baseline_hash=" + baseHash);
            appendReplayLine("rigid.changed_bytes=" + std::to_string(changed));
            appendReplayLine("rigid.first_diff="
                + std::to_string(firstDiff == static_cast<size_t>(-1) ? -1 : static_cast<int64_t>(firstDiff)));
        }
        replay << "}";
    } else {
        replay << "\"rigid\":{\"active\":false}";
        appendReplayLine("rigid.active=0");
    }

    replay << ",";
    if (context.hasClothSolver) {
        const auto current = context.clothSolver->captureState();
        const auto currentBytes = serializeClothState(current);
        const auto currentHash = hashHex(hashBytesFnv1a64(currentBytes));
        replay << "\"cloth\":{\"active\":true"
               << ",\"current_hash\":\"" << currentHash << "\""
               << ",\"baseline_present\":" << (context.hasClothLastState ? "true" : "false");
        appendReplayLine("cloth.active=1");
        appendReplayLine("cloth.current_hash=" + currentHash);
        if (context.hasClothLastState) {
            const auto baseBytes = serializeClothState(context.clothLastState);
            const auto baseHash = hashHex(hashBytesFnv1a64(baseBytes));
            const auto changed = byteDiffCount(baseBytes, currentBytes);
            const auto firstDiff = firstByteDiff(baseBytes, currentBytes);
            replay << ",\"baseline_hash\":\"" << baseHash << "\""
                   << ",\"equal\":" << (changed == 0 ? "true" : "false")
                   << ",\"changed_bytes\":" << changed
                   << ",\"first_diff\":" << (firstDiff == static_cast<size_t>(-1) ? -1 : static_cast<int64_t>(firstDiff));
            appendReplayLine("cloth.baseline_hash=" + baseHash);
            appendReplayLine("cloth.changed_bytes=" + std::to_string(changed));
            appendReplayLine("cloth.first_diff="
                + std::to_string(firstDiff == static_cast<size_t>(-1) ? -1 : static_cast<int64_t>(firstDiff)));
        }
        replay << "}";
    } else {
        replay << "\"cloth\":{\"active\":false}";
        appendReplayLine("cloth.active=0");
    }

    replay << ",";
    if (context.hasFluidSolver) {
        const auto current = context.fluidSolver->captureState();
        const auto currentBytes = serializeFluidState(current);
        const auto currentHash = hashHex(hashBytesFnv1a64(currentBytes));
        replay << "\"fluid\":{\"active\":true"
               << ",\"current_hash\":\"" << currentHash << "\""
               << ",\"baseline_present\":" << (context.hasFluidLastState ? "true" : "false");
        appendReplayLine("fluid.active=1");
        appendReplayLine("fluid.current_hash=" + currentHash);
        if (context.hasFluidLastState) {
            const auto baseBytes = serializeFluidState(context.fluidLastState);
            const auto baseHash = hashHex(hashBytesFnv1a64(baseBytes));
            const auto changed = byteDiffCount(baseBytes, currentBytes);
            const auto firstDiff = firstByteDiff(baseBytes, currentBytes);
            replay << ",\"baseline_hash\":\"" << baseHash << "\""
                   << ",\"equal\":" << (changed == 0 ? "true" : "false")
                   << ",\"changed_bytes\":" << changed
                   << ",\"first_diff\":" << (firstDiff == static_cast<size_t>(-1) ? -1 : static_cast<int64_t>(firstDiff));
            appendReplayLine("fluid.baseline_hash=" + baseHash);
            appendReplayLine("fluid.changed_bytes=" + std::to_string(changed));
            appendReplayLine("fluid.first_diff="
                + std::to_string(firstDiff == static_cast<size_t>(-1) ? -1 : static_cast<int64_t>(firstDiff)));
        }
        replay << "}";
    } else {
        replay << "\"fluid\":{\"active\":false}";
        appendReplayLine("fluid.active=0");
    }

    const std::string replayHash = hashHex(hashBytesFnv1a64(replayBytes));
    std::ostringstream out;
    out << "{\"command_surface\":" << serializeCommandSurfaceCompactJson(commands)
        << ",\"replay_hash\":\"" << replayHash << "\""
        << ",\"replay\":{" << replay.str() << "}"
        << "}";
    return out.str();
}

[[nodiscard]] size_t byteDiffCount(const std::vector<uint8_t>& a,
                                   const std::vector<uint8_t>& b) noexcept
{
    const size_t minSize = std::min(a.size(), b.size());
    size_t changed = 0;
    for (size_t i = 0; i < minSize; ++i) {
        if (a[i] != b[i]) {
            ++changed;
        }
    }
    changed += (a.size() > b.size()) ? (a.size() - b.size()) : (b.size() - a.size());
    return changed;
}

[[nodiscard]] size_t firstByteDiff(const std::vector<uint8_t>& a,
                                   const std::vector<uint8_t>& b) noexcept
{
    const size_t minSize = std::min(a.size(), b.size());
    for (size_t i = 0; i < minSize; ++i) {
        if (a[i] != b[i]) {
            return i;
        }
    }
    if (a.size() != b.size()) {
        return minSize;
    }
    return static_cast<size_t>(-1);
}

// ── Mesh / Scene / Animation / Cross-solver state serializers ─────────────────

[[nodiscard]] std::vector<uint8_t> serializeMeshState(const nexus::geometry::Mesh& mesh)
{
    std::vector<uint8_t> bytes;
    const auto appendFloat = [&bytes](float f) {
        const uint8_t* b = reinterpret_cast<const uint8_t*>(&f);
        bytes.insert(bytes.end(), b, b + 4);
    };
    const auto appendUint32 = [&bytes](uint32_t v) {
        const uint8_t* b = reinterpret_cast<const uint8_t*>(&v);
        bytes.insert(bytes.end(), b, b + 4);
    };

    const auto& positions = mesh.attributes().positions();
    appendUint32(static_cast<uint32_t>(positions.size()));
    for (const auto& p : positions) {
        appendFloat(p.x); appendFloat(p.y); appendFloat(p.z);
    }

    const auto& normals = mesh.attributes().normals();
    appendUint32(static_cast<uint32_t>(normals.size()));
    for (const auto& n : normals) {
        appendFloat(n.x); appendFloat(n.y); appendFloat(n.z);
    }

    const size_t faceCount = mesh.topology().faceCount();
    appendUint32(static_cast<uint32_t>(faceCount));
    for (size_t i = 0; i < faceCount; ++i) {
        const auto& face = mesh.topology().face(i);
        appendUint32(static_cast<uint32_t>(face.indices.size()));
        for (const uint32_t idx : face.indices) {
            appendUint32(idx);
        }
    }
    return bytes;
}

[[nodiscard]] std::vector<uint8_t> serializeSceneState(const nexus::asset::SceneAsset& scene)
{
    std::vector<uint8_t> bytes;
    struct SceneEntryDigest {
        std::string name;
        std::string sourcePath;
        uint64_t meshHash = 0;
    };
    const auto appendUint32 = [&bytes](uint32_t v) {
        const uint8_t* b = reinterpret_cast<const uint8_t*>(&v);
        bytes.insert(bytes.end(), b, b + 4);
    };
    const auto appendUint64 = [&bytes](uint64_t v) {
        const uint8_t* b = reinterpret_cast<const uint8_t*>(&v);
        bytes.insert(bytes.end(), b, b + 8);
    };
    const auto appendStr = [&bytes](const std::string& s) {
        bytes.insert(bytes.end(), s.begin(), s.end());
        bytes.push_back('\n');
    };

    appendStr(scene.sceneName());
    appendUint32(static_cast<uint32_t>(scene.entryCount()));
    std::vector<SceneEntryDigest> digests;
    digests.reserve(scene.entryCount());
    for (size_t i = 0; i < scene.entryCount(); ++i) {
        const auto& e = scene.entry(i);
        digests.push_back(SceneEntryDigest{
            .name = e.name,
            .sourcePath = e.sourcePath,
            .meshHash = hashBytesFnv1a64(serializeMeshState(e.mesh)),
        });
    }
    std::sort(digests.begin(), digests.end(), [](const SceneEntryDigest& lhs, const SceneEntryDigest& rhs) {
        if (lhs.name != rhs.name) {
            return lhs.name < rhs.name;
        }
        if (lhs.sourcePath != rhs.sourcePath) {
            return lhs.sourcePath < rhs.sourcePath;
        }
        return lhs.meshHash < rhs.meshHash;
    });
    for (const auto& digest : digests) {
        appendStr(digest.name);
        appendStr(digest.sourcePath);
        appendUint64(digest.meshHash);
    }
    return bytes;
}

[[nodiscard]] std::vector<uint8_t> serializeParametricState(
    const nexus::parametric::ConstraintGraph& graph)
{
    const std::string text = nexus::parametric::ParametricGraphSerializer::serialize(graph);
    return std::vector<uint8_t>(text.begin(), text.end());
}

[[nodiscard]] std::vector<uint8_t> serializeAnimationState(
    const nexus::animation::Skeleton& skeleton,
    const nexus::animation::Pose& pose)
{
    std::vector<uint8_t> bytes;
    const auto appendInt32 = [&bytes](int32_t v) {
        const uint8_t* b = reinterpret_cast<const uint8_t*>(&v);
        bytes.insert(bytes.end(), b, b + 4);
    };
    const auto appendUint32 = [&bytes](uint32_t v) {
        const uint8_t* b = reinterpret_cast<const uint8_t*>(&v);
        bytes.insert(bytes.end(), b, b + 4);
    };
    const auto appendStr = [&bytes](const std::string& s) {
        bytes.insert(bytes.end(), s.begin(), s.end());
        bytes.push_back('\n');
    };
    const auto appendTransform = [&bytes](const nexus::animation::Transform& t) {
        const auto af = [&bytes](float f) {
            const uint8_t* b = reinterpret_cast<const uint8_t*>(&f);
            bytes.insert(bytes.end(), b, b + 4);
        };
        af(t.translation.x); af(t.translation.y); af(t.translation.z);
        af(t.rotation.x); af(t.rotation.y); af(t.rotation.z); af(t.rotation.w);
        af(t.scale.x); af(t.scale.y); af(t.scale.z);
    };

    appendUint32(static_cast<uint32_t>(skeleton.boneCount()));
    for (size_t i = 0; i < skeleton.boneCount(); ++i) {
        appendStr(skeleton.boneName(i));
        appendInt32(skeleton.parentIndex(i));
        appendTransform(skeleton.bindLocal(i));
        appendTransform(pose.localTransform(i));
    }
    return bytes;
}

[[nodiscard]] std::vector<uint8_t> serializeCrossSolverState(const ScriptContext& context)
{
    std::vector<uint8_t> bytes;
    const auto appendLine = [&bytes](const std::string& line) {
        bytes.insert(bytes.end(), line.begin(), line.end());
        bytes.push_back('\n');
    };
    if (context.hasRigidSolver) {
        const auto st = context.rigidSolver->captureState();
        appendLine("rigid=" + hashHex(hashBytesFnv1a64(serializeSimState(st))));
    } else {
        appendLine("rigid=inactive");
    }
    if (context.hasClothSolver) {
        const auto st = context.clothSolver->captureState();
        appendLine("cloth=" + hashHex(hashBytesFnv1a64(serializeClothState(st))));
    } else {
        appendLine("cloth=inactive");
    }
    if (context.hasFluidSolver) {
        const auto st = context.fluidSolver->captureState();
        appendLine("fluid=" + hashHex(hashBytesFnv1a64(serializeFluidState(st))));
    } else {
        appendLine("fluid=inactive");
    }
    return bytes;
}

[[nodiscard]] std::string makeCrossSolverBundleJson(const ScriptContext& context)
{
    const auto bytes = serializeCrossSolverState(context);
    const std::string combinedHash = hashHex(hashBytesFnv1a64(bytes));
    std::ostringstream oss;
    oss << "{\"cross_solver_hash\":\"" << combinedHash << "\""
        << ",\"rigid\":" << (context.hasRigidSolver ? "true" : "false")
        << ",\"cloth\":" << (context.hasClothSolver ? "true" : "false")
        << ",\"fluid\":" << (context.hasFluidSolver ? "true" : "false")
        << "}";
    return oss.str();
}

} // namespace

bool ScriptRegistry::registerCommand(std::string name, CommandHandler handler)
{
    if (name.empty() || !handler) {
        return false;
    }
    m_handlers.insert_or_assign(std::move(name), std::move(handler));
    return true;
}

bool ScriptRegistry::hasCommand(std::string_view name) const
{
    return m_handlers.find(std::string(name)) != m_handlers.end();
}

std::vector<std::string> ScriptRegistry::listCommands() const
{
    std::vector<std::string> commands;
    commands.reserve(m_handlers.size());
    for (const auto& [name, _] : m_handlers) {
        commands.push_back(name);
    }
    return commands;
}

bool ScriptRegistry::execute(ScriptContext& context,
                             const ScriptCommand& command,
                             std::vector<std::string>& messages) const
{
    const auto it = m_handlers.find(command.name);
    if (it == m_handlers.end()) {
        messages.push_back("unknown command: " + command.name);
        return false;
    }
    return it->second(context, command, messages);
}

ScriptBatchHarness::ScriptBatchHarness()
{
    registerBuiltinCommands();
}

std::string ScriptBatchHarness::normalizePath(const std::filesystem::path& base,
                                              const std::string& pathText)
{
    const std::filesystem::path path(pathText);
    if (path.is_absolute()) {
        return makePathString(path);
    }
    return makePathString(base / path);
}

ScriptRunReport ScriptBatchHarness::runScript(std::string_view scriptText,
                                              ScriptContext& context) const
{
    ScriptRunReport report{};
    const auto lines = splitScriptLines(scriptText);
    for (size_t lineIndex = 0; lineIndex < lines.size(); ++lineIndex) {
        const std::string& line = lines[lineIndex];
        std::vector<std::string> parseErrors;
        ScriptCommand command = parseCommandLine(line, parseErrors);
        if (!parseErrors.empty()) {
            std::sort(parseErrors.begin(), parseErrors.end());
            report.valid = false;
            report.messages.insert(report.messages.end(), parseErrors.begin(), parseErrors.end());
            report.steps.push_back(ScriptStepReport{lineIndex + 1, {}, false, parseErrors});
            continue;
        }
        if (command.name.empty()) {
            continue;
        }

        std::vector<std::string> messages;
        const bool success = m_registry.execute(context, command, messages);
        std::sort(messages.begin(), messages.end());
        report.steps.push_back(ScriptStepReport{lineIndex + 1, command.name, success, messages});
        if (!success) {
            report.valid = false;
            report.messages.insert(report.messages.end(), messages.begin(), messages.end());
        }
    }
    std::sort(report.messages.begin(), report.messages.end());
    return report;
}

ScriptRunReport ScriptBatchHarness::runScriptFile(const std::filesystem::path& scriptPath,
                                                 ScriptContext& context) const
{
    std::ifstream in(scriptPath);
    if (!in) {
        ScriptRunReport report{};
        report.valid = false;
        report.messages.push_back("failed to open script file: " + makePathString(scriptPath));
        std::sort(report.messages.begin(), report.messages.end());
        return report;
    }

    std::ostringstream buffer;
    buffer << in.rdbuf();
    context.workingDirectory = scriptPath.parent_path();
    return runScript(buffer.str(), context);
}

ScriptCommand ScriptBatchHarness::parseCommandLine(std::string_view line,
                                                   std::vector<std::string>& errors)
{
    ScriptCommand command{};
    const auto tokens = tokenizeLine(line);
    if (tokens.empty()) {
        return command;
    }

    command.name = tokens.front();
    for (size_t i = 1; i < tokens.size(); ++i) {
        std::string key;
        std::string value;
        if (!parseKeyValueToken(tokens[i], key, value)) {
            errors.push_back("invalid argument token: " + tokens[i]);
            continue;
        }
        command.args.emplace(std::move(key), std::move(value));
    }
    return command;
}

std::vector<std::string> ScriptBatchHarness::splitScriptLines(std::string_view scriptText)
{
    std::vector<std::string> lines;
    std::string current;
    for (char c : scriptText) {
        if (c == '\n') {
            lines.push_back(trimCopy(current));
            current.clear();
        } else if (c != '\r') {
            current.push_back(c);
        }
    }
    if (!current.empty() || (!scriptText.empty() && scriptText.back() == '\n')) {
        lines.push_back(trimCopy(current));
    }
    return lines;
}

std::optional<std::string> ScriptBatchHarness::stripQuotes(std::string_view text)
{
    if (text.size() >= 2 && text.front() == '"' && text.back() == '"') {
        return std::string(text.substr(1, text.size() - 2));
    }
    return std::nullopt;
}

std::optional<float> ScriptBatchHarness::parseFloat(std::string_view text)
{
    char* end = nullptr;
    const std::string tmp(text);
    const float value = std::strtof(tmp.c_str(), &end);
    if (end == tmp.c_str() || *end != '\0') {
        return std::nullopt;
    }
    return value;
}

std::optional<int32_t> ScriptBatchHarness::parseInt(std::string_view text)
{
    int32_t value = 0;
    const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
    if (result.ec != std::errc{} || result.ptr != text.data() + text.size()) {
        return std::nullopt;
    }
    return value;
}

std::optional<bool> ScriptBatchHarness::parseBool(std::string_view text)
{
    if (text == "1" || text == "true" || text == "True" || text == "yes") {
        return true;
    }
    if (text == "0" || text == "false" || text == "False" || text == "no") {
        return false;
    }
    return std::nullopt;
}

void ScriptBatchHarness::registerBuiltinCommands()
{
    using namespace nexus::geometry;
    using namespace nexus::asset;
    using namespace nexus::animation;
    using namespace nexus::gfx;

    m_registry.registerCommand("script.list_commands",
        [this](ScriptContext&, const ScriptCommand&, std::vector<std::string>& messages) {
            const auto commands = m_registry.listCommands();
            messages.push_back("command_count=" + std::to_string(commands.size()));
            for (const auto& name : commands) {
                messages.push_back("command=" + name);
            }
            return true;
        });

    m_registry.registerCommand("script.commands_hash",
        [this](ScriptContext&, const ScriptCommand&, std::vector<std::string>& messages) {
            const auto commands = m_registry.listCommands();
            std::vector<uint8_t> bytes;
            for (const auto& name : commands) {
                bytes.insert(bytes.end(), name.begin(), name.end());
                bytes.push_back(static_cast<uint8_t>('\n'));
            }
            messages.push_back("commands hash=" + hashHex(hashBytesFnv1a64(bytes))
                + " count=" + std::to_string(commands.size()));
            return true;
        });

    m_registry.registerCommand("script.assert_command",
        [this](ScriptContext&, const ScriptCommand& command, std::vector<std::string>& messages) {
            const auto nameArg = getArg(command, "name");
            if (!nameArg || nameArg->empty()) {
                messages.push_back("script.assert_command requires name=");
                return false;
            }
            if (!m_registry.hasCommand(*nameArg)) {
                messages.push_back("script.assert_command missing: " + *nameArg);
                return false;
            }
            messages.push_back("script.assert_command present: " + *nameArg);
            return true;
        });

    m_registry.registerCommand("script.export_report",
        [this](ScriptContext& context, const ScriptCommand& command, std::vector<std::string>& messages) {
            const auto pathArg = getArg(command, "path");
            if (!pathArg) {
                messages.push_back("script.export_report requires path=");
                return false;
            }
            const std::filesystem::path target = normalizePath(context.workingDirectory, *pathArg);
            const std::string report = makeDiagnosticsReportJson(context, m_registry.listCommands());
            std::vector<uint8_t> bytes(report.begin(), report.end());
            if (!writeBytesToFile(target, bytes, messages)) {
                return false;
            }
            messages.push_back("report exported bytes=" + std::to_string(bytes.size()));
            return true;
        });

    m_registry.registerCommand("script.export_replay_bundle",
        [this](ScriptContext& context, const ScriptCommand& command, std::vector<std::string>& messages) {
            const auto pathArg = getArg(command, "path");
            if (!pathArg) {
                messages.push_back("script.export_replay_bundle requires path=");
                return false;
            }
            const std::filesystem::path target = normalizePath(context.workingDirectory, *pathArg);
            const std::string report = makeReplaySummaryBundleJson(context, m_registry.listCommands());
            std::vector<uint8_t> bytes(report.begin(), report.end());
            if (!writeBytesToFile(target, bytes, messages)) {
                return false;
            }
            messages.push_back("replay bundle exported bytes=" + std::to_string(bytes.size()));
            return true;
        });

    m_registry.registerCommand("script.verify_replay_bundle",
        [this](ScriptContext& context, const ScriptCommand& command, std::vector<std::string>& messages) {
            const auto pathArg = getArg(command, "path");
            if (!pathArg) {
                messages.push_back("script.verify_replay_bundle requires path=");
                return false;
            }

            const std::filesystem::path source = normalizePath(context.workingDirectory, *pathArg);
            std::vector<uint8_t> input;
            if (!readBytesFromFile(source, input, messages)) {
                return false;
            }

            const std::string expectedText(input.begin(), input.end());
            const auto expectedHash = extractJsonStringField(expectedText, "replay_hash");
            if (!expectedHash) {
                messages.push_back("script.verify_replay_bundle missing replay_hash in: " + makePathString(source));
                return false;
            }

            const std::string current = makeReplaySummaryBundleJson(context, m_registry.listCommands());
            const auto actualHash = extractJsonStringField(current, "replay_hash");
            if (!actualHash) {
                messages.push_back("script.verify_replay_bundle failed to compute replay_hash");
                return false;
            }

            if (*expectedHash != *actualHash) {
                messages.push_back("script.verify_replay_bundle mismatch expected=" + *expectedHash
                    + " actual=" + *actualHash);
                return false;
            }

            messages.push_back("script.verify_replay_bundle match: " + *actualHash);
            return true;
        });

    m_registry.registerCommand("mesh.make_triangle",
        [](ScriptContext& context, const ScriptCommand& command, std::vector<std::string>& messages) {
            const auto size = parseFloatArg(command, "size").value_or(1.f);
            context.mesh = nexus::geometry::primitives::makeTriangle(size);
            context.hasMesh = true;
            context.meshName = command.args.contains("name") ? command.args.at("name") : "triangle";
            messages.push_back("mesh created: " + context.meshName);
            return true;
        });

    m_registry.registerCommand("mesh.make_sphere",
        [](ScriptContext& context, const ScriptCommand& command, std::vector<std::string>& messages) {
            const float radius  = parseFloatArg(command, "radius").value_or(1.f);
            const auto latSegs  = static_cast<uint32_t>(std::max(2, parseIntArg(command, "lat_segs").value_or(16)));
            const auto lonSegs  = static_cast<uint32_t>(std::max(3, parseIntArg(command, "lon_segs").value_or(16)));
            context.mesh = nexus::geometry::primitives::makeSphere(radius, latSegs, lonSegs);
            context.hasMesh = true;
            context.meshName = command.args.contains("name") ? command.args.at("name") : "sphere";
            messages.push_back("mesh created: " + context.meshName
                + " vertices=" + std::to_string(context.mesh.attributes().vertexCount())
                + " faces=" + std::to_string(context.mesh.topology().faceCount()));
            return true;
        });

    m_registry.registerCommand("mesh.make_cylinder",
        [](ScriptContext& context, const ScriptCommand& command, std::vector<std::string>& messages) {
            const float radius    = parseFloatArg(command, "radius").value_or(1.f);
            const float height    = parseFloatArg(command, "height").value_or(2.f);
            const auto radialSegs = static_cast<uint32_t>(std::max(3, parseIntArg(command, "segs").value_or(16)));
            context.mesh = nexus::geometry::primitives::makeCylinder(radius, height, radialSegs);
            context.hasMesh = true;
            context.meshName = command.args.contains("name") ? command.args.at("name") : "cylinder";
            messages.push_back("mesh created: " + context.meshName
                + " vertices=" + std::to_string(context.mesh.attributes().vertexCount())
                + " faces=" + std::to_string(context.mesh.topology().faceCount()));
            return true;
        });

    m_registry.registerCommand("mesh.make_cone",
        [](ScriptContext& context, const ScriptCommand& command, std::vector<std::string>& messages) {
            const float radius    = parseFloatArg(command, "radius").value_or(1.f);
            const float height    = parseFloatArg(command, "height").value_or(2.f);
            const auto radialSegs = static_cast<uint32_t>(std::max(3, parseIntArg(command, "segs").value_or(16)));
            context.mesh = nexus::geometry::primitives::makeCone(radius, height, radialSegs);
            context.hasMesh = true;
            context.meshName = command.args.contains("name") ? command.args.at("name") : "cone";
            messages.push_back("mesh created: " + context.meshName
                + " vertices=" + std::to_string(context.mesh.attributes().vertexCount())
                + " faces=" + std::to_string(context.mesh.topology().faceCount()));
            return true;
        });

    m_registry.registerCommand("mesh.make_capsule",
        [](ScriptContext& context, const ScriptCommand& command, std::vector<std::string>& messages) {
            const float radius         = parseFloatArg(command, "radius").value_or(0.5f);
            const float cylinderHeight = parseFloatArg(command, "height").value_or(1.f);
            const auto radialSegs      = static_cast<uint32_t>(std::max(3, parseIntArg(command, "segs").value_or(16)));
            const auto ringSegs        = static_cast<uint32_t>(std::max(2, parseIntArg(command, "ring_segs").value_or(8)));
            context.mesh = nexus::geometry::primitives::makeCapsule(radius, cylinderHeight, radialSegs, ringSegs);
            context.hasMesh = true;
            context.meshName = command.args.contains("name") ? command.args.at("name") : "capsule";
            messages.push_back("mesh created: " + context.meshName
                + " vertices=" + std::to_string(context.mesh.attributes().vertexCount())
                + " faces=" + std::to_string(context.mesh.topology().faceCount()));
            return true;
        });

    m_registry.registerCommand("mesh.compute_normals",
        [](ScriptContext& context, const ScriptCommand&, std::vector<std::string>& messages) {
            if (!context.hasMesh) {
                messages.push_back("mesh.compute_normals requires mesh.make_triangle or mesh.load first");
                return false;
            }
            if (!context.mesh.computeVertexNormals()) {
                messages.push_back("mesh.compute_normals failed");
                return false;
            }
            messages.push_back("mesh normals computed");
            return true;
        });

    m_registry.registerCommand("mesh.transform",
        [](ScriptContext& context, const ScriptCommand& command, std::vector<std::string>& messages) {
            if (!context.hasMesh) {
                messages.push_back("mesh.transform requires a mesh");
                return false;
            }

            const float tx = parseFloatArg(command, "tx").value_or(0.f);
            const float ty = parseFloatArg(command, "ty").value_or(0.f);
            const float tz = parseFloatArg(command, "tz").value_or(0.f);
            const float sx = parseFloatArg(command, "sx").value_or(1.f);
            const float sy = parseFloatArg(command, "sy").value_or(1.f);
            const float sz = parseFloatArg(command, "sz").value_or(1.f);

            nexus::render::Mat4 transform = makeTranslationMatrix(tx, ty, tz);
            const nexus::render::Mat4 scale = makeScaleMatrix(sx, sy, sz);
            transform = transform * scale;

            const GeometryCommandDesc desc{GeometryCommandType::Transform, transform};
            const GeometryCommandReport report = GeometryCommandSurface::execute(context.mesh, desc);
            if (!report.valid) {
                messages.insert(messages.end(), report.errors.begin(), report.errors.end());
                return false;
            }
            messages.push_back("mesh transformed");
            return true;
        });

    m_registry.registerCommand("mesh.export",
        [](ScriptContext& context, const ScriptCommand& command, std::vector<std::string>& messages) {
            if (!context.hasMesh) {
                messages.push_back("mesh.export requires a mesh");
                return false;
            }
            const auto pathArg = getArg(command, "path");
            if (!pathArg) {
                messages.push_back("mesh.export requires path=");
                return false;
            }
            const std::string formatText = command.args.contains("format") ? command.args.at("format") : "obj";
            MeshExportOptions options{};
            if (formatText == "ply") {
                options.format = MeshExportFormat::PLY;
            } else {
                options.format = MeshExportFormat::OBJ;
            }

            const std::filesystem::path target = normalizePath(context.workingDirectory, *pathArg);
            const MeshExportReport report = MeshIO::exportMesh(context.mesh, target.string(), options);
            if (!report.valid) {
                messages.insert(messages.end(), report.messages.begin(), report.messages.end());
                return false;
            }
            messages.push_back("mesh exported: " + target.string());
            return true;
        });

    m_registry.registerCommand("scene.new",
        [](ScriptContext& context, const ScriptCommand& command, std::vector<std::string>& messages) {
            context.scene = SceneAsset{};
            context.hasScene = true;
            if (command.args.contains("name")) {
                context.scene.setSceneName(command.args.at("name"));
            }
            context.sceneName = context.scene.sceneName();
            messages.push_back("scene created: " + context.sceneName);
            return true;
        });

    m_registry.registerCommand("scene.add_mesh_entry",
        [](ScriptContext& context, const ScriptCommand& command, std::vector<std::string>& messages) {
            if (!context.hasMesh) {
                messages.push_back("scene.add_mesh_entry requires a current mesh");
                return false;
            }
            if (!context.hasScene) {
                context.scene = SceneAsset{};
                context.hasScene = true;
            }

            SceneMeshEntry entry;
            entry.name = command.args.contains("name") ? command.args.at("name") : context.meshName;
            if (entry.name.empty()) {
                entry.name = "mesh_entry";
            }
            entry.sourcePath = command.args.contains("source") ? command.args.at("source") : std::string{};
            entry.mesh = context.mesh;
            context.scene.addEntry(std::move(entry));
            messages.push_back("scene entry added: " + context.scene.entry(context.scene.entryCount() - 1).name);
            return true;
        });

    m_registry.registerCommand("scene.rename_entry",
        [](ScriptContext& context, const ScriptCommand& command, std::vector<std::string>& messages) {
            if (!context.hasScene) {
                messages.push_back("scene.rename_entry requires a scene");
                return false;
            }
            const auto toArg = getArg(command, "to");
            if (!toArg || toArg->empty()) {
                messages.push_back("scene.rename_entry requires to=");
                return false;
            }

            const bool hasIndexArg = command.args.contains("index");
            const auto fromArg    = getArg(command, "from");

            SceneMeshEntry* entry = nullptr;
            std::string     oldName;

            if (hasIndexArg) {
                const auto indexArg = parseIntArg(command, "index");
                if (!indexArg || *indexArg < 0) {
                    messages.push_back("scene.rename_entry invalid index");
                    return false;
                }
                const size_t idx = static_cast<size_t>(*indexArg);
                if (idx >= context.scene.entryCount()) {
                    messages.push_back("scene.rename_entry index out of range");
                    return false;
                }
                entry   = &context.scene.entry(idx);
                oldName = entry->name;
            } else {
                if (!fromArg || fromArg->empty()) {
                    messages.push_back("scene.rename_entry requires from= or index=");
                    return false;
                }
                entry = context.scene.findByName(*fromArg);
                if (!entry) {
                    messages.push_back("scene.rename_entry missing entry: " + *fromArg);
                    return false;
                }
                oldName = *fromArg;
            }

            if (oldName != *toArg && context.scene.findByName(*toArg)) {
                messages.push_back("scene.rename_entry target already exists: " + *toArg);
                return false;
            }

            entry->name = *toArg;
            messages.push_back("scene entry renamed: " + oldName + " -> " + *toArg);
            return true;
        });

    m_registry.registerCommand("scene.remove_entry",
        [](ScriptContext& context, const ScriptCommand& command, std::vector<std::string>& messages) {
            if (!context.hasScene) {
                messages.push_back("scene.remove_entry requires a scene");
                return false;
            }
            const bool hasIndexArg = command.args.contains("index");
            const auto nameArg = getArg(command, "name");
            if (!hasIndexArg && (!nameArg || nameArg->empty())) {
                messages.push_back("scene.remove_entry requires name= or index=");
                return false;
            }

            if (hasIndexArg) {
                const auto indexArg = parseIntArg(command, "index");
                if (!indexArg || *indexArg < 0) {
                    messages.push_back("scene.remove_entry invalid index");
                    return false;
                }
                const size_t removeIndex = static_cast<size_t>(*indexArg);
                if (removeIndex >= context.scene.entryCount()) {
                    messages.push_back("scene.remove_entry index out of range");
                    return false;
                }

                const std::string removedName = context.scene.entry(removeIndex).name;
                context.scene.removeEntry(removeIndex);
                messages.push_back("scene entry removed: " + removedName + " index=" + std::to_string(removeIndex));
                return true;
            }

            size_t removeIndex = context.scene.entryCount();
            for (size_t i = 0; i < context.scene.entryCount(); ++i) {
                if (context.scene.entry(i).name == *nameArg) {
                    removeIndex = i;
                    break;
                }
            }
            if (removeIndex == context.scene.entryCount()) {
                messages.push_back("scene.remove_entry missing entry: " + *nameArg);
                return false;
            }

            context.scene.removeEntry(removeIndex);
            messages.push_back("scene entry removed: " + *nameArg);
            return true;
        });

    m_registry.registerCommand("scene.load",
        [](ScriptContext& context, const ScriptCommand& command, std::vector<std::string>& messages) {
            const auto pathArg = getArg(command, "path");
            if (!pathArg) {
                messages.push_back("scene.load requires path=");
                return false;
            }
            const std::filesystem::path target = normalizePath(context.workingDirectory, *pathArg);
            if (!context.hasScene) {
                context.scene = SceneAsset{};
                context.hasScene = true;
            }
            const SceneAssetIOReport report = context.scene.load(target.string());
            if (!report.valid) {
                messages.insert(messages.end(), report.messages.begin(), report.messages.end());
                return false;
            }
            context.sceneName = context.scene.sceneName();
            messages.push_back("scene loaded: " + target.string());
            return true;
        });

    m_registry.registerCommand("scene.save",
        [](ScriptContext& context, const ScriptCommand& command, std::vector<std::string>& messages) {
            if (!context.hasScene) {
                messages.push_back("scene.save requires a scene");
                return false;
            }
            const auto pathArg = getArg(command, "path");
            if (!pathArg) {
                messages.push_back("scene.save requires path=");
                return false;
            }
            const std::filesystem::path target = normalizePath(context.workingDirectory, *pathArg);
            const SceneAssetIOReport report = context.scene.save(target.string());
            if (!report.valid) {
                messages.insert(messages.end(), report.messages.begin(), report.messages.end());
                return false;
            }
            messages.push_back("scene saved: " + target.string());
            return true;
        });

    m_registry.registerCommand("scene.export_text",
        [](ScriptContext& context, const ScriptCommand& command, std::vector<std::string>& messages) {
            if (!context.hasScene) {
                messages.push_back("scene.export_text requires a scene");
                return false;
            }
            const auto pathArg = getArg(command, "path");
            if (!pathArg) {
                messages.push_back("scene.export_text requires path=");
                return false;
            }
            const std::filesystem::path target = normalizePath(context.workingDirectory, *pathArg);
            const SceneAssetIOReport report = context.scene.exportText(target.string());
            if (!report.valid) {
                messages.insert(messages.end(), report.messages.begin(), report.messages.end());
                return false;
            }
            messages.push_back("scene text exported: " + target.string());
            return true;
        });

    m_registry.registerCommand("render.create_null_context",
        [](ScriptContext& context, const ScriptCommand& command, std::vector<std::string>& messages) {
            RenderContextDesc desc{};
            desc.preferredBackend = Backend::Null;
            desc.validation = ValidationLevel::Off;
            auto renderContext = RenderContext::create(desc);
            if (!renderContext) {
                messages.push_back("render context creation failed");
                return false;
            }
            context.renderContext = std::move(renderContext);
            const auto width = parseIntArg(command, "width").value_or(1280);
            const auto height = parseIntArg(command, "height").value_or(720);
            messages.push_back("render context created: null " + std::to_string(width) + "x" + std::to_string(height));
            return true;
        });

    m_registry.registerCommand("render.describe",
        [](ScriptContext& context, const ScriptCommand&, std::vector<std::string>& messages) {
            if (!context.renderContext) {
                messages.push_back("render.describe requires render.create_null_context first");
                return false;
            }
            messages.push_back("backend=" + std::string(context.renderContext->activeBackend() == Backend::Null ? "null" : "other"));
            messages.push_back("tier=" + std::to_string(static_cast<int>(context.renderContext->hardwareTier())));
            return true;
        });

    m_registry.registerCommand("render.plan_frame",
        [](ScriptContext& context, const ScriptCommand& command, std::vector<std::string>& messages) {
            (void)command;
            if (!context.renderContext) {
                messages.push_back("render.plan_frame requires render.create_null_context first");
                return false;
            }
            auto& device = context.renderContext->device();
            const CmdBufHandle cmd = device.allocateCommandBuffer(QueueType::Graphics);
            if (!cmd.valid()) {
                messages.push_back("render.plan_frame failed to allocate command buffer");
                return false;
            }
            RenderPassGraphPlanner planner;
            const auto pass = planner.addPass(RenderPassGraphPlanner::PassDesc{QueueType::Graphics, std::span<const CmdBufHandle>(&cmd, 1)});
            const auto plan = planner.buildSubmissionPlan({});
            messages.push_back("render plan pass_count=" + std::to_string(pass + 1));
            messages.push_back("render plan submit_count=" + std::to_string(plan.submits.size()));
            device.freeCommandBuffer(cmd);
            return true;
        });

    m_registry.registerCommand("animation.add_bone",
        [](ScriptContext& context, const ScriptCommand& command, std::vector<std::string>& messages) {
            const auto nameArg = getArg(command, "name");
            if (!nameArg || nameArg->empty()) {
                messages.push_back("animation.add_bone requires name=");
                return false;
            }
            const int32_t parent = parseIntArg(command, "parent").value_or(-1);
            BoneDesc bone{};
            bone.name = *nameArg;
            bone.parentIndex = parent;
            bone.bindLocal.translation.x = parseFloatArg(command, "tx").value_or(0.f);
            bone.bindLocal.translation.y = parseFloatArg(command, "ty").value_or(0.f);
            bone.bindLocal.translation.z = parseFloatArg(command, "tz").value_or(0.f);
            bone.bindLocal.scale.x = parseFloatArg(command, "sx").value_or(1.f);
            bone.bindLocal.scale.y = parseFloatArg(command, "sy").value_or(1.f);
            bone.bindLocal.scale.z = parseFloatArg(command, "sz").value_or(1.f);

            const int32_t index = context.skeleton.addBone(std::move(bone));
            if (index < 0) {
                messages.push_back("animation.add_bone failed");
                return false;
            }
            context.hasSkeleton = true;
            context.pose.resize(context.skeleton.boneCount());
            messages.push_back("bone added: " + *nameArg);
            return true;
        });

    m_registry.registerCommand("animation.reset_pose",
        [](ScriptContext& context, const ScriptCommand&, std::vector<std::string>& messages) {
            if (!context.hasSkeleton) {
                messages.push_back("animation.reset_pose requires a skeleton");
                return false;
            }
            context.pose.resize(context.skeleton.boneCount());
            for (size_t i = 0; i < context.skeleton.boneCount(); ++i) {
                context.pose.setLocalTransform(i, context.skeleton.bindLocal(i));
            }
            messages.push_back("pose reset to bind local");
            return true;
        });

    m_registry.registerCommand("animation.sample_bind_pose",
        [](ScriptContext& context, const ScriptCommand&, std::vector<std::string>& messages) {
            if (!context.hasSkeleton) {
                messages.push_back("animation.sample_bind_pose requires a skeleton");
                return false;
            }
            context.pose.resize(context.skeleton.boneCount());
            for (size_t i = 0; i < context.skeleton.boneCount(); ++i) {
                context.pose.setLocalTransform(i, context.skeleton.bindLocal(i));
            }
            context.pose.computeModelMatrices(context.skeleton);
            messages.push_back("bind pose sampled");
            return true;
        });

    // ── Rigid-body simulation commands ────────────────────────────────────────

    m_registry.registerCommand("sim.rigid.create",
        [](ScriptContext& context, const ScriptCommand& command, std::vector<std::string>& messages) {
            context.rigidSolver  = std::make_unique<nexus::RigidBodySolver>();
            context.hasRigidSolver = true;
            context.hasRigidLastState = false;
            const float gx = parseFloatArg(command, "gx").value_or(0.f);
            const float gy = parseFloatArg(command, "gy").value_or(-9.81f);
            const float gz = parseFloatArg(command, "gz").value_or(0.f);
            context.rigidSolver->setGravity({gx, gy, gz});
            messages.push_back("rigid solver created");
            return true;
        });

    m_registry.registerCommand("sim.rigid.apply_force",
        [](ScriptContext& context, const ScriptCommand& command, std::vector<std::string>& messages) {
            if (!context.hasRigidSolver) {
                messages.push_back("sim.rigid.apply_force requires sim.rigid.create first");
                return false;
            }
            const auto id = parseIntArg(command, "id");
            if (!id || *id <= 0) {
                messages.push_back("sim.rigid.apply_force requires id=>0");
                return false;
            }
            const nexus::SimVec3 force{
                parseFloatArg(command, "fx").value_or(0.f),
                parseFloatArg(command, "fy").value_or(0.f),
                parseFloatArg(command, "fz").value_or(0.f),
            };
            if (!context.rigidSolver->applyForce(static_cast<nexus::BodyId>(*id), force)) {
                messages.push_back("sim.rigid.apply_force failed");
                return false;
            }
            messages.push_back("rigid force applied id=" + std::to_string(*id));
            return true;
        });

    m_registry.registerCommand("sim.rigid.add_body",
        [](ScriptContext& context, const ScriptCommand& command, std::vector<std::string>& messages) {
            if (!context.hasRigidSolver) {
                messages.push_back("sim.rigid.add_body requires sim.rigid.create first");
                return false;
            }
            nexus::SimBodyDesc desc{};
            desc.mass        = parseFloatArg(command, "mass").value_or(1.f);
            desc.position.x  = parseFloatArg(command, "tx").value_or(0.f);
            desc.position.y  = parseFloatArg(command, "ty").value_or(0.f);
            desc.position.z  = parseFloatArg(command, "tz").value_or(0.f);
            desc.velocity.x  = parseFloatArg(command, "vx").value_or(0.f);
            desc.velocity.y  = parseFloatArg(command, "vy").value_or(0.f);
            desc.velocity.z  = parseFloatArg(command, "vz").value_or(0.f);
            const nexus::BodyId id = context.rigidSolver->addBody(desc);
            messages.push_back("rigid body added id=" + std::to_string(id)
                + " mass=" + std::to_string(desc.mass));
            return true;
        });

    m_registry.registerCommand("sim.rigid.step",
        [](ScriptContext& context, const ScriptCommand& command, std::vector<std::string>& messages) {
            if (!context.hasRigidSolver) {
                messages.push_back("sim.rigid.step requires sim.rigid.create first");
                return false;
            }
            const double dt = static_cast<double>(parseFloatArg(command, "dt").value_or(0.016f));
            const nexus::StepReport report = context.rigidSolver->step(dt);
            if (!report.ok) {
                messages.push_back("sim.rigid.step failed (invalid dt)");
                return false;
            }
            messages.push_back("rigid step t=" + std::to_string(report.simulationTime)
                + " bodies=" + std::to_string(report.bodiesIntegrated));
            return true;
        });

    m_registry.registerCommand("sim.rigid.capture_state",
        [](ScriptContext& context, const ScriptCommand&, std::vector<std::string>& messages) {
            if (!context.hasRigidSolver) {
                messages.push_back("sim.rigid.capture_state requires sim.rigid.create first");
                return false;
            }
            context.rigidLastState = context.rigidSolver->captureState();
            context.hasRigidLastState = true;
            messages.push_back("rigid state captured bodies="
                + std::to_string(context.rigidLastState.bodies.size()));
            return true;
        });

    m_registry.registerCommand("sim.rigid.set_baseline",
        [](ScriptContext& context, const ScriptCommand&, std::vector<std::string>& messages) {
            if (!context.hasRigidSolver) {
                messages.push_back("sim.rigid.set_baseline requires sim.rigid.create first");
                return false;
            }
            context.rigidLastState = context.rigidSolver->captureState();
            context.hasRigidLastState = true;
            messages.push_back("rigid baseline set bodies="
                + std::to_string(context.rigidLastState.bodies.size()));
            return true;
        });

    m_registry.registerCommand("sim.rigid.state_hash",
        [](ScriptContext& context, const ScriptCommand&, std::vector<std::string>& messages) {
            if (!context.hasRigidSolver) {
                messages.push_back("sim.rigid.state_hash requires sim.rigid.create first");
                return false;
            }
            const nexus::SimState current = context.rigidSolver->captureState();
            const std::vector<uint8_t> bytes = serializeSimState(current);
            messages.push_back("rigid hash=" + hashHex(hashBytesFnv1a64(bytes))
                + " bytes=" + std::to_string(bytes.size()));
            return true;
        });

    m_registry.registerCommand("sim.rigid.state_summary",
        [](ScriptContext& context, const ScriptCommand&, std::vector<std::string>& messages) {
            if (!context.hasRigidSolver) {
                messages.push_back("sim.rigid.state_summary requires sim.rigid.create first");
                return false;
            }
            const nexus::SimState current = context.rigidSolver->captureState();
            const std::vector<uint8_t> bytes = serializeSimState(current);
            messages.push_back("rigid summary bodies=" + std::to_string(current.bodies.size())
                + " time=" + std::to_string(current.simulationTime)
                + " bytes=" + std::to_string(bytes.size())
                + " hash=" + hashHex(hashBytesFnv1a64(bytes)));
            return true;
        });

    m_registry.registerCommand("sim.rigid.expect_hash",
        [](ScriptContext& context, const ScriptCommand& command, std::vector<std::string>& messages) {
            if (!context.hasRigidSolver) {
                messages.push_back("sim.rigid.expect_hash requires sim.rigid.create first");
                return false;
            }
            const auto expected = parseExpectedHashArg(command);
            if (!expected) {
                messages.push_back("sim.rigid.expect_hash requires hash=<uint64|0xHEX>");
                return false;
            }
            const nexus::SimState current = context.rigidSolver->captureState();
            const std::vector<uint8_t> bytes = serializeSimState(current);
            const uint64_t actual = hashBytesFnv1a64(bytes);
            if (actual != *expected) {
                messages.push_back("sim.rigid.expect_hash mismatch expected="
                    + hashHex(*expected) + " actual=" + hashHex(actual));
                return false;
            }
            messages.push_back("sim.rigid.expect_hash match " + hashHex(actual));
            return true;
        });

    m_registry.registerCommand("sim.rigid.diff_state",
        [](ScriptContext& context, const ScriptCommand&, std::vector<std::string>& messages) {
            if (!context.hasRigidSolver) {
                messages.push_back("sim.rigid.diff_state requires sim.rigid.create first");
                return false;
            }
            if (!context.hasRigidLastState) {
                messages.push_back("sim.rigid.diff_state requires sim.rigid.capture_state, sim.rigid.export_state, or sim.rigid.import_state first");
                return false;
            }
            const nexus::SimState current = context.rigidSolver->captureState();
            const std::vector<uint8_t> currBytes = serializeSimState(current);
            const std::vector<uint8_t> baseBytes = serializeSimState(context.rigidLastState);
            const size_t changed = byteDiffCount(baseBytes, currBytes);
            const size_t firstDiff = firstByteDiff(baseBytes, currBytes);
            messages.push_back("rigid diff equal=" + std::string(changed == 0 ? "1" : "0")
                + " changed=" + std::to_string(changed)
                + " first_diff=" + std::string(firstDiff == static_cast<size_t>(-1) ? "-1" : std::to_string(firstDiff))
                + " base_hash=" + hashHex(hashBytesFnv1a64(baseBytes))
                + " curr_hash=" + hashHex(hashBytesFnv1a64(currBytes)));
            return true;
        });

    m_registry.registerCommand("sim.rigid.restore_state",
        [](ScriptContext& context, const ScriptCommand&, std::vector<std::string>& messages) {
            if (!context.hasRigidSolver) {
                messages.push_back("sim.rigid.restore_state requires sim.rigid.create first");
                return false;
            }
            if (!context.rigidSolver->restoreState(context.rigidLastState)) {
                messages.push_back("sim.rigid.restore_state failed");
                return false;
            }
            messages.push_back("rigid state restored");
            return true;
        });

    m_registry.registerCommand("sim.rigid.export_state",
        [](ScriptContext& context, const ScriptCommand& command, std::vector<std::string>& messages) {
            if (!context.hasRigidSolver) {
                messages.push_back("sim.rigid.export_state requires sim.rigid.create first");
                return false;
            }
            const auto pathArg = getArg(command, "path");
            if (!pathArg) {
                messages.push_back("sim.rigid.export_state requires path=");
                return false;
            }
            context.rigidLastState = context.rigidSolver->captureState();
            context.hasRigidLastState = true;
            const std::vector<uint8_t> bytes = serializeSimState(context.rigidLastState);
            const std::filesystem::path target = normalizePath(context.workingDirectory, *pathArg);
            if (!writeBytesToFile(target, bytes, messages)) {
                return false;
            }
            messages.push_back("rigid state exported bytes=" + std::to_string(bytes.size()));
            return true;
        });

    m_registry.registerCommand("sim.rigid.import_state",
        [](ScriptContext& context, const ScriptCommand& command, std::vector<std::string>& messages) {
            if (!context.hasRigidSolver) {
                messages.push_back("sim.rigid.import_state requires sim.rigid.create first");
                return false;
            }
            const auto pathArg = getArg(command, "path");
            if (!pathArg) {
                messages.push_back("sim.rigid.import_state requires path=");
                return false;
            }
            std::vector<uint8_t> bytes;
            const std::filesystem::path target = normalizePath(context.workingDirectory, *pathArg);
            if (!readBytesFromFile(target, bytes, messages)) {
                return false;
            }
            nexus::SimState state;
            if (!deserializeSimState(bytes, state)) {
                messages.push_back("sim.rigid.import_state deserialize failed");
                return false;
            }
            if (!context.rigidSolver->restoreState(state)) {
                messages.push_back("sim.rigid.import_state restore failed");
                return false;
            }
            context.rigidLastState = std::move(state);
            context.hasRigidLastState = true;
            messages.push_back("rigid state imported bodies="
                + std::to_string(context.rigidLastState.bodies.size()));
            return true;
        });

    // ── Cloth simulation commands ─────────────────────────────────────────────

    m_registry.registerCommand("sim.cloth.create",
        [](ScriptContext& context, const ScriptCommand& command, std::vector<std::string>& messages) {
            context.clothSolver  = std::make_unique<nexus::ClothSolver>();
            context.hasClothSolver = true;
            context.hasClothLastState = false;
            const float gx = parseFloatArg(command, "gx").value_or(0.f);
            const float gy = parseFloatArg(command, "gy").value_or(-9.81f);
            const float gz = parseFloatArg(command, "gz").value_or(0.f);
            context.clothSolver->setGravity({gx, gy, gz});
            messages.push_back("cloth solver created");
            return true;
        });

    m_registry.registerCommand("sim.cloth.add_edge",
        [](ScriptContext& context, const ScriptCommand& command, std::vector<std::string>& messages) {
            if (!context.hasClothSolver) {
                messages.push_back("sim.cloth.add_edge requires sim.cloth.create first");
                return false;
            }
            const auto a = parseIntArg(command, "a");
            const auto b = parseIntArg(command, "b");
            if (!a || !b || *a <= 0 || *b <= 0) {
                messages.push_back("sim.cloth.add_edge requires a=>0 and b=>0");
                return false;
            }
            const float rest = parseFloatArg(command, "rest").value_or(1.0f);
            const float stiff = parseFloatArg(command, "stiff").value_or(100.0f);
            if (!context.clothSolver->addEdge(static_cast<nexus::ClothNodeId>(*a),
                                              static_cast<nexus::ClothNodeId>(*b),
                                              rest,
                                              stiff)) {
                messages.push_back("sim.cloth.add_edge failed");
                return false;
            }
            messages.push_back("cloth edge added a=" + std::to_string(*a) + " b=" + std::to_string(*b));
            return true;
        });

    m_registry.registerCommand("sim.cloth.add_node",
        [](ScriptContext& context, const ScriptCommand& command, std::vector<std::string>& messages) {
            if (!context.hasClothSolver) {
                messages.push_back("sim.cloth.add_node requires sim.cloth.create first");
                return false;
            }
            nexus::ClothNodeDesc desc{};
            desc.mass        = parseFloatArg(command, "mass").value_or(1.f);
            desc.position.x  = parseFloatArg(command, "tx").value_or(0.f);
            desc.position.y  = parseFloatArg(command, "ty").value_or(0.f);
            desc.position.z  = parseFloatArg(command, "tz").value_or(0.f);
            const nexus::ClothNodeId id = context.clothSolver->addNode(desc);
            messages.push_back("cloth node added id=" + std::to_string(id)
                + " mass=" + std::to_string(desc.mass));
            return true;
        });

    m_registry.registerCommand("sim.cloth.step",
        [](ScriptContext& context, const ScriptCommand& command, std::vector<std::string>& messages) {
            if (!context.hasClothSolver) {
                messages.push_back("sim.cloth.step requires sim.cloth.create first");
                return false;
            }
            const double dt = static_cast<double>(parseFloatArg(command, "dt").value_or(0.016f));
            const nexus::ClothStepReport report = context.clothSolver->step(dt);
            if (!report.ok) {
                messages.push_back("sim.cloth.step failed (invalid dt)");
                return false;
            }
            messages.push_back("cloth step t=" + std::to_string(report.simulationTime)
                + " nodes=" + std::to_string(report.nodesIntegrated));
            return true;
        });

    m_registry.registerCommand("sim.cloth.capture_state",
        [](ScriptContext& context, const ScriptCommand&, std::vector<std::string>& messages) {
            if (!context.hasClothSolver) {
                messages.push_back("sim.cloth.capture_state requires sim.cloth.create first");
                return false;
            }
            context.clothLastState = context.clothSolver->captureState();
            context.hasClothLastState = true;
            messages.push_back("cloth state captured nodes="
                + std::to_string(context.clothLastState.nodes.size()));
            return true;
        });

    m_registry.registerCommand("sim.cloth.set_baseline",
        [](ScriptContext& context, const ScriptCommand&, std::vector<std::string>& messages) {
            if (!context.hasClothSolver) {
                messages.push_back("sim.cloth.set_baseline requires sim.cloth.create first");
                return false;
            }
            context.clothLastState = context.clothSolver->captureState();
            context.hasClothLastState = true;
            messages.push_back("cloth baseline set nodes="
                + std::to_string(context.clothLastState.nodes.size()));
            return true;
        });

    m_registry.registerCommand("sim.cloth.state_hash",
        [](ScriptContext& context, const ScriptCommand&, std::vector<std::string>& messages) {
            if (!context.hasClothSolver) {
                messages.push_back("sim.cloth.state_hash requires sim.cloth.create first");
                return false;
            }
            const nexus::ClothState current = context.clothSolver->captureState();
            const std::vector<uint8_t> bytes = serializeClothState(current);
            messages.push_back("cloth hash=" + hashHex(hashBytesFnv1a64(bytes))
                + " bytes=" + std::to_string(bytes.size()));
            return true;
        });

    m_registry.registerCommand("sim.cloth.state_summary",
        [](ScriptContext& context, const ScriptCommand&, std::vector<std::string>& messages) {
            if (!context.hasClothSolver) {
                messages.push_back("sim.cloth.state_summary requires sim.cloth.create first");
                return false;
            }
            const nexus::ClothState current = context.clothSolver->captureState();
            const std::vector<uint8_t> bytes = serializeClothState(current);
            messages.push_back("cloth summary nodes=" + std::to_string(current.nodes.size())
                + " time=" + std::to_string(current.simulationTime)
                + " bytes=" + std::to_string(bytes.size())
                + " hash=" + hashHex(hashBytesFnv1a64(bytes)));
            return true;
        });

    m_registry.registerCommand("sim.cloth.expect_hash",
        [](ScriptContext& context, const ScriptCommand& command, std::vector<std::string>& messages) {
            if (!context.hasClothSolver) {
                messages.push_back("sim.cloth.expect_hash requires sim.cloth.create first");
                return false;
            }
            const auto expected = parseExpectedHashArg(command);
            if (!expected) {
                messages.push_back("sim.cloth.expect_hash requires hash=<uint64|0xHEX>");
                return false;
            }
            const nexus::ClothState current = context.clothSolver->captureState();
            const std::vector<uint8_t> bytes = serializeClothState(current);
            const uint64_t actual = hashBytesFnv1a64(bytes);
            if (actual != *expected) {
                messages.push_back("sim.cloth.expect_hash mismatch expected="
                    + hashHex(*expected) + " actual=" + hashHex(actual));
                return false;
            }
            messages.push_back("sim.cloth.expect_hash match " + hashHex(actual));
            return true;
        });

    m_registry.registerCommand("sim.cloth.diff_state",
        [](ScriptContext& context, const ScriptCommand&, std::vector<std::string>& messages) {
            if (!context.hasClothSolver) {
                messages.push_back("sim.cloth.diff_state requires sim.cloth.create first");
                return false;
            }
            if (!context.hasClothLastState) {
                messages.push_back("sim.cloth.diff_state requires sim.cloth.capture_state, sim.cloth.export_state, or sim.cloth.import_state first");
                return false;
            }
            const nexus::ClothState current = context.clothSolver->captureState();
            const std::vector<uint8_t> currBytes = serializeClothState(current);
            const std::vector<uint8_t> baseBytes = serializeClothState(context.clothLastState);
            const size_t changed = byteDiffCount(baseBytes, currBytes);
            const size_t firstDiff = firstByteDiff(baseBytes, currBytes);
            messages.push_back("cloth diff equal=" + std::string(changed == 0 ? "1" : "0")
                + " changed=" + std::to_string(changed)
                + " first_diff=" + std::string(firstDiff == static_cast<size_t>(-1) ? "-1" : std::to_string(firstDiff))
                + " base_hash=" + hashHex(hashBytesFnv1a64(baseBytes))
                + " curr_hash=" + hashHex(hashBytesFnv1a64(currBytes)));
            return true;
        });

    m_registry.registerCommand("sim.cloth.restore_state",
        [](ScriptContext& context, const ScriptCommand&, std::vector<std::string>& messages) {
            if (!context.hasClothSolver) {
                messages.push_back("sim.cloth.restore_state requires sim.cloth.create first");
                return false;
            }
            if (!context.clothSolver->restoreState(context.clothLastState)) {
                messages.push_back("sim.cloth.restore_state failed");
                return false;
            }
            messages.push_back("cloth state restored");
            return true;
        });

    m_registry.registerCommand("sim.cloth.export_state",
        [](ScriptContext& context, const ScriptCommand& command, std::vector<std::string>& messages) {
            if (!context.hasClothSolver) {
                messages.push_back("sim.cloth.export_state requires sim.cloth.create first");
                return false;
            }
            const auto pathArg = getArg(command, "path");
            if (!pathArg) {
                messages.push_back("sim.cloth.export_state requires path=");
                return false;
            }
            context.clothLastState = context.clothSolver->captureState();
            context.hasClothLastState = true;
            const std::vector<uint8_t> bytes = serializeClothState(context.clothLastState);
            const std::filesystem::path target = normalizePath(context.workingDirectory, *pathArg);
            if (!writeBytesToFile(target, bytes, messages)) {
                return false;
            }
            messages.push_back("cloth state exported bytes=" + std::to_string(bytes.size()));
            return true;
        });

    m_registry.registerCommand("sim.cloth.import_state",
        [](ScriptContext& context, const ScriptCommand& command, std::vector<std::string>& messages) {
            if (!context.hasClothSolver) {
                messages.push_back("sim.cloth.import_state requires sim.cloth.create first");
                return false;
            }
            const auto pathArg = getArg(command, "path");
            if (!pathArg) {
                messages.push_back("sim.cloth.import_state requires path=");
                return false;
            }
            std::vector<uint8_t> bytes;
            const std::filesystem::path target = normalizePath(context.workingDirectory, *pathArg);
            if (!readBytesFromFile(target, bytes, messages)) {
                return false;
            }
            nexus::ClothState state;
            if (!deserializeClothState(bytes, state)) {
                messages.push_back("sim.cloth.import_state deserialize failed");
                return false;
            }
            if (!context.clothSolver->restoreState(state)) {
                messages.push_back("sim.cloth.import_state restore failed");
                return false;
            }
            context.clothLastState = std::move(state);
            context.hasClothLastState = true;
            messages.push_back("cloth state imported nodes="
                + std::to_string(context.clothLastState.nodes.size()));
            return true;
        });

    // ── Fluid simulation commands ─────────────────────────────────────────────

    m_registry.registerCommand("sim.fluid.create",
        [](ScriptContext& context, const ScriptCommand& command, std::vector<std::string>& messages) {
            context.fluidSolver  = std::make_unique<nexus::FluidSolver>();
            context.hasFluidSolver = true;
            context.hasFluidLastState = false;
            const float gx = parseFloatArg(command, "gx").value_or(0.f);
            const float gy = parseFloatArg(command, "gy").value_or(-9.81f);
            const float gz = parseFloatArg(command, "gz").value_or(0.f);
            const float h = parseFloatArg(command, "h").value_or(0.1f);
            const float k = parseFloatArg(command, "k").value_or(200.0f);
            context.fluidSolver->setGravity({gx, gy, gz});
            context.fluidSolver->setSmoothingRadius(h);
            context.fluidSolver->setPressureStiffness(k);
            messages.push_back("fluid solver created");
            return true;
        });

    m_registry.registerCommand("sim.fluid.add_particle",
        [](ScriptContext& context, const ScriptCommand& command, std::vector<std::string>& messages) {
            if (!context.hasFluidSolver) {
                messages.push_back("sim.fluid.add_particle requires sim.fluid.create first");
                return false;
            }
            nexus::FluidParticleDesc desc{};
            desc.mass        = parseFloatArg(command, "mass").value_or(1.f);
            desc.position.x  = parseFloatArg(command, "tx").value_or(0.f);
            desc.position.y  = parseFloatArg(command, "ty").value_or(0.f);
            desc.position.z  = parseFloatArg(command, "tz").value_or(0.f);
            desc.velocity.x  = parseFloatArg(command, "vx").value_or(0.f);
            desc.velocity.y  = parseFloatArg(command, "vy").value_or(0.f);
            desc.velocity.z  = parseFloatArg(command, "vz").value_or(0.f);
            desc.density     = parseFloatArg(command, "density").value_or(1000.0f);
            const nexus::FluidParticleId id = context.fluidSolver->addParticle(desc);
            messages.push_back("fluid particle added id=" + std::to_string(id)
                + " mass=" + std::to_string(desc.mass));
            return true;
        });

    m_registry.registerCommand("sim.fluid.step",
        [](ScriptContext& context, const ScriptCommand& command, std::vector<std::string>& messages) {
            if (!context.hasFluidSolver) {
                messages.push_back("sim.fluid.step requires sim.fluid.create first");
                return false;
            }
            const double dt = static_cast<double>(parseFloatArg(command, "dt").value_or(0.016f));
            const nexus::FluidStepReport report = context.fluidSolver->step(dt);
            if (!report.ok) {
                messages.push_back("sim.fluid.step failed (invalid dt)");
                return false;
            }
            messages.push_back("fluid step t=" + std::to_string(report.simulationTime)
                + " particles=" + std::to_string(report.particlesAdvanced));
            return true;
        });

    m_registry.registerCommand("sim.fluid.capture_state",
        [](ScriptContext& context, const ScriptCommand&, std::vector<std::string>& messages) {
            if (!context.hasFluidSolver) {
                messages.push_back("sim.fluid.capture_state requires sim.fluid.create first");
                return false;
            }
            context.fluidLastState = context.fluidSolver->captureState();
            context.hasFluidLastState = true;
            messages.push_back("fluid state captured particles="
                + std::to_string(context.fluidLastState.particles.size()));
            return true;
        });

    m_registry.registerCommand("sim.fluid.set_baseline",
        [](ScriptContext& context, const ScriptCommand&, std::vector<std::string>& messages) {
            if (!context.hasFluidSolver) {
                messages.push_back("sim.fluid.set_baseline requires sim.fluid.create first");
                return false;
            }
            context.fluidLastState = context.fluidSolver->captureState();
            context.hasFluidLastState = true;
            messages.push_back("fluid baseline set particles="
                + std::to_string(context.fluidLastState.particles.size()));
            return true;
        });

    m_registry.registerCommand("sim.fluid.state_hash",
        [](ScriptContext& context, const ScriptCommand&, std::vector<std::string>& messages) {
            if (!context.hasFluidSolver) {
                messages.push_back("sim.fluid.state_hash requires sim.fluid.create first");
                return false;
            }
            const nexus::FluidState current = context.fluidSolver->captureState();
            const std::vector<uint8_t> bytes = serializeFluidState(current);
            messages.push_back("fluid hash=" + hashHex(hashBytesFnv1a64(bytes))
                + " bytes=" + std::to_string(bytes.size()));
            return true;
        });

    m_registry.registerCommand("sim.fluid.state_summary",
        [](ScriptContext& context, const ScriptCommand&, std::vector<std::string>& messages) {
            if (!context.hasFluidSolver) {
                messages.push_back("sim.fluid.state_summary requires sim.fluid.create first");
                return false;
            }
            const nexus::FluidState current = context.fluidSolver->captureState();
            const std::vector<uint8_t> bytes = serializeFluidState(current);
            messages.push_back("fluid summary particles=" + std::to_string(current.particles.size())
                + " time=" + std::to_string(current.simulationTime)
                + " bytes=" + std::to_string(bytes.size())
                + " hash=" + hashHex(hashBytesFnv1a64(bytes)));
            return true;
        });

    m_registry.registerCommand("sim.fluid.expect_hash",
        [](ScriptContext& context, const ScriptCommand& command, std::vector<std::string>& messages) {
            if (!context.hasFluidSolver) {
                messages.push_back("sim.fluid.expect_hash requires sim.fluid.create first");
                return false;
            }
            const auto expected = parseExpectedHashArg(command);
            if (!expected) {
                messages.push_back("sim.fluid.expect_hash requires hash=<uint64|0xHEX>");
                return false;
            }
            const nexus::FluidState current = context.fluidSolver->captureState();
            const std::vector<uint8_t> bytes = serializeFluidState(current);
            const uint64_t actual = hashBytesFnv1a64(bytes);
            if (actual != *expected) {
                messages.push_back("sim.fluid.expect_hash mismatch expected="
                    + hashHex(*expected) + " actual=" + hashHex(actual));
                return false;
            }
            messages.push_back("sim.fluid.expect_hash match " + hashHex(actual));
            return true;
        });

    m_registry.registerCommand("sim.fluid.diff_state",
        [](ScriptContext& context, const ScriptCommand&, std::vector<std::string>& messages) {
            if (!context.hasFluidSolver) {
                messages.push_back("sim.fluid.diff_state requires sim.fluid.create first");
                return false;
            }
            if (!context.hasFluidLastState) {
                messages.push_back("sim.fluid.diff_state requires sim.fluid.capture_state, sim.fluid.export_state, or sim.fluid.import_state first");
                return false;
            }
            const nexus::FluidState current = context.fluidSolver->captureState();
            const std::vector<uint8_t> currBytes = serializeFluidState(current);
            const std::vector<uint8_t> baseBytes = serializeFluidState(context.fluidLastState);
            const size_t changed = byteDiffCount(baseBytes, currBytes);
            const size_t firstDiff = firstByteDiff(baseBytes, currBytes);
            messages.push_back("fluid diff equal=" + std::string(changed == 0 ? "1" : "0")
                + " changed=" + std::to_string(changed)
                + " first_diff=" + std::string(firstDiff == static_cast<size_t>(-1) ? "-1" : std::to_string(firstDiff))
                + " base_hash=" + hashHex(hashBytesFnv1a64(baseBytes))
                + " curr_hash=" + hashHex(hashBytesFnv1a64(currBytes)));
            return true;
        });

    m_registry.registerCommand("sim.fluid.restore_state",
        [](ScriptContext& context, const ScriptCommand&, std::vector<std::string>& messages) {
            if (!context.hasFluidSolver) {
                messages.push_back("sim.fluid.restore_state requires sim.fluid.create first");
                return false;
            }
            if (!context.fluidSolver->restoreState(context.fluidLastState)) {
                messages.push_back("sim.fluid.restore_state failed");
                return false;
            }
            messages.push_back("fluid state restored");
            return true;
        });

    m_registry.registerCommand("sim.fluid.export_state",
        [](ScriptContext& context, const ScriptCommand& command, std::vector<std::string>& messages) {
            if (!context.hasFluidSolver) {
                messages.push_back("sim.fluid.export_state requires sim.fluid.create first");
                return false;
            }
            const auto pathArg = getArg(command, "path");
            if (!pathArg) {
                messages.push_back("sim.fluid.export_state requires path=");
                return false;
            }
            context.fluidLastState = context.fluidSolver->captureState();
            context.hasFluidLastState = true;
            const std::vector<uint8_t> bytes = serializeFluidState(context.fluidLastState);
            const std::filesystem::path target = normalizePath(context.workingDirectory, *pathArg);
            if (!writeBytesToFile(target, bytes, messages)) {
                return false;
            }
            messages.push_back("fluid state exported bytes=" + std::to_string(bytes.size()));
            return true;
        });

    m_registry.registerCommand("sim.fluid.import_state",
        [](ScriptContext& context, const ScriptCommand& command, std::vector<std::string>& messages) {
            if (!context.hasFluidSolver) {
                messages.push_back("sim.fluid.import_state requires sim.fluid.create first");
                return false;
            }
            const auto pathArg = getArg(command, "path");
            if (!pathArg) {
                messages.push_back("sim.fluid.import_state requires path=");
                return false;
            }
            std::vector<uint8_t> bytes;
            const std::filesystem::path target = normalizePath(context.workingDirectory, *pathArg);
            if (!readBytesFromFile(target, bytes, messages)) {
                return false;
            }
            nexus::FluidState state;
            if (!deserializeFluidState(bytes, state)) {
                messages.push_back("sim.fluid.import_state deserialize failed");
                return false;
            }
            if (!context.fluidSolver->restoreState(state)) {
                messages.push_back("sim.fluid.import_state restore failed");
                return false;
            }
            context.fluidLastState = std::move(state);
            context.hasFluidLastState = true;
            messages.push_back("fluid state imported particles="
                + std::to_string(context.fluidLastState.particles.size()));
            return true;
        });

    // ── Gaussian Splatting commands ───────────────────────────────────────────

    m_registry.registerCommand("gaussian.load_ply",
        [](ScriptContext& context, const ScriptCommand& command, std::vector<std::string>& messages) {
            const auto pathArg = getArg(command, "path");
            if (!pathArg) {
                messages.push_back("gaussian.load_ply requires path=");
                return false;
            }
            const std::filesystem::path target = normalizePath(context.workingDirectory, *pathArg);
            auto cloud = nexus::gfx::GaussianSplatCloud::loadFromPLY(target.string());
            if (!cloud) {
                messages.push_back("gaussian.load_ply failed: " + target.string());
                return false;
            }
            context.gaussianCloud    = std::move(*cloud);
            context.hasGaussianCloud = true;
            messages.push_back("gaussian cloud loaded splats="
                + std::to_string(context.gaussianCloud.splatCount()));
            return true;
        });

    m_registry.registerCommand("gaussian.describe",
        [](ScriptContext& context, const ScriptCommand&, std::vector<std::string>& messages) {
            if (!context.hasGaussianCloud) {
                messages.push_back("gaussian.describe requires gaussian.load_ply first");
                return false;
            }
            messages.push_back("gaussian splats=" + std::to_string(context.gaussianCloud.splatCount()));
            messages.push_back("gaussian format=" + context.gaussianCloud.sourceFormat);
            messages.push_back("gaussian sh_degree=" + std::to_string(context.gaussianCloud.shDegree));
            return true;
        });

    // ── Mesh hashing and baseline commands ───────────────────────────────────

    m_registry.registerCommand("mesh.hash",
        [](ScriptContext& context, const ScriptCommand&, std::vector<std::string>& messages) {
            if (!context.hasMesh) {
                messages.push_back("mesh.hash requires a mesh");
                return false;
            }
            const auto bytes = serializeMeshState(context.mesh);
            messages.push_back("mesh hash=" + hashHex(hashBytesFnv1a64(bytes))
                + " vertices=" + std::to_string(context.mesh.attributes().vertexCount())
                + " faces=" + std::to_string(context.mesh.topology().faceCount())
                + " bytes=" + std::to_string(bytes.size()));
            return true;
        });

    m_registry.registerCommand("mesh.set_baseline",
        [](ScriptContext& context, const ScriptCommand&, std::vector<std::string>& messages) {
            if (!context.hasMesh) {
                messages.push_back("mesh.set_baseline requires a mesh");
                return false;
            }
            context.meshBaseline = context.mesh;
            context.hasMeshBaseline = true;
            messages.push_back("mesh baseline set vertices="
                + std::to_string(context.mesh.attributes().vertexCount())
                + " faces=" + std::to_string(context.mesh.topology().faceCount()));
            return true;
        });

    m_registry.registerCommand("mesh.diff",
        [](ScriptContext& context, const ScriptCommand&, std::vector<std::string>& messages) {
            if (!context.hasMesh) {
                messages.push_back("mesh.diff requires a mesh");
                return false;
            }
            if (!context.hasMeshBaseline) {
                messages.push_back("mesh.diff requires mesh.set_baseline first");
                return false;
            }
            const auto currBytes = serializeMeshState(context.mesh);
            const auto baseBytes = serializeMeshState(context.meshBaseline);
            const size_t changed = byteDiffCount(baseBytes, currBytes);
            const size_t firstDiff = firstByteDiff(baseBytes, currBytes);
            messages.push_back("mesh diff equal=" + std::string(changed == 0 ? "1" : "0")
                + " changed=" + std::to_string(changed)
                + " first_diff=" + std::string(firstDiff == static_cast<size_t>(-1) ? "-1" : std::to_string(firstDiff))
                + " base_hash=" + hashHex(hashBytesFnv1a64(baseBytes))
                + " curr_hash=" + hashHex(hashBytesFnv1a64(currBytes)));
            return true;
        });

    m_registry.registerCommand("mesh.expect_hash",
        [](ScriptContext& context, const ScriptCommand& command, std::vector<std::string>& messages) {
            if (!context.hasMesh) {
                messages.push_back("mesh.expect_hash requires a mesh");
                return false;
            }
            const auto expected = parseExpectedHashArg(command);
            if (!expected) {
                messages.push_back("mesh.expect_hash requires hash=<uint64|0xHEX>");
                return false;
            }
            const auto bytes = serializeMeshState(context.mesh);
            const uint64_t actual = hashBytesFnv1a64(bytes);
            if (actual != *expected) {
                messages.push_back("mesh.expect_hash mismatch expected="
                    + hashHex(*expected) + " actual=" + hashHex(actual));
                return false;
            }
            messages.push_back("mesh.expect_hash match " + hashHex(actual));
            return true;
        });

    // ── Scene hashing and baseline commands ──────────────────────────────────

    m_registry.registerCommand("scene.hash",
        [](ScriptContext& context, const ScriptCommand&, std::vector<std::string>& messages) {
            if (!context.hasScene) {
                messages.push_back("scene.hash requires a scene");
                return false;
            }
            const auto bytes = serializeSceneState(context.scene);
            messages.push_back("scene hash=" + hashHex(hashBytesFnv1a64(bytes))
                + " entries=" + std::to_string(context.scene.entryCount())
                + " bytes=" + std::to_string(bytes.size()));
            return true;
        });

    m_registry.registerCommand("scene.set_baseline",
        [](ScriptContext& context, const ScriptCommand&, std::vector<std::string>& messages) {
            if (!context.hasScene) {
                messages.push_back("scene.set_baseline requires a scene");
                return false;
            }
            context.sceneBaselineBytes = serializeSceneState(context.scene);
            context.hasSceneBaseline = true;
            messages.push_back("scene baseline set entries="
                + std::to_string(context.scene.entryCount()));
            return true;
        });

    m_registry.registerCommand("scene.diff",
        [](ScriptContext& context, const ScriptCommand&, std::vector<std::string>& messages) {
            if (!context.hasScene) {
                messages.push_back("scene.diff requires a scene");
                return false;
            }
            if (!context.hasSceneBaseline) {
                messages.push_back("scene.diff requires scene.set_baseline first");
                return false;
            }
            const auto currBytes = serializeSceneState(context.scene);
            const auto& baseBytes = context.sceneBaselineBytes;
            const size_t changed = byteDiffCount(baseBytes, currBytes);
            const size_t firstDiff = firstByteDiff(baseBytes, currBytes);
            messages.push_back("scene diff equal=" + std::string(changed == 0 ? "1" : "0")
                + " changed=" + std::to_string(changed)
                + " first_diff=" + std::string(firstDiff == static_cast<size_t>(-1) ? "-1" : std::to_string(firstDiff))
                + " base_hash=" + hashHex(hashBytesFnv1a64(baseBytes))
                + " curr_hash=" + hashHex(hashBytesFnv1a64(currBytes)));
            return true;
        });

    m_registry.registerCommand("scene.export_bundle",
        [](ScriptContext& context, const ScriptCommand& command, std::vector<std::string>& messages) {
            if (!context.hasScene) {
                messages.push_back("scene.export_bundle requires a scene");
                return false;
            }
            const auto pathArg = getArg(command, "path");
            if (!pathArg) {
                messages.push_back("scene.export_bundle requires path=");
                return false;
            }
            const std::filesystem::path target = normalizePath(context.workingDirectory, *pathArg);
            const auto bytes = serializeSceneState(context.scene);
            const std::string sceneHash = hashHex(hashBytesFnv1a64(bytes));
            std::ostringstream oss;
            oss << "{\"scene_hash\":\"" << sceneHash << "\""
                << ",\"entry_count\":" << context.scene.entryCount()
                << ",\"scene_name\":\"" << jsonEscape(context.scene.sceneName()) << "\""
                << "}";
            const std::string json = oss.str();
            std::vector<uint8_t> outBytes(json.begin(), json.end());
            if (!writeBytesToFile(target, outBytes, messages)) {
                return false;
            }
            messages.push_back("scene bundle exported hash=" + sceneHash
                + " bytes=" + std::to_string(outBytes.size()));
            return true;
        });

    m_registry.registerCommand("scene.verify_bundle",
        [](ScriptContext& context, const ScriptCommand& command, std::vector<std::string>& messages) {
            if (!context.hasScene) {
                messages.push_back("scene.verify_bundle requires a scene");
                return false;
            }
            const auto pathArg = getArg(command, "path");
            if (!pathArg) {
                messages.push_back("scene.verify_bundle requires path=");
                return false;
            }
            const std::filesystem::path source = normalizePath(context.workingDirectory, *pathArg);
            std::vector<uint8_t> input;
            if (!readBytesFromFile(source, input, messages)) {
                return false;
            }
            const std::string text(input.begin(), input.end());
            const auto expectedHash = extractJsonStringField(text, "scene_hash");
            if (!expectedHash) {
                messages.push_back("scene.verify_bundle missing scene_hash in: " + makePathString(source));
                return false;
            }
            const auto currBytes = serializeSceneState(context.scene);
            const std::string actualHash = hashHex(hashBytesFnv1a64(currBytes));
            if (*expectedHash != actualHash) {
                messages.push_back("scene.verify_bundle mismatch expected=" + *expectedHash
                    + " actual=" + actualHash);
                return false;
            }
            messages.push_back("scene.verify_bundle match: " + actualHash);
            return true;
        });

    // ── Animation state hash and baseline commands ────────────────────────────

    m_registry.registerCommand("animation.state_hash",
        [](ScriptContext& context, const ScriptCommand&, std::vector<std::string>& messages) {
            if (!context.hasSkeleton) {
                messages.push_back("animation.state_hash requires a skeleton");
                return false;
            }
            const auto bytes = serializeAnimationState(context.skeleton, context.pose);
            messages.push_back("animation hash=" + hashHex(hashBytesFnv1a64(bytes))
                + " bones=" + std::to_string(context.skeleton.boneCount())
                + " bytes=" + std::to_string(bytes.size()));
            return true;
        });

    m_registry.registerCommand("animation.set_baseline",
        [](ScriptContext& context, const ScriptCommand&, std::vector<std::string>& messages) {
            if (!context.hasSkeleton) {
                messages.push_back("animation.set_baseline requires a skeleton");
                return false;
            }
            context.skeletonBaseline = context.skeleton;
            context.poseBaseline = context.pose;
            context.hasAnimBaseline = true;
            messages.push_back("animation baseline set bones="
                + std::to_string(context.skeleton.boneCount()));
            return true;
        });

    m_registry.registerCommand("animation.diff",
        [](ScriptContext& context, const ScriptCommand&, std::vector<std::string>& messages) {
            if (!context.hasSkeleton) {
                messages.push_back("animation.diff requires a skeleton");
                return false;
            }
            if (!context.hasAnimBaseline) {
                messages.push_back("animation.diff requires animation.set_baseline first");
                return false;
            }
            const auto currBytes = serializeAnimationState(context.skeleton, context.pose);
            const auto baseBytes = serializeAnimationState(context.skeletonBaseline, context.poseBaseline);
            const size_t changed = byteDiffCount(baseBytes, currBytes);
            const size_t firstDiff = firstByteDiff(baseBytes, currBytes);
            messages.push_back("animation diff equal=" + std::string(changed == 0 ? "1" : "0")
                + " changed=" + std::to_string(changed)
                + " first_diff=" + std::string(firstDiff == static_cast<size_t>(-1) ? "-1" : std::to_string(firstDiff))
                + " base_hash=" + hashHex(hashBytesFnv1a64(baseBytes))
                + " curr_hash=" + hashHex(hashBytesFnv1a64(currBytes)));
            return true;
        });

    m_registry.registerCommand("animation.expect_hash",
        [](ScriptContext& context, const ScriptCommand& command, std::vector<std::string>& messages) {
            if (!context.hasSkeleton) {
                messages.push_back("animation.expect_hash requires a skeleton");
                return false;
            }
            const auto expected = parseExpectedHashArg(command);
            if (!expected) {
                messages.push_back("animation.expect_hash requires hash=<uint64|0xHEX>");
                return false;
            }
            const auto bytes = serializeAnimationState(context.skeleton, context.pose);
            const uint64_t actual = hashBytesFnv1a64(bytes);
            if (actual != *expected) {
                messages.push_back("animation.expect_hash mismatch expected="
                    + hashHex(*expected) + " actual=" + hashHex(actual));
                return false;
            }
            messages.push_back("animation.expect_hash match " + hashHex(actual));
            return true;
        });

    // ── Cross-solver interaction verification commands ────────────────────────

    m_registry.registerCommand("sim.cross_solver_hash",
        [](ScriptContext& context, const ScriptCommand&, std::vector<std::string>& messages) {
            const auto bytes = serializeCrossSolverState(context);
            messages.push_back("cross_solver hash=" + hashHex(hashBytesFnv1a64(bytes))
                + " rigid=" + (context.hasRigidSolver ? "1" : "0")
                + " cloth=" + (context.hasClothSolver ? "1" : "0")
                + " fluid=" + (context.hasFluidSolver ? "1" : "0"));
            return true;
        });

    m_registry.registerCommand("sim.export_cross_solver_bundle",
        [](ScriptContext& context, const ScriptCommand& command, std::vector<std::string>& messages) {
            const auto pathArg = getArg(command, "path");
            if (!pathArg) {
                messages.push_back("sim.export_cross_solver_bundle requires path=");
                return false;
            }
            const std::filesystem::path target = normalizePath(context.workingDirectory, *pathArg);
            const std::string json = makeCrossSolverBundleJson(context);
            std::vector<uint8_t> outBytes(json.begin(), json.end());
            if (!writeBytesToFile(target, outBytes, messages)) {
                return false;
            }
            messages.push_back("cross_solver bundle exported bytes=" + std::to_string(outBytes.size()));
            return true;
        });

    m_registry.registerCommand("sim.verify_cross_solver_bundle",
        [](ScriptContext& context, const ScriptCommand& command, std::vector<std::string>& messages) {
            const auto pathArg = getArg(command, "path");
            if (!pathArg) {
                messages.push_back("sim.verify_cross_solver_bundle requires path=");
                return false;
            }
            const std::filesystem::path source = normalizePath(context.workingDirectory, *pathArg);
            std::vector<uint8_t> input;
            if (!readBytesFromFile(source, input, messages)) {
                return false;
            }
            const std::string text(input.begin(), input.end());
            const auto expectedHash = extractJsonStringField(text, "cross_solver_hash");
            if (!expectedHash) {
                messages.push_back("sim.verify_cross_solver_bundle missing cross_solver_hash in: "
                    + makePathString(source));
                return false;
            }
            const auto currBytes = serializeCrossSolverState(context);
            const std::string actualHash = hashHex(hashBytesFnv1a64(currBytes));
            if (*expectedHash != actualHash) {
                messages.push_back("sim.verify_cross_solver_bundle mismatch expected=" + *expectedHash
                    + " actual=" + actualHash);
                return false;
            }
            messages.push_back("sim.verify_cross_solver_bundle match: " + actualHash);
            return true;
        });

    // ── Parametric commands ────────────────────────────────────────────────────

    m_registry.registerCommand("parametric.new",
        [](ScriptContext& context, const ScriptCommand&, std::vector<std::string>& messages) {
            context.parametricGraph    = nexus::parametric::ConstraintGraph{};
            context.hasParametricGraph = true;
            messages.push_back("parametric graph created");
            return true;
        });

    m_registry.registerCommand("parametric.add_point",
        [](ScriptContext& context, const ScriptCommand& command, std::vector<std::string>& messages) {
            if (!context.hasParametricGraph) {
                messages.push_back("parametric.add_point requires parametric.new first");
                return false;
            }
            const double x = parseDoubleArg(command, "x").value_or(0.0);
            const double y = parseDoubleArg(command, "y").value_or(0.0);
            const double z = parseDoubleArg(command, "z").value_or(0.0);
            const auto id = context.parametricGraph.addPoint({x, y, z});
            messages.push_back("parametric entity id=" + std::to_string(id)
                + " x=" + std::to_string(x)
                + " y=" + std::to_string(y)
                + " z=" + std::to_string(z));
            return true;
        });

    m_registry.registerCommand("parametric.remove_entity",
        [](ScriptContext& context, const ScriptCommand& command, std::vector<std::string>& messages) {
            if (!context.hasParametricGraph) {
                messages.push_back("parametric.remove_entity requires parametric.new first");
                return false;
            }
            const auto idArg = parseIntArg(command, "id");
            if (!idArg || *idArg <= 0) {
                messages.push_back("parametric.remove_entity requires valid id=");
                return false;
            }
            const auto entityId = static_cast<nexus::parametric::ParametricEntityId>(*idArg);
            if (!context.parametricGraph.removeEntity(entityId)) {
                messages.push_back("parametric.remove_entity not found id=" + std::to_string(entityId));
                return false;
            }
            messages.push_back("parametric entity removed id=" + std::to_string(entityId));
            return true;
        });

    m_registry.registerCommand("parametric.add_distance_constraint",
        [](ScriptContext& context, const ScriptCommand& command, std::vector<std::string>& messages) {
            if (!context.hasParametricGraph) {
                messages.push_back("parametric.add_distance_constraint requires parametric.new first");
                return false;
            }
            const auto aArg    = parseIntArg(command, "a");
            const auto bArg    = parseIntArg(command, "b");
            const auto distArg = parseDoubleArg(command, "dist");
            if (!aArg || !bArg || !distArg) {
                messages.push_back("parametric.add_distance_constraint requires a= b= dist=");
                return false;
            }
            const auto idA = static_cast<nexus::parametric::ParametricEntityId>(*aArg);
            const auto idB = static_cast<nexus::parametric::ParametricEntityId>(*bArg);
            const auto cid = context.parametricGraph.addDistanceConstraint(idA, idB, *distArg);
            if (cid == nexus::parametric::kInvalidConstraintId) {
                messages.push_back("parametric.add_distance_constraint failed: invalid entity ids");
                return false;
            }
            messages.push_back("parametric distance constraint id=" + std::to_string(cid)
                + " a=" + std::to_string(idA) + " b=" + std::to_string(idB)
                + " dist=" + std::to_string(*distArg));
            return true;
        });

    m_registry.registerCommand("parametric.add_coincident_constraint",
        [](ScriptContext& context, const ScriptCommand& command, std::vector<std::string>& messages) {
            if (!context.hasParametricGraph) {
                messages.push_back("parametric.add_coincident_constraint requires parametric.new first");
                return false;
            }
            const auto aArg = parseIntArg(command, "a");
            const auto bArg = parseIntArg(command, "b");
            if (!aArg || !bArg) {
                messages.push_back("parametric.add_coincident_constraint requires a= b=");
                return false;
            }
            const auto idA = static_cast<nexus::parametric::ParametricEntityId>(*aArg);
            const auto idB = static_cast<nexus::parametric::ParametricEntityId>(*bArg);
            const auto cid = context.parametricGraph.addCoincidentConstraint(idA, idB);
            if (cid == nexus::parametric::kInvalidConstraintId) {
                messages.push_back("parametric.add_coincident_constraint failed: invalid entity ids");
                return false;
            }
            messages.push_back("parametric coincident constraint id=" + std::to_string(cid)
                + " a=" + std::to_string(idA) + " b=" + std::to_string(idB));
            return true;
        });

    m_registry.registerCommand("parametric.add_axis_aligned_distance_constraint",
        [](ScriptContext& context, const ScriptCommand& command, std::vector<std::string>& messages) {
            if (!context.hasParametricGraph) {
                messages.push_back("parametric.add_axis_aligned_distance_constraint requires parametric.new first");
                return false;
            }
            const auto aArg = parseIntArg(command, "a");
            const auto bArg = parseIntArg(command, "b");
            const auto distArg = parseDoubleArg(command, "dist");
            const auto axisArg = getArg(command, "axis");
            if (!aArg || !bArg || !distArg || !axisArg || axisArg->empty()) {
                messages.push_back("parametric.add_axis_aligned_distance_constraint requires a= b= axis= dist=");
                return false;
            }

            nexus::parametric::Axis axis = nexus::parametric::Axis::X;
            if (*axisArg == "x" || *axisArg == "X") {
                axis = nexus::parametric::Axis::X;
            } else if (*axisArg == "y" || *axisArg == "Y") {
                axis = nexus::parametric::Axis::Y;
            } else if (*axisArg == "z" || *axisArg == "Z") {
                axis = nexus::parametric::Axis::Z;
            } else {
                messages.push_back("parametric.add_axis_aligned_distance_constraint invalid axis (use x|y|z)");
                return false;
            }

            const auto idA = static_cast<nexus::parametric::ParametricEntityId>(*aArg);
            const auto idB = static_cast<nexus::parametric::ParametricEntityId>(*bArg);
            const auto cid = context.parametricGraph.addAxisAlignedDistanceConstraint(idA, idB, axis, *distArg);
            if (cid == nexus::parametric::kInvalidConstraintId) {
                messages.push_back("parametric.add_axis_aligned_distance_constraint failed: invalid entity ids");
                return false;
            }
            messages.push_back("parametric axis constraint id=" + std::to_string(cid)
                + " a=" + std::to_string(idA)
                + " b=" + std::to_string(idB)
                + " axis=" + *axisArg
                + " dist=" + std::to_string(*distArg));
            return true;
        });

    m_registry.registerCommand("parametric.solve",
        [](ScriptContext& context, const ScriptCommand& command, std::vector<std::string>& messages) {
            if (!context.hasParametricGraph) {
                messages.push_back("parametric.solve requires parametric.new first");
                return false;
            }
            nexus::parametric::ParametricSolverConfig cfg;
            if (const auto maxIter = parseIntArg(command, "max_iterations"))
                cfg.maxIterations = static_cast<uint32_t>(std::max(1, *maxIter));
            const auto report = nexus::parametric::ParametricSolver::solve(context.parametricGraph, cfg);
            messages.push_back("parametric solved converged=" + std::string(report.converged ? "1" : "0")
                + " iterations=" + std::to_string(report.iterationsRan)
                + " error=" + std::to_string(report.maxConstraintError));
            if (!report.converged) {
                for (const auto& err : report.errors)
                    messages.push_back("parametric solver error: " + err);
            }
            return report.converged;
        });

    m_registry.registerCommand("parametric.get_point",
        [](ScriptContext& context, const ScriptCommand& command, std::vector<std::string>& messages) {
            if (!context.hasParametricGraph) {
                messages.push_back("parametric.get_point requires parametric.new first");
                return false;
            }
            const auto idArg = parseIntArg(command, "id");
            if (!idArg || *idArg <= 0) {
                messages.push_back("parametric.get_point requires valid id=");
                return false;
            }
            const auto entityId = static_cast<nexus::parametric::ParametricEntityId>(*idArg);
            const auto* pt = context.parametricGraph.point(entityId);
            if (!pt) {
                messages.push_back("parametric.get_point not found id=" + std::to_string(entityId));
                return false;
            }
            messages.push_back("parametric point x=" + std::to_string(pt->x)
                + " y=" + std::to_string(pt->y)
                + " z=" + std::to_string(pt->z));
            return true;
        });

    m_registry.registerCommand("parametric.describe",
        [](ScriptContext& context, const ScriptCommand&, std::vector<std::string>& messages) {
            if (!context.hasParametricGraph) {
                messages.push_back("parametric.describe requires parametric.new first");
                return false;
            }
            messages.push_back(
                "parametric entities=" + std::to_string(context.parametricGraph.entityCount())
                + " distance_constraints=" + std::to_string(context.parametricGraph.distanceConstraintCount())
                + " coincident_constraints=" + std::to_string(context.parametricGraph.coincidentConstraintCount())
                + " axis_constraints=" + std::to_string(context.parametricGraph.axisAlignedDistanceConstraintCount()));
            return true;
        });

    m_registry.registerCommand("parametric.hash",
        [](ScriptContext& context, const ScriptCommand&, std::vector<std::string>& messages) {
            if (!context.hasParametricGraph) {
                messages.push_back("parametric.hash requires parametric.new first");
                return false;
            }
            const auto bytes = serializeParametricState(context.parametricGraph);
            messages.push_back("parametric hash=" + hashHex(hashBytesFnv1a64(bytes))
                + " entities=" + std::to_string(context.parametricGraph.entityCount())
                + " bytes=" + std::to_string(bytes.size()));
            return true;
        });

    m_registry.registerCommand("parametric.set_baseline",
        [](ScriptContext& context, const ScriptCommand&, std::vector<std::string>& messages) {
            if (!context.hasParametricGraph) {
                messages.push_back("parametric.set_baseline requires parametric.new first");
                return false;
            }
            context.parametricBaselineBytes = serializeParametricState(context.parametricGraph);
            context.hasParametricBaseline = true;
            messages.push_back("parametric baseline set entities="
                + std::to_string(context.parametricGraph.entityCount()));
            return true;
        });

    m_registry.registerCommand("parametric.diff",
        [](ScriptContext& context, const ScriptCommand&, std::vector<std::string>& messages) {
            if (!context.hasParametricGraph) {
                messages.push_back("parametric.diff requires parametric.new first");
                return false;
            }
            if (!context.hasParametricBaseline) {
                messages.push_back("parametric.diff requires parametric.set_baseline first");
                return false;
            }
            const auto currBytes = serializeParametricState(context.parametricGraph);
            const auto& baseBytes = context.parametricBaselineBytes;
            const size_t changed = byteDiffCount(baseBytes, currBytes);
            const size_t firstDiff = firstByteDiff(baseBytes, currBytes);
            messages.push_back("parametric diff equal=" + std::string(changed == 0 ? "1" : "0")
                + " changed=" + std::to_string(changed)
                + " first_diff=" + std::string(firstDiff == static_cast<size_t>(-1) ? "-1" : std::to_string(firstDiff))
                + " base_hash=" + hashHex(hashBytesFnv1a64(baseBytes))
                + " curr_hash=" + hashHex(hashBytesFnv1a64(currBytes)));
            return true;
        });

    m_registry.registerCommand("parametric.expect_hash",
        [](ScriptContext& context, const ScriptCommand& command, std::vector<std::string>& messages) {
            if (!context.hasParametricGraph) {
                messages.push_back("parametric.expect_hash requires parametric.new first");
                return false;
            }
            const auto expected = parseExpectedHashArg(command);
            if (!expected) {
                messages.push_back("parametric.expect_hash requires hash=<uint64|0xHEX>");
                return false;
            }
            const auto bytes = serializeParametricState(context.parametricGraph);
            const uint64_t actual = hashBytesFnv1a64(bytes);
            if (actual != *expected) {
                messages.push_back("parametric.expect_hash mismatch expected="
                    + hashHex(*expected) + " actual=" + hashHex(actual));
                return false;
            }
            messages.push_back("parametric.expect_hash match " + hashHex(actual));
            return true;
        });

    m_registry.registerCommand("parametric.serialize",
        [](ScriptContext& context, const ScriptCommand& command, std::vector<std::string>& messages) {
            if (!context.hasParametricGraph) {
                messages.push_back("parametric.serialize requires parametric.new first");
                return false;
            }
            const auto pathArg = getArg(command, "path");
            if (!pathArg) {
                messages.push_back("parametric.serialize requires path=");
                return false;
            }
            const std::string serialized =
                nexus::parametric::ParametricGraphSerializer::serialize(context.parametricGraph);
            const std::vector<uint8_t> bytes(serialized.begin(), serialized.end());
            const std::filesystem::path target = normalizePath(context.workingDirectory, *pathArg);
            if (!writeBytesToFile(target, bytes, messages)) {
                return false;
            }
            messages.push_back("parametric graph serialized entities="
                + std::to_string(context.parametricGraph.entityCount())
                + " bytes=" + std::to_string(bytes.size()));
            return true;
        });

    m_registry.registerCommand("parametric.load",
        [](ScriptContext& context, const ScriptCommand& command, std::vector<std::string>& messages) {
            const auto pathArg = getArg(command, "path");
            if (!pathArg) {
                messages.push_back("parametric.load requires path=");
                return false;
            }
            std::vector<uint8_t> rawBytes;
            const std::filesystem::path source = normalizePath(context.workingDirectory, *pathArg);
            if (!readBytesFromFile(source, rawBytes, messages)) {
                return false;
            }
            const std::string text(rawBytes.begin(), rawBytes.end());
            nexus::parametric::ConstraintGraph graph;
            const auto result =
                nexus::parametric::ParametricGraphSerializer::deserialize(text, graph);
            if (!result.valid) {
                messages.push_back("parametric.load deserialize failed");
                return false;
            }
            context.parametricGraph    = std::move(graph);
            context.hasParametricGraph = true;
            messages.push_back("parametric graph loaded entities="
                + std::to_string(context.parametricGraph.entityCount()));
            return true;
        });
}

} // namespace nexus::automation
