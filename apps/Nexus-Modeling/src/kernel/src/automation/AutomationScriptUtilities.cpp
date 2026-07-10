#include "AutomationScriptUtilities.h"

#include <nexus/asset/SceneAsset.h>
#include <nexus/animation/AnimationCore.h>
#include <nexus/geometry/GeometryKernel.h>
#include <nexus/gfx/GaussianSplatting.h>
#include <nexus/parametric/ConstraintGraph.h>
#include <nexus/parametric/ParametricSerialization.h>
#include <nexus/sim/ClothSolver.h>
#include <nexus/sim/FluidSolver.h>
#include <nexus/sim/SimulationCore.h>

#include <algorithm>
#include <bit>
#include <charconv>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <sstream>
#include <utility>

namespace nexus::automation {

bool isFiniteFloat(float value) noexcept
{
    const std::uint32_t bits = std::bit_cast<std::uint32_t>(value);
    return (bits & 0x7F800000u) != 0x7F800000u;
}

bool isFiniteDouble(double value) noexcept
{
    const std::uint64_t bits = std::bit_cast<std::uint64_t>(value);
    return (bits & 0x7FF0000000000000ull) != 0x7FF0000000000000ull;
}

std::string formatScalar(double v)
{
    // std::to_string uses the shortest round-trip form in C++26 ("4", "4.5"),
    // which silently drops trailing decimals. Keep the short form but guarantee a
    // decimal point for whole numbers so message output stays stable ("4.0").
    std::string s = std::to_string(v);
    const bool hasDotOrExp = s.find('.') != std::string::npos || s.find('e') != std::string::npos ||
                             s.find('E') != std::string::npos;
    const bool nonFinite   = s.find('n') != std::string::npos || s.find('i') != std::string::npos; // nan/inf
    if (!hasDotOrExp && !nonFinite) {
        s += ".0";
    }
    return s;
}

std::string formatScalar(float v) { return formatScalar(static_cast<double>(v)); }

nexus::render::Mat4 makeTranslationMatrix(float x, float y, float z) noexcept
{
    nexus::render::Mat4 m = nexus::render::Mat4::identity();
    m.m[0][3] = x;
    m.m[1][3] = y;
    m.m[2][3] = z;
    return m;
}

nexus::render::Mat4 makeScaleMatrix(float x, float y, float z) noexcept
{
    nexus::render::Mat4 m = nexus::render::Mat4::identity();
    m.m[0][0] = x;
    m.m[1][1] = y;
    m.m[2][2] = z;
    return m;
}

std::string trimCopy(std::string_view text)
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

std::vector<std::string> tokenizeLine(std::string_view line)
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

bool parseKeyValueToken(const std::string& token,
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

std::optional<std::string> getArg(const ScriptCommand& command, std::string_view key)
{
    auto it = command.args.find(std::string(key));
    if (it == command.args.end()) {
        return std::nullopt;
    }
    return it->second;
}

std::optional<float> parseFloatArg(const ScriptCommand& command, std::string_view key)
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
    if (!isFiniteFloat(value)) {
        return std::nullopt;
    }
    return value;
}

std::optional<int32_t> parseIntArg(const ScriptCommand& command, std::string_view key)
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

std::optional<double> parseDoubleArg(const ScriptCommand& command, std::string_view key)
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
    if (!isFiniteDouble(value)) {
        return std::nullopt;
    }
    return value;
}

std::string makePathString(const std::filesystem::path& path)
{
    return path.lexically_normal().string();
}

bool writeBytesToFile(const std::filesystem::path& path,
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

bool readBytesFromFile(const std::filesystem::path& path,
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

uint64_t hashBytesFnv1a64(const std::vector<uint8_t>& bytes) noexcept
{
    uint64_t h = 14695981039346656037ull;
    for (const uint8_t b : bytes) {
        h ^= static_cast<uint64_t>(b);
        h *= 1099511628211ull;
    }
    return h;
}

std::string hashHex(uint64_t value)
{
    std::ostringstream oss;
    oss << "0x" << std::hex << std::uppercase << value;
    return oss.str();
}

std::optional<std::string> extractJsonStringField(std::string_view text,
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

std::optional<uint64_t> parseExpectedHashArg(const ScriptCommand& command)
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

size_t byteDiffCount(const std::vector<uint8_t>& a,
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

size_t firstByteDiff(const std::vector<uint8_t>& a,
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

std::string jsonEscape(std::string_view text)
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

std::string joinCommandsJson(const std::vector<std::string>& commands)
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

std::string serializeCommandSurfaceJson(const std::vector<std::string>& commands)
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

std::string serializeCommandSurfaceCompactJson(const std::vector<std::string>& commands)
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


std::string makeDiagnosticsReportJson(const ScriptContext& context,
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

std::string makeReplaySummaryBundleJson(const ScriptContext& context,
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

// ── Mesh / Scene / Animation / Cross-solver state serializers ─────────────────

std::vector<uint8_t> serializeMeshState(const nexus::geometry::Mesh& mesh)
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

std::vector<uint8_t> serializeGaussianState(const nexus::gfx::GaussianSplatCloud& cloud)
{
    std::vector<uint8_t> bytes;
    const auto appendUint32 = [&bytes](uint32_t v) {
        const uint8_t* b = reinterpret_cast<const uint8_t*>(&v);
        bytes.insert(bytes.end(), b, b + 4);
    };
    const auto appendFloat = [&bytes](float f) {
        const uint8_t* b = reinterpret_cast<const uint8_t*>(&f);
        bytes.insert(bytes.end(), b, b + 4);
    };
    const auto appendStr = [&bytes](const std::string& s) {
        bytes.insert(bytes.end(), s.begin(), s.end());
        bytes.push_back('\n');
    };

    appendStr(cloud.sourceFormat);
    appendUint32(cloud.shDegree);
    appendUint32(static_cast<uint32_t>(cloud.splats.size()));
    for (const auto& s : cloud.splats) {
        appendFloat(s.position.x); appendFloat(s.position.y); appendFloat(s.position.z);
        appendFloat(s.scale.x); appendFloat(s.scale.y); appendFloat(s.scale.z);
        appendFloat(s.rotation.x); appendFloat(s.rotation.y); appendFloat(s.rotation.z); appendFloat(s.rotation.w);
        appendFloat(s.opacity);
        for (float c : s.dc) {
            appendFloat(c);
        }
        for (float c : s.shRest) {
            appendFloat(c);
        }
    }
    return bytes;
}

std::vector<uint8_t> serializeSceneState(const nexus::asset::SceneAsset& scene)
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

std::vector<uint8_t> serializeParametricState(
    const nexus::parametric::ConstraintGraph& graph)
{
    const std::string text = nexus::parametric::ParametricGraphSerializer::serialize(graph);
    return std::vector<uint8_t>(text.begin(), text.end());
}

std::vector<uint8_t> serializeAnimationState(
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

std::vector<uint8_t> serializeCrossSolverState(const ScriptContext& context)
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

std::string makeCrossSolverBundleJson(const ScriptContext& context)
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

} // namespace nexus::automation
