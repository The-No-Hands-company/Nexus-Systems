// ─────────────────────────────────────────────────────────────────────────────
//  Nexus GFX — 3D Gaussian Splatting implementation
// ─────────────────────────────────────────────────────────────────────────────
#include <nexus/gfx/GaussianSplatting.h>

#include <algorithm>
#include <charconv>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <unordered_map>

namespace nexus::gfx {

// ─────────────────────────────────────────────────────────────────────────────
//  Internal PLY parsing helpers
// ─────────────────────────────────────────────────────────────────────────────
namespace {

struct PlyProperty {
    std::string name;
    uint32_t    byteOffset = 0;
    uint32_t    byteSize   = 4;   // only float32 is currently expected
};

struct PlyHeader {
    uint32_t                  vertexCount = 0;
    bool                      binaryLittleEndian = false;
    std::vector<PlyProperty>  properties;
    uint32_t                  headerEndOffset = 0;  // byte offset to first data byte
    uint32_t                  strideBytes     = 0;
};

[[nodiscard]] std::optional<PlyHeader> parsePLYHeader(const std::vector<uint8_t>& bytes)
{
    // Must start with "ply\n"
    if (bytes.size() < 4) return std::nullopt;
    if (bytes[0] != 'p' || bytes[1] != 'l' || bytes[2] != 'y') return std::nullopt;

    PlyHeader hdr;
    size_t    pos = 0;

    // Helper: read one text line from pos
    auto readLine = [&]() -> std::string {
        std::string line;
        while (pos < bytes.size()) {
            char c = static_cast<char>(bytes[pos++]);
            if (c == '\n') break;
            if (c != '\r') line += c;
        }
        return line;
    };

    bool inVertexElement = false;

    while (pos < bytes.size()) {
        std::string line = readLine();

        if (line == "end_header") {
            hdr.headerEndOffset = static_cast<uint32_t>(pos);
            break;
        }

        // format line
        if (line.rfind("format", 0) == 0) {
            if (line.find("binary_little_endian") != std::string::npos)
                hdr.binaryLittleEndian = true;
            continue;
        }

        // element vertex N
        if (line.rfind("element vertex", 0) == 0) {
            std::istringstream ss(line);
            std::string tok;
            ss >> tok >> tok;  // "element" "vertex"
            ss >> hdr.vertexCount;
            inVertexElement = true;
            continue;
        }

        // element <other> — stop property accumulation for vertex
        if (line.rfind("element", 0) == 0 && line.find("vertex") == std::string::npos) {
            inVertexElement = false;
            continue;
        }

        // property float <name>
        if (inVertexElement && line.rfind("property float", 0) == 0) {
            std::istringstream ss(line);
            std::string tok;
            ss >> tok >> tok >> tok;  // "property" "float" <name>
            PlyProperty p;
            p.name       = tok;
            p.byteOffset = hdr.strideBytes;
            p.byteSize   = 4;
            hdr.properties.push_back(p);
            hdr.strideBytes += 4;
            continue;
        }
    }

    if (hdr.vertexCount == 0 || hdr.strideBytes == 0 || hdr.headerEndOffset == 0)
        return std::nullopt;

    return hdr;
}

// Build a name→index map for the parsed properties
[[nodiscard]] std::unordered_map<std::string, uint32_t>
buildPropIndex(const std::vector<PlyProperty>& props)
{
    std::unordered_map<std::string, uint32_t> idx;
    for (uint32_t i = 0; i < static_cast<uint32_t>(props.size()); ++i)
        idx[props[i].name] = i;
    return idx;
}

[[nodiscard]] float readF32LE(const uint8_t* ptr)
{
    float v;
    std::memcpy(&v, ptr, 4);
    return v;
}

[[nodiscard]] std::optional<GaussianSplatCloud>
parseFromHeader(const PlyHeader& hdr,
                const std::vector<uint8_t>& bytes,
                std::string fmt)
{
    // Require at minimum x, y, z
    auto propIdx = buildPropIndex(hdr.properties);
    if (propIdx.find("x") == propIdx.end() ||
        propIdx.find("y") == propIdx.end() ||
        propIdx.find("z") == propIdx.end())
        return std::nullopt;

    uint64_t dataSize = static_cast<uint64_t>(hdr.vertexCount) * hdr.strideBytes;
    if (hdr.headerEndOffset + dataSize > bytes.size())
        return std::nullopt;

    GaussianSplatCloud cloud;
    cloud.sourceFormat = std::move(fmt);
    cloud.splats.reserve(hdr.vertexCount);

    const uint8_t* base = bytes.data() + hdr.headerEndOffset;

    auto getF32 = [&](uint32_t vi, const std::string& name, float defaultVal = 0.f) -> float {
        auto it = propIdx.find(name);
        if (it == propIdx.end()) return defaultVal;
        const uint8_t* p = base + static_cast<uint64_t>(vi) * hdr.strideBytes
                                + hdr.properties[it->second].byteOffset;
        return readF32LE(p);
    };

    for (uint32_t i = 0; i < hdr.vertexCount; ++i) {
        GaussianSplat splat{};
        splat.position = { getF32(i, "x"), getF32(i, "y"), getF32(i, "z") };
        splat.scale    = { getF32(i, "scale_0"), getF32(i, "scale_1"), getF32(i, "scale_2") };
        splat.rotation = { getF32(i, "rot_1"),   getF32(i, "rot_2"),
                           getF32(i, "rot_3"),   getF32(i, "rot_0", 1.f) };  // wxyz→xyzw
        splat.opacity  = getF32(i, "opacity");
        splat.dc[0]    = getF32(i, "f_dc_0");
        splat.dc[1]    = getF32(i, "f_dc_1");
        splat.dc[2]    = getF32(i, "f_dc_2");
        for (int r = 0; r < 45; ++r) {
            std::string rname = "f_rest_" + std::to_string(r);
            splat.shRest[static_cast<size_t>(r)] = getF32(i, rname);
        }
        cloud.splats.push_back(splat);
    }

    return cloud;
}

} // anonymous namespace

// ─────────────────────────────────────────────────────────────────────────────
//  GaussianSplatCloud — public API
// ─────────────────────────────────────────────────────────────────────────────
std::optional<GaussianSplatCloud>
GaussianSplatCloud::loadFromPLY(const std::string& path)
{
    std::ifstream f(path, std::ios::binary);
    if (!f) return std::nullopt;
    f.seekg(0, std::ios::end);
    auto sz = f.tellg();
    f.seekg(0, std::ios::beg);
    if (sz <= 0) return std::nullopt;
    std::vector<uint8_t> bytes(static_cast<size_t>(sz));
    f.read(reinterpret_cast<char*>(bytes.data()), sz);
    if (!f) return std::nullopt;
    return loadFromPLYBytes(bytes);
}

std::optional<GaussianSplatCloud>
GaussianSplatCloud::loadFromPLYBytes(const std::vector<uint8_t>& bytes)
{
    auto hdr = parsePLYHeader(bytes);
    if (!hdr) return std::nullopt;
    return parseFromHeader(*hdr, bytes, "ply");
}

bool GaussianSplatCloud::saveToPLY(const std::string& path) const
{
    std::ofstream f(path, std::ios::binary);
    if (!f) return false;

    // ASCII header
    std::ostringstream hdr;
    hdr << "ply\n"
        << "format binary_little_endian 1.0\n"
        << "element vertex " << splats.size() << "\n"
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

    std::string hdrStr = hdr.str();
    f.write(hdrStr.data(), static_cast<std::streamsize>(hdrStr.size()));

    for (const auto& s : splats) {
        // rot stored as wxyz in PLY (rot_0=w)
        float data[14] = {
            s.position.x, s.position.y, s.position.z,
            s.dc[0], s.dc[1], s.dc[2],
            s.opacity,
            s.scale.x, s.scale.y, s.scale.z,
            s.rotation.w,  // rot_0 = w
            s.rotation.x,  // rot_1 = x
            s.rotation.y,  // rot_2 = y
            s.rotation.z   // rot_3 = z
        };
        f.write(reinterpret_cast<const char*>(data), sizeof(data));
    }

    return f.good();
}

} // namespace nexus::gfx
