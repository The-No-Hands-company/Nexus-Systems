#include <nexus/geometry/MeshIO.h>

#include <algorithm>
#include <bit>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <limits>
#include <map>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace nexus::geometry {

namespace {

bool isFiniteFloat(float value) noexcept
{
    constexpr std::uint32_t kExpMask = 0x7F800000u;
    const std::uint32_t bits = std::bit_cast<std::uint32_t>(value);
    return (bits & kExpMask) != kExpMask;
}

bool meshAttributesAreFinite(const Mesh& mesh,
                             const MeshExportOptions& options,
                             MeshExportReport& report)
{
    for (const auto& position : mesh.attributes().positions()) {
        if (!isFiniteFloat(position.x) || !isFiniteFloat(position.y) || !isFiniteFloat(position.z)) {
            report.diagnostic = MeshExportDiagnostic::InvalidMesh;
            report.messages.push_back("Cannot export: mesh positions contain non-finite values");
            std::sort(report.messages.begin(), report.messages.end());
            return false;
        }
    }

    if (options.includeNormals && mesh.attributes().hasNormals()) {
        for (const auto& normal : mesh.attributes().normals()) {
            if (!isFiniteFloat(normal.x) || !isFiniteFloat(normal.y) || !isFiniteFloat(normal.z)) {
                report.diagnostic = MeshExportDiagnostic::InvalidMesh;
                report.messages.push_back("Cannot export: mesh normals contain non-finite values");
                std::sort(report.messages.begin(), report.messages.end());
                return false;
            }
        }
    }

    if (options.includeUVs && mesh.attributes().hasUVs()) {
        for (const auto& uv : mesh.attributes().uvs()) {
            if (!isFiniteFloat(uv.u) || !isFiniteFloat(uv.v)) {
                report.diagnostic = MeshExportDiagnostic::InvalidMesh;
                report.messages.push_back("Cannot export: mesh UVs contain non-finite values");
                std::sort(report.messages.begin(), report.messages.end());
                return false;
            }
        }
    }

    return true;
}


// ── OBJ ───────────────────────────────────────────────────────────────────────
MeshExportReport exportOBJ(const Mesh&              mesh,
                           const std::string&       path,
                           const MeshExportOptions& opts)
{
    MeshExportReport report{};

    std::ofstream out(path);
    if (!out.is_open()) {
        report.diagnostic = MeshExportDiagnostic::FileOpenFailed;
        report.messages.push_back("Failed to open file for writing: " + path);
        std::sort(report.messages.begin(), report.messages.end());
        return report;
    }
    out.precision(9);

    const auto& positions = mesh.attributes().positions();
    const bool  hasNormals = opts.includeNormals && mesh.attributes().hasNormals();
    const bool  hasUVs     = opts.includeUVs     && mesh.attributes().hasUVs();
    const auto& normals    = hasNormals ? mesh.attributes().normals()  : decltype(mesh.attributes().normals()){};
    const auto& uvs        = hasUVs     ? mesh.attributes().uvs()      : decltype(mesh.attributes().uvs()){};

    auto check = [&]() -> bool {
        if (out.good()) return true;
        report.diagnostic = MeshExportDiagnostic::WriteError;
        report.messages.push_back("Write error while exporting OBJ: " + path);
        std::sort(report.messages.begin(), report.messages.end());
        return false;
    };

    out << "# Nexus OBJ export\n";
    if (!check()) return report;

    for (const auto& p : positions) {
        out << "v " << p.x << ' ' << p.y << ' ' << p.z << '\n';
        if (!check()) return report;
    }

    if (hasNormals) {
        for (const auto& n : normals) {
            out << "vn " << n.x << ' ' << n.y << ' ' << n.z << '\n';
            if (!check()) return report;
        }
    }

    if (hasUVs) {
        for (const auto& uv : uvs) {
            out << "vt " << uv.u << ' ' << uv.v << '\n';
            if (!check()) return report;
        }
    }

    const size_t faceCount = mesh.topology().faceCount();
    for (size_t fi = 0; fi < faceCount; ++fi) {
        const Face& f = mesh.topology().face(fi);
        if (f.indices.empty()) continue;

        out << 'f';
        for (const uint32_t idx : f.indices) {
            const uint32_t vi = idx + 1u;
            if (hasNormals && hasUVs) {
                out << ' ' << vi << '/' << vi << '/' << vi;
            } else if (hasNormals) {
                out << ' ' << vi << "//" << vi;
            } else if (hasUVs) {
                out << ' ' << vi << '/' << vi;
            } else {
                out << ' ' << vi;
            }
        }
        out << '\n';
        if (!check()) return report;
        ++report.facesWritten;
    }

    report.verticesWritten = static_cast<uint32_t>(positions.size());
    report.valid           = true;
    std::sort(report.messages.begin(), report.messages.end());
    return report;
}

// ── PLY (ASCII) ───────────────────────────────────────────────────────────────
MeshExportReport exportPLY(const Mesh&              mesh,
                           const std::string&       path,
                           const MeshExportOptions& opts)
{
    MeshExportReport report{};

    std::ofstream out(path);
    if (!out.is_open()) {
        report.diagnostic = MeshExportDiagnostic::FileOpenFailed;
        report.messages.push_back("Failed to open file for writing: " + path);
        std::sort(report.messages.begin(), report.messages.end());
        return report;
    }
    out.precision(9);

    const auto& positions  = mesh.attributes().positions();
    const bool  hasNormals = opts.includeNormals && mesh.attributes().hasNormals();
    const bool  hasUVs     = opts.includeUVs     && mesh.attributes().hasUVs();
    const auto& normals    = hasNormals ? mesh.attributes().normals() : decltype(mesh.attributes().normals()){};
    const auto& uvs        = hasUVs     ? mesh.attributes().uvs()     : decltype(mesh.attributes().uvs()){};

    const uint32_t vertexCount = static_cast<uint32_t>(positions.size());
    const uint32_t faceCount   = static_cast<uint32_t>(mesh.topology().faceCount());

    auto check = [&]() -> bool {
        if (out.good()) return true;
        report.diagnostic = MeshExportDiagnostic::WriteError;
        report.messages.push_back("Write error while exporting PLY: " + path);
        std::sort(report.messages.begin(), report.messages.end());
        return false;
    };

    out << "ply\n"
           "format ascii 1.0\n"
           "comment Nexus PLY export\n"
           "element vertex " << vertexCount << "\n"
           "property float x\n"
           "property float y\n"
           "property float z\n";
    if (!check()) return report;

    if (hasNormals) {
        out << "property float nx\n"
               "property float ny\n"
               "property float nz\n";
        if (!check()) return report;
    }
    if (hasUVs) {
        out << "property float s\n"
               "property float t\n";
        if (!check()) return report;
    }

    out << "element face " << faceCount << "\n"
           "property list uchar int vertex_indices\n"
           "end_header\n";
    if (!check()) return report;

    for (uint32_t vi = 0; vi < vertexCount; ++vi) {
        out << positions[vi].x << ' ' << positions[vi].y << ' ' << positions[vi].z;
        if (hasNormals) {
            out << ' ' << normals[vi].x << ' ' << normals[vi].y << ' ' << normals[vi].z;
        }
        if (hasUVs) {
            out << ' ' << uvs[vi].u << ' ' << uvs[vi].v;
        }
        out << '\n';
        if (!check()) return report;
    }

    for (size_t fi = 0; fi < faceCount; ++fi) {
        const Face& f = mesh.topology().face(fi);
        if (f.indices.empty()) continue;

        out << f.indices.size();
        for (const uint32_t idx : f.indices) {
            out << ' ' << idx;
        }
        out << '\n';
        if (!check()) return report;
        ++report.facesWritten;
    }

    report.verticesWritten = vertexCount;
    report.valid           = true;
    std::sort(report.messages.begin(), report.messages.end());
    return report;
}

// ── STL (binary) export ─────────────────────────────────────────────────────
using Vec3 = nexus::render::Vec3;

void writeU16LE(std::ostream& o, uint16_t v)
{
    const char b[2] = {static_cast<char>(v & 0xFF), static_cast<char>((v >> 8) & 0xFF)};
    o.write(b, 2);
}
void writeU32LE(std::ostream& o, uint32_t v)
{
    const char b[4] = {static_cast<char>(v & 0xFF), static_cast<char>((v >> 8) & 0xFF),
                       static_cast<char>((v >> 16) & 0xFF), static_cast<char>((v >> 24) & 0xFF)};
    o.write(b, 4);
}
void writeF32LE(std::ostream& o, float f) { writeU32LE(o, std::bit_cast<uint32_t>(f)); }

Vec3 facetNormal(const Vec3& a, const Vec3& b, const Vec3& c)
{
    const float ux = b.x - a.x, uy = b.y - a.y, uz = b.z - a.z;
    const float vx = c.x - a.x, vy = c.y - a.y, vz = c.z - a.z;
    float nx = uy * vz - uz * vy;
    float ny = uz * vx - ux * vz;
    float nz = ux * vy - uy * vx;
    const float len2 = nx * nx + ny * ny + nz * nz;
    if (len2 > 0.f) {
        const float inv = 1.f / std::sqrt(len2);
        nx *= inv;
        ny *= inv;
        nz *= inv;
    }
    Vec3 r;
    r.x = nx;
    r.y = ny;
    r.z = nz;
    return r;
}

MeshExportReport exportSTL(const Mesh& mesh, const std::string& path, const MeshExportOptions& /*options*/)
{
    MeshExportReport report{};

    // STL stores triangles only; triangulate a working copy (input is const).
    Mesh tri = mesh;
    (void)tri.topology().triangulate();
    const auto& positions = tri.attributes().positions();
    const auto& topo      = tri.topology();

    std::ofstream out(path, std::ios::binary);
    if (!out) {
        report.diagnostic = MeshExportDiagnostic::FileOpenFailed;
        report.messages.push_back("Cannot export: failed to open file for writing");
        std::sort(report.messages.begin(), report.messages.end());
        return report;
    }

    char header[80];
    std::memset(header, 0, sizeof(header));
    std::memcpy(header, "Nexus binary STL", 16);
    out.write(header, sizeof(header));

    uint32_t triangleCount = 0;
    for (size_t f = 0; f < topo.faceCount(); ++f) {
        if (topo.face(f).indices.size() == 3) {
            ++triangleCount;
        }
    }
    writeU32LE(out, triangleCount);

    for (size_t f = 0; f < topo.faceCount(); ++f) {
        const auto& face = topo.face(f);
        if (face.indices.size() != 3) {
            continue;
        }
        const Vec3& a = positions[face.indices[0]];
        const Vec3& b = positions[face.indices[1]];
        const Vec3& c = positions[face.indices[2]];
        const Vec3  n = facetNormal(a, b, c);
        writeF32LE(out, n.x);
        writeF32LE(out, n.y);
        writeF32LE(out, n.z);
        for (const Vec3& p : {a, b, c}) {
            writeF32LE(out, p.x);
            writeF32LE(out, p.y);
            writeF32LE(out, p.z);
        }
        writeU16LE(out, 0);  // attribute byte count
    }

    if (!out) {
        report.diagnostic = MeshExportDiagnostic::WriteError;
        report.messages.push_back("Write error while emitting STL data");
        std::sort(report.messages.begin(), report.messages.end());
        return report;
    }

    report.facesWritten    = triangleCount;
    report.verticesWritten  = triangleCount * 3;
    report.valid           = true;
    std::sort(report.messages.begin(), report.messages.end());
    return report;
}

// ── Import helpers ──────────────────────────────────────────────────────────
uint32_t readU32LE(const unsigned char* p)
{
    return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8)
         | (static_cast<uint32_t>(p[2]) << 16) | (static_cast<uint32_t>(p[3]) << 24);
}
float readF32LE(const unsigned char* p) { return std::bit_cast<float>(readU32LE(p)); }

// Parse an OBJ face-vertex reference: "v", "v/vt", "v//vn", or "v/vt/vn".
// Component indices are OBJ-style (1-based, negatives allowed); 0 means "absent".
void parseOBJVertexRef(const std::string& tok, long& vi, long& ti, long& ni)
{
    vi = ti = ni = 0;
    const size_t p1 = tok.find('/');
    auto toLong = [](const std::string& s) -> long {
        return s.empty() ? 0L : std::strtol(s.c_str(), nullptr, 10);
    };
    if (p1 == std::string::npos) {
        vi = toLong(tok);
        return;
    }
    vi = toLong(tok.substr(0, p1));
    const size_t p2 = tok.find('/', p1 + 1);
    if (p2 == std::string::npos) {
        ti = toLong(tok.substr(p1 + 1));
        return;
    }
    if (p2 > p1 + 1) {
        ti = toLong(tok.substr(p1 + 1, p2 - p1 - 1));
    }
    ni = toLong(tok.substr(p2 + 1));
}

// Resolve an OBJ 1-based / negative index to a 0-based array index; -1 on out-of-range.
long resolveOBJIndex(long idx, size_t count)
{
    if (idx > 0 && static_cast<size_t>(idx) <= count) {
        return idx - 1;
    }
    if (idx < 0 && static_cast<size_t>(-idx) <= count) {
        return static_cast<long>(count) + idx;
    }
    return -1;
}

MeshImportReport importOBJ(const std::string& path, const MeshImportOptions& options, Mesh& out)
{
    MeshImportReport report{};
    std::ifstream in(path);
    if (!in) {
        report.diagnostic = MeshImportDiagnostic::FileOpenFailed;
        report.messages.push_back("Cannot import: failed to open file");
        return report;
    }

    std::vector<Vec2> vt;
    std::vector<Vec3> v, vn;
    std::vector<Vec3> outPos, outNrm;
    std::vector<Vec2> outUV;
    std::vector<Face> faces;
    std::unordered_map<std::string, uint32_t> cornerMap;
    bool anyVT = false, anyVN = false;

    std::string line;
    while (std::getline(in, line)) {
        std::istringstream ls(line);
        std::string tag;
        if (!(ls >> tag)) {
            continue;
        }
        if (tag == "v") {
            float x, y, z;
            if (!(ls >> x >> y >> z)) {
                report.diagnostic = MeshImportDiagnostic::ParseError;
                report.messages.push_back("Malformed 'v' (position) record");
                return report;
            }
            if (!isFiniteFloat(x) || !isFiniteFloat(y) || !isFiniteFloat(z)) {
                report.diagnostic = MeshImportDiagnostic::NonFiniteData;
                report.messages.push_back("Non-finite value in vertex position");
                return report;
            }
            Vec3 p;
            p.x = x;
            p.y = y;
            p.z = z;
            v.push_back(p);
        } else if (tag == "vt") {
            float u = 0.f, w = 0.f;
            if (!(ls >> u)) {
                report.diagnostic = MeshImportDiagnostic::ParseError;
                report.messages.push_back("Malformed 'vt' (texcoord) record");
                return report;
            }
            ls >> w;  // optional second coordinate
            if (!isFiniteFloat(u) || !isFiniteFloat(w)) {
                report.diagnostic = MeshImportDiagnostic::NonFiniteData;
                report.messages.push_back("Non-finite value in texture coordinate");
                return report;
            }
            vt.push_back(Vec2{u, w});
            anyVT = true;
        } else if (tag == "vn") {
            float x, y, z;
            if (!(ls >> x >> y >> z)) {
                report.diagnostic = MeshImportDiagnostic::ParseError;
                report.messages.push_back("Malformed 'vn' (normal) record");
                return report;
            }
            if (!isFiniteFloat(x) || !isFiniteFloat(y) || !isFiniteFloat(z)) {
                report.diagnostic = MeshImportDiagnostic::NonFiniteData;
                report.messages.push_back("Non-finite value in vertex normal");
                return report;
            }
            Vec3 n;
            n.x = x;
            n.y = y;
            n.z = z;
            vn.push_back(n);
        } else if (tag == "f") {
            std::vector<uint32_t> faceIdx;
            std::string ref;
            while (ls >> ref) {
                long vi, ti, ni;
                parseOBJVertexRef(ref, vi, ti, ni);
                const long pv = resolveOBJIndex(vi, v.size());
                if (pv < 0) {
                    report.diagnostic = MeshImportDiagnostic::ParseError;
                    report.messages.push_back("Face references out-of-range vertex index");
                    return report;
                }
                long pt = ti != 0 ? resolveOBJIndex(ti, vt.size()) : -1;
                long pn = ni != 0 ? resolveOBJIndex(ni, vn.size()) : -1;

                auto it = cornerMap.find(ref);
                uint32_t outIdx;
                if (it == cornerMap.end()) {
                    outIdx = static_cast<uint32_t>(outPos.size());
                    outPos.push_back(v[static_cast<size_t>(pv)]);
                    if (anyVT) {
                        outUV.push_back(pt >= 0 ? vt[static_cast<size_t>(pt)] : Vec2{0.f, 0.f});
                    }
                    if (anyVN) {
                        outNrm.push_back(pn >= 0 ? vn[static_cast<size_t>(pn)] : Vec3{});
                    }
                    cornerMap.emplace(ref, outIdx);
                } else {
                    outIdx = it->second;
                }
                faceIdx.push_back(outIdx);
            }
            if (faceIdx.size() >= 3) {
                Face face;
                face.indices = std::move(faceIdx);
                faces.push_back(std::move(face));
            }
        }
        // all other records (o, g, s, mtllib, usemtl, #, ...) are ignored
    }

    if (outPos.empty()) {
        report.diagnostic = MeshImportDiagnostic::EmptyMesh;
        report.messages.push_back("No geometry found in OBJ file");
        return report;
    }

    out.attributes().setPositions(std::move(outPos));
    for (Face& f : faces) {
        out.topology().addFace(std::move(f));
    }
    if (anyVT && !outUV.empty()) {
        out.attributes().setUVs(std::move(outUV));
    }
    if (anyVN && !outNrm.empty()) {
        out.attributes().setNormals(std::move(outNrm));
    } else if (options.computeNormalsIfMissing) {
        (void)out.computeVertexNormals();
    }

    report.verticesRead = static_cast<uint32_t>(out.attributes().vertexCount());
    report.facesRead    = static_cast<uint32_t>(out.topology().faceCount());
    report.valid        = true;
    return report;
}

MeshImportReport importSTL(const std::string& path, const MeshImportOptions& options, Mesh& out)
{
    MeshImportReport report{};
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        report.diagnostic = MeshImportDiagnostic::FileOpenFailed;
        report.messages.push_back("Cannot import: failed to open file");
        return report;
    }
    const std::vector<char> buf((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    const size_t sz = buf.size();

    std::vector<Vec3> positions, normals;
    std::vector<Face> faces;

    // Binary detection is size-authoritative: header(80) + count(4) + 50*count.
    bool binary = false;
    if (sz >= 84) {
        const uint32_t tc = readU32LE(reinterpret_cast<const unsigned char*>(buf.data()) + 80);
        if (sz == 84 + static_cast<size_t>(tc) * 50) {
            binary = true;
        }
    }

    auto pushTri = [&](const Vec3& a, const Vec3& b, const Vec3& c, const Vec3& n) {
        const uint32_t base = static_cast<uint32_t>(positions.size());
        positions.push_back(a);
        positions.push_back(b);
        positions.push_back(c);
        normals.push_back(n);
        normals.push_back(n);
        normals.push_back(n);
        Face f;
        f.indices = {base, base + 1, base + 2};
        faces.push_back(std::move(f));
    };

    if (binary) {
        const auto* p = reinterpret_cast<const unsigned char*>(buf.data());
        const uint32_t tc = readU32LE(p + 80);
        const unsigned char* q = p + 84;
        for (uint32_t t = 0; t < tc; ++t, q += 50) {
            float f12[12];
            for (int i = 0; i < 12; ++i) {
                f12[i] = readF32LE(q + i * 4);
                if (!isFiniteFloat(f12[i])) {
                    report.diagnostic = MeshImportDiagnostic::NonFiniteData;
                    report.messages.push_back("Non-finite value in binary STL triangle");
                    return report;
                }
            }
            Vec3 n, a, b, c;
            n = Vec3{f12[0], f12[1], f12[2]};
            a = Vec3{f12[3], f12[4], f12[5]};
            b = Vec3{f12[6], f12[7], f12[8]};
            c = Vec3{f12[9], f12[10], f12[11]};
            pushTri(a, b, c, n);
        }
    } else {
        std::istringstream ss(std::string(buf.begin(), buf.end()));
        std::string w;
        Vec3 curN{};
        std::vector<Vec3> fv;
        while (ss >> w) {
            if (w == "facet") {
                std::string kw;
                ss >> kw;  // "normal"
                float nx = 0.f, ny = 0.f, nz = 0.f;
                if (ss >> nx >> ny >> nz) {
                    curN = Vec3{nx, ny, nz};
                }
                fv.clear();
            } else if (w == "vertex") {
                float x, y, z;
                if (!(ss >> x >> y >> z)) {
                    report.diagnostic = MeshImportDiagnostic::ParseError;
                    report.messages.push_back("Malformed 'vertex' record in ASCII STL");
                    return report;
                }
                if (!isFiniteFloat(x) || !isFiniteFloat(y) || !isFiniteFloat(z)) {
                    report.diagnostic = MeshImportDiagnostic::NonFiniteData;
                    report.messages.push_back("Non-finite value in ASCII STL vertex");
                    return report;
                }
                fv.push_back(Vec3{x, y, z});
            } else if (w == "endfacet") {
                if (fv.size() >= 3) {
                    pushTri(fv[0], fv[1], fv[2], curN);
                }
                fv.clear();
            }
        }
    }

    if (positions.empty()) {
        report.diagnostic = MeshImportDiagnostic::EmptyMesh;
        report.messages.push_back("No triangles found in STL file");
        return report;
    }

    out.attributes().setPositions(std::move(positions));
    for (Face& f : faces) {
        out.topology().addFace(std::move(f));
    }
    if (options.weldVertices) {
        (void)out.weldCoincidentVertices(options.weldEpsilon);
        // Welding invalidates the per-corner facet normals; recompute smooth normals.
        if (options.computeNormalsIfMissing) {
            (void)out.computeVertexNormals();
        }
    } else {
        out.attributes().setNormals(std::move(normals));
    }

    report.verticesRead = static_cast<uint32_t>(out.attributes().vertexCount());
    report.facesRead    = static_cast<uint32_t>(out.topology().faceCount());
    report.valid        = true;
    return report;
}

// ── PLY (ASCII + binary little-endian) import ────────────────────────────────
enum class PlyType : uint8_t { I8, U8, I16, U16, I32, U32, F32, F64, Invalid };

PlyType plyTypeFromName(const std::string& s)
{
    if (s == "char"   || s == "int8")    return PlyType::I8;
    if (s == "uchar"  || s == "uint8")   return PlyType::U8;
    if (s == "short"  || s == "int16")   return PlyType::I16;
    if (s == "ushort" || s == "uint16")  return PlyType::U16;
    if (s == "int"    || s == "int32")   return PlyType::I32;
    if (s == "uint"   || s == "uint32")  return PlyType::U32;
    if (s == "float"  || s == "float32") return PlyType::F32;
    if (s == "double" || s == "float64") return PlyType::F64;
    return PlyType::Invalid;
}

size_t plyTypeSize(PlyType t)
{
    switch (t) {
        case PlyType::I8:
        case PlyType::U8:  return 1;
        case PlyType::I16:
        case PlyType::U16: return 2;
        case PlyType::I32:
        case PlyType::U32:
        case PlyType::F32: return 4;
        case PlyType::F64: return 8;
        default:           return 0;
    }
}

uint64_t readLEBytes(const unsigned char* p, size_t n)
{
    uint64_t v = 0;
    for (size_t i = 0; i < n; ++i) {
        v |= static_cast<uint64_t>(p[i]) << (8 * i);
    }
    return v;
}

double plyBinValue(const unsigned char* p, PlyType t)
{
    switch (t) {
        case PlyType::I8:  return static_cast<double>(static_cast<int8_t>(p[0]));
        case PlyType::U8:  return static_cast<double>(p[0]);
        case PlyType::I16: return static_cast<double>(static_cast<int16_t>(readLEBytes(p, 2)));
        case PlyType::U16: return static_cast<double>(static_cast<uint16_t>(readLEBytes(p, 2)));
        case PlyType::I32: return static_cast<double>(static_cast<int32_t>(readLEBytes(p, 4)));
        case PlyType::U32: return static_cast<double>(static_cast<uint32_t>(readLEBytes(p, 4)));
        case PlyType::F32: return static_cast<double>(std::bit_cast<float>(static_cast<uint32_t>(readLEBytes(p, 4))));
        case PlyType::F64: return std::bit_cast<double>(readLEBytes(p, 8));
        default:           return 0.0;
    }
}

struct PlyProp {
    std::string name;
    PlyType     type = PlyType::Invalid;
};

MeshImportReport importPLY(const std::string& path, const MeshImportOptions& options, Mesh& out)
{
    MeshImportReport report{};
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        report.diagnostic = MeshImportDiagnostic::FileOpenFailed;
        report.messages.push_back("Cannot import: failed to open file");
        return report;
    }
    const std::vector<char> buf((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    const std::string content(buf.begin(), buf.end());

    const size_t hpos = content.find("end_header");
    if (hpos == std::string::npos) {
        report.diagnostic = MeshImportDiagnostic::ParseError;
        report.messages.push_back("PLY: missing end_header");
        return report;
    }
    const size_t nl = content.find('\n', hpos);
    const size_t bodyStart = (nl == std::string::npos) ? content.size() : nl + 1;

    enum class Fmt { Ascii, BinaryLE, BinaryBE, Unknown } fmt = Fmt::Unknown;
    std::vector<PlyProp> vprops;
    PlyType faceCountType = PlyType::Invalid, faceIndexType = PlyType::Invalid;
    uint64_t vertexCount = 0, faceCount = 0;
    std::string curElem;

    std::istringstream hs(content.substr(0, hpos));
    std::string line;
    while (std::getline(hs, line)) {
        std::istringstream ls(line);
        std::string tok;
        if (!(ls >> tok)) {
            continue;
        }
        if (tok == "format") {
            std::string f;
            ls >> f;
            if (f == "ascii") fmt = Fmt::Ascii;
            else if (f == "binary_little_endian") fmt = Fmt::BinaryLE;
            else if (f == "binary_big_endian") fmt = Fmt::BinaryBE;
        } else if (tok == "element") {
            std::string e;
            uint64_t n = 0;
            ls >> e >> n;
            curElem = e;
            if (e == "vertex") vertexCount = n;
            else if (e == "face") faceCount = n;
        } else if (tok == "property") {
            std::string t1;
            ls >> t1;
            if (t1 == "list") {
                std::string ct, it, nm;
                ls >> ct >> it >> nm;
                if (curElem == "face") {
                    faceCountType = plyTypeFromName(ct);
                    faceIndexType = plyTypeFromName(it);
                }
            } else if (curElem == "vertex") {
                std::string nm;
                ls >> nm;
                vprops.push_back(PlyProp{nm, plyTypeFromName(t1)});
            }
        }
    }

    if (fmt == Fmt::Unknown) {
        report.diagnostic = MeshImportDiagnostic::ParseError;
        report.messages.push_back("PLY: unknown or missing format");
        return report;
    }
    if (fmt == Fmt::BinaryBE) {
        report.diagnostic = MeshImportDiagnostic::UnsupportedFormat;
        report.messages.push_back("PLY: binary_big_endian is not supported");
        return report;
    }

    auto idxOf = [&](const char* nm) -> int {
        for (size_t i = 0; i < vprops.size(); ++i) {
            if (vprops[i].name == nm) return static_cast<int>(i);
        }
        return -1;
    };
    const int ix = idxOf("x"), iy = idxOf("y"), iz = idxOf("z");
    if (ix < 0 || iy < 0 || iz < 0) {
        report.diagnostic = MeshImportDiagnostic::ParseError;
        report.messages.push_back("PLY: vertex element lacks x/y/z");
        return report;
    }
    const int inx = idxOf("nx"), iny = idxOf("ny"), inz = idxOf("nz");
    int is = idxOf("s"); if (is < 0) is = idxOf("u"); if (is < 0) is = idxOf("texture_u");
    int it = idxOf("t"); if (it < 0) it = idxOf("v"); if (it < 0) it = idxOf("texture_v");
    const bool hasN  = inx >= 0 && iny >= 0 && inz >= 0;
    const bool hasUV = is >= 0 && it >= 0;

    std::vector<Vec3> positions, normals;
    std::vector<Vec2> uvs;
    std::vector<Face> faces;

    auto finiteOK = [&](float a, float b, float c) {
        return isFiniteFloat(a) && isFiniteFloat(b) && isFiniteFloat(c);
    };

    if (fmt == Fmt::Ascii) {
        std::istringstream bs(content.substr(bodyStart));
        std::vector<double> vals(vprops.size());
        for (uint64_t v = 0; v < vertexCount; ++v) {
            for (size_t k = 0; k < vprops.size(); ++k) {
                if (!(bs >> vals[k])) {
                    report.diagnostic = MeshImportDiagnostic::ParseError;
                    report.messages.push_back("PLY: truncated vertex data");
                    return report;
                }
            }
            const auto px = static_cast<float>(vals[static_cast<size_t>(ix)]);
            const auto py = static_cast<float>(vals[static_cast<size_t>(iy)]);
            const auto pz = static_cast<float>(vals[static_cast<size_t>(iz)]);
            if (!finiteOK(px, py, pz)) {
                report.diagnostic = MeshImportDiagnostic::NonFiniteData;
                report.messages.push_back("PLY: non-finite vertex position");
                return report;
            }
            positions.push_back(Vec3{px, py, pz});
            if (hasN) {
                normals.push_back(Vec3{static_cast<float>(vals[static_cast<size_t>(inx)]),
                                       static_cast<float>(vals[static_cast<size_t>(iny)]),
                                       static_cast<float>(vals[static_cast<size_t>(inz)])});
            }
            if (hasUV) {
                uvs.push_back(Vec2{static_cast<float>(vals[static_cast<size_t>(is)]),
                                   static_cast<float>(vals[static_cast<size_t>(it)])});
            }
        }
        for (uint64_t f = 0; f < faceCount; ++f) {
            uint64_t cnt = 0;
            if (!(bs >> cnt)) {
                report.diagnostic = MeshImportDiagnostic::ParseError;
                report.messages.push_back("PLY: truncated face data");
                return report;
            }
            std::vector<uint32_t> idx;
            idx.reserve(cnt);
            for (uint64_t k = 0; k < cnt; ++k) {
                long long id = 0;
                if (!(bs >> id) || id < 0 || static_cast<uint64_t>(id) >= vertexCount) {
                    report.diagnostic = MeshImportDiagnostic::ParseError;
                    report.messages.push_back("PLY: face index out of range");
                    return report;
                }
                idx.push_back(static_cast<uint32_t>(id));
            }
            if (cnt >= 3) {
                Face fc;
                fc.indices = std::move(idx);
                faces.push_back(std::move(fc));
            }
        }
    } else {  // BinaryLE
        const auto* base = reinterpret_cast<const unsigned char*>(buf.data());
        const unsigned char* p   = base + bodyStart;
        const unsigned char* end = base + buf.size();

        std::vector<size_t> off(vprops.size());
        size_t stride = 0;
        for (size_t k = 0; k < vprops.size(); ++k) {
            const size_t s = plyTypeSize(vprops[k].type);
            if (s == 0) {
                report.diagnostic = MeshImportDiagnostic::ParseError;
                report.messages.push_back("PLY: unknown vertex property type");
                return report;
            }
            off[k] = stride;
            stride += s;
        }
        for (uint64_t v = 0; v < vertexCount; ++v) {
            if (p + stride > end) {
                report.diagnostic = MeshImportDiagnostic::ParseError;
                report.messages.push_back("PLY: truncated binary vertex data");
                return report;
            }
            auto val = [&](int i) { return plyBinValue(p + off[static_cast<size_t>(i)], vprops[static_cast<size_t>(i)].type); };
            const auto px = static_cast<float>(val(ix));
            const auto py = static_cast<float>(val(iy));
            const auto pz = static_cast<float>(val(iz));
            if (!finiteOK(px, py, pz)) {
                report.diagnostic = MeshImportDiagnostic::NonFiniteData;
                report.messages.push_back("PLY: non-finite vertex position");
                return report;
            }
            positions.push_back(Vec3{px, py, pz});
            if (hasN) {
                normals.push_back(Vec3{static_cast<float>(val(inx)), static_cast<float>(val(iny)),
                                       static_cast<float>(val(inz))});
            }
            if (hasUV) {
                uvs.push_back(Vec2{static_cast<float>(val(is)), static_cast<float>(val(it))});
            }
            p += stride;
        }
        if (faceCount > 0) {
            const size_t ctSize = plyTypeSize(faceCountType);
            const size_t itSize = plyTypeSize(faceIndexType);
            if (ctSize == 0 || itSize == 0) {
                report.diagnostic = MeshImportDiagnostic::ParseError;
                report.messages.push_back("PLY: unknown face list type");
                return report;
            }
            for (uint64_t f = 0; f < faceCount; ++f) {
                if (p + ctSize > end) {
                    report.diagnostic = MeshImportDiagnostic::ParseError;
                    report.messages.push_back("PLY: truncated binary face data");
                    return report;
                }
                const auto cnt = static_cast<uint64_t>(plyBinValue(p, faceCountType));
                p += ctSize;
                std::vector<uint32_t> idx;
                idx.reserve(cnt);
                for (uint64_t k = 0; k < cnt; ++k) {
                    if (p + itSize > end) {
                        report.diagnostic = MeshImportDiagnostic::ParseError;
                        report.messages.push_back("PLY: truncated binary face indices");
                        return report;
                    }
                    const auto id = static_cast<uint64_t>(plyBinValue(p, faceIndexType));
                    p += itSize;
                    if (id >= vertexCount) {
                        report.diagnostic = MeshImportDiagnostic::ParseError;
                        report.messages.push_back("PLY: face index out of range");
                        return report;
                    }
                    idx.push_back(static_cast<uint32_t>(id));
                }
                if (cnt >= 3) {
                    Face fc;
                    fc.indices = std::move(idx);
                    faces.push_back(std::move(fc));
                }
            }
        }
    }

    if (positions.empty()) {
        report.diagnostic = MeshImportDiagnostic::EmptyMesh;
        report.messages.push_back("PLY: no vertices found");
        return report;
    }

    out.attributes().setPositions(std::move(positions));
    for (Face& f : faces) {
        out.topology().addFace(std::move(f));
    }
    if (hasUV && !uvs.empty()) {
        out.attributes().setUVs(std::move(uvs));
    }
    if (hasN && !normals.empty()) {
        out.attributes().setNormals(std::move(normals));
    } else if (options.computeNormalsIfMissing) {
        (void)out.computeVertexNormals();
    }

    report.verticesRead = static_cast<uint32_t>(out.attributes().vertexCount());
    report.facesRead    = static_cast<uint32_t>(out.topology().faceCount());
    report.valid        = true;
    return report;
}

// ── glTF 2.0 (.gltf / .glb) — hand-rolled JSON + accessor decoding ────────────

std::vector<uint8_t> base64Decode(const std::string& in)
{
    auto val = [](char c) -> int {
        if (c >= 'A' && c <= 'Z') return c - 'A';
        if (c >= 'a' && c <= 'z') return c - 'a' + 26;
        if (c >= '0' && c <= '9') return c - '0' + 52;
        if (c == '+') return 62;
        if (c == '/') return 63;
        return -1;
    };
    std::vector<uint8_t> out;
    int buf = 0, bits = 0;
    for (char c : in) {
        if (c == '=') break;
        const int v = val(c);
        if (v < 0) continue;  // skip whitespace / non-alphabet
        buf = (buf << 6) | v;
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            out.push_back(static_cast<uint8_t>((buf >> bits) & 0xFF));
        }
    }
    return out;
}

// Minimal JSON value + recursive-descent parser (sufficient for glTF).
struct Json {
    enum class T { Null, Bool, Num, Str, Arr, Obj } t = T::Null;
    bool b = false;
    double n = 0.0;
    std::string s;
    std::vector<Json> a;
    std::map<std::string, Json> o;

    [[nodiscard]] const Json* get(const std::string& k) const
    {
        if (t != T::Obj) return nullptr;
        const auto it = o.find(k);
        return it == o.end() ? nullptr : &it->second;
    }
    [[nodiscard]] double num(double d = 0.0) const { return t == T::Num ? n : d; }
    [[nodiscard]] const std::string& sstr() const
    {
        static const std::string empty;
        return t == T::Str ? s : empty;
    }
    [[nodiscard]] bool isArr() const { return t == T::Arr; }
    [[nodiscard]] bool isObj() const { return t == T::Obj; }
};

struct JsonParser {
    const char* p;
    const char* e;
    bool        ok = true;

    JsonParser(const char* begin, const char* end) : p(begin), e(end) {}

    void ws()
    {
        while (p < e && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) ++p;
    }
    bool parse(Json& out)
    {
        ws();
        value(out);
        return ok;
    }
    void value(Json& j)
    {
        if (p >= e) { ok = false; return; }
        const char c = *p;
        if (c == '{') object(j);
        else if (c == '[') array(j);
        else if (c == '"') { j.t = Json::T::Str; string(j.s); }
        else if (c == 't' || c == 'f') boolean(j);
        else if (c == 'n') { literal("null"); j.t = Json::T::Null; }
        else number(j);
    }
    void literal(const char* lit)
    {
        const size_t n = std::strlen(lit);
        if (static_cast<size_t>(e - p) < n || std::strncmp(p, lit, n) != 0) { ok = false; return; }
        p += n;
    }
    void boolean(Json& j)
    {
        if (*p == 't') { literal("true"); j.t = Json::T::Bool; j.b = true; }
        else { literal("false"); j.t = Json::T::Bool; j.b = false; }
    }
    void number(Json& j)
    {
        char* endp = nullptr;
        const double d = std::strtod(p, &endp);
        if (endp == p) { ok = false; return; }
        p = endp;
        j.t = Json::T::Num;
        j.n = d;
    }
    void string(std::string& out)
    {
        ++p;  // opening quote
        out.clear();
        while (p < e && *p != '"') {
            char c = *p++;
            if (c == '\\') {
                if (p >= e) { ok = false; return; }
                const char x = *p++;
                switch (x) {
                    case '"':  out += '"';  break;
                    case '\\': out += '\\'; break;
                    case '/':  out += '/';  break;
                    case 'n':  out += '\n'; break;
                    case 't':  out += '\t'; break;
                    case 'r':  out += '\r'; break;
                    case 'b':  out += '\b'; break;
                    case 'f':  out += '\f'; break;
                    case 'u': {
                        if (e - p < 4) { ok = false; return; }
                        int cp = 0;
                        for (int i = 0; i < 4; ++i) {
                            const char h = *p++;
                            cp <<= 4;
                            if (h >= '0' && h <= '9') cp |= h - '0';
                            else if (h >= 'a' && h <= 'f') cp |= h - 'a' + 10;
                            else if (h >= 'A' && h <= 'F') cp |= h - 'A' + 10;
                            else { ok = false; return; }
                        }
                        if (cp < 0x80) {
                            out += static_cast<char>(cp);
                        } else if (cp < 0x800) {
                            out += static_cast<char>(0xC0 | (cp >> 6));
                            out += static_cast<char>(0x80 | (cp & 0x3F));
                        } else {
                            out += static_cast<char>(0xE0 | (cp >> 12));
                            out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
                            out += static_cast<char>(0x80 | (cp & 0x3F));
                        }
                        break;
                    }
                    default: ok = false; return;
                }
            } else {
                out += c;
            }
        }
        if (p >= e) { ok = false; return; }
        ++p;  // closing quote
    }
    void array(Json& j)
    {
        j.t = Json::T::Arr;
        ++p;
        ws();
        if (p < e && *p == ']') { ++p; return; }
        while (p < e) {
            Json el;
            value(el);
            if (!ok) return;
            j.a.push_back(std::move(el));
            ws();
            if (p < e && *p == ',') { ++p; ws(); continue; }
            if (p < e && *p == ']') { ++p; return; }
            ok = false;
            return;
        }
        ok = false;
    }
    void object(Json& j)
    {
        j.t = Json::T::Obj;
        ++p;
        ws();
        if (p < e && *p == '}') { ++p; return; }
        while (p < e) {
            ws();
            if (p >= e || *p != '"') { ok = false; return; }
            std::string key;
            string(key);
            if (!ok) return;
            ws();
            if (p >= e || *p != ':') { ok = false; return; }
            ++p;
            ws();
            Json v;
            value(v);
            if (!ok) return;
            j.o.emplace(std::move(key), std::move(v));
            ws();
            if (p < e && *p == ',') { ++p; continue; }
            if (p < e && *p == '}') { ++p; return; }
            ok = false;
            return;
        }
        ok = false;
    }
};

int gltfCompSize(int ct)
{
    switch (ct) {
        case 5120: case 5121: return 1;  // (u)byte
        case 5122: case 5123: return 2;  // (u)short
        case 5125: case 5126: return 4;  // uint / float
        default:              return 0;
    }
}
int gltfTypeCount(const std::string& t)
{
    if (t == "SCALAR") return 1;
    if (t == "VEC2")   return 2;
    if (t == "VEC3")   return 3;
    if (t == "VEC4")   return 4;
    if (t == "MAT2")   return 4;
    if (t == "MAT3")   return 9;
    if (t == "MAT4")   return 16;
    return 0;
}
double gltfReadComp(const unsigned char* p, int ct)
{
    switch (ct) {
        case 5120: return static_cast<double>(static_cast<int8_t>(p[0]));
        case 5121: return static_cast<double>(p[0]);
        case 5122: return static_cast<double>(static_cast<int16_t>(readLEBytes(p, 2)));
        case 5123: return static_cast<double>(static_cast<uint16_t>(readLEBytes(p, 2)));
        case 5125: return static_cast<double>(static_cast<uint32_t>(readLEBytes(p, 4)));
        case 5126: return static_cast<double>(std::bit_cast<float>(static_cast<uint32_t>(readLEBytes(p, 4))));
        default:   return 0.0;
    }
}

// Reads accessor 'accIdx' into 'out' as flat doubles (count * compCount).
bool gltfReadAccessor(const Json& g, const std::vector<std::vector<uint8_t>>& bufs, int accIdx,
                      std::vector<double>& out, int& compCount, int& count)
{
    const Json* accessors = g.get("accessors");
    if (!accessors || !accessors->isArr() || accIdx < 0 || accIdx >= static_cast<int>(accessors->a.size())) {
        return false;
    }
    const Json& acc = accessors->a[static_cast<size_t>(accIdx)];
    const Json* ctv = acc.get("componentType");
    const Json* tv  = acc.get("type");
    const Json* cv  = acc.get("count");
    const Json* bvv = acc.get("bufferView");
    if (!ctv || !tv || !cv || !bvv) return false;  // sparse accessors not supported
    const int ct = static_cast<int>(ctv->num());
    compCount    = gltfTypeCount(tv->sstr());
    count        = static_cast<int>(cv->num());
    const int cs = gltfCompSize(ct);
    if (compCount == 0 || cs == 0 || count < 0) return false;

    const int bvIdx = static_cast<int>(bvv->num());
    const size_t accOffset = acc.get("byteOffset") ? static_cast<size_t>(acc.get("byteOffset")->num()) : 0;
    const Json* bviews = g.get("bufferViews");
    if (!bviews || !bviews->isArr() || bvIdx < 0 || bvIdx >= static_cast<int>(bviews->a.size())) return false;
    const Json& bv = bviews->a[static_cast<size_t>(bvIdx)];
    const Json* bufv = bv.get("buffer");
    if (!bufv) return false;
    const int bufIdx = static_cast<int>(bufv->num());
    if (bufIdx < 0 || bufIdx >= static_cast<int>(bufs.size())) return false;
    const size_t bvOffset = bv.get("byteOffset") ? static_cast<size_t>(bv.get("byteOffset")->num()) : 0;
    const size_t stride    = bv.get("byteStride") ? static_cast<size_t>(bv.get("byteStride")->num())
                                                   : static_cast<size_t>(compCount) * static_cast<size_t>(cs);
    const auto& buffer = bufs[static_cast<size_t>(bufIdx)];
    const size_t base = bvOffset + accOffset;

    out.resize(static_cast<size_t>(count) * static_cast<size_t>(compCount));
    for (int i = 0; i < count; ++i) {
        const size_t elemOff = base + static_cast<size_t>(i) * stride;
        if (elemOff + static_cast<size_t>(compCount) * static_cast<size_t>(cs) > buffer.size()) return false;
        for (int c = 0; c < compCount; ++c) {
            out[static_cast<size_t>(i) * static_cast<size_t>(compCount) + static_cast<size_t>(c)] =
                gltfReadComp(buffer.data() + elemOff + static_cast<size_t>(c) * static_cast<size_t>(cs), ct);
        }
    }
    return true;
}

MeshImportReport importGLTF(const std::string& path, const MeshImportOptions& options, Mesh& out)
{
    MeshImportReport report{};
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        report.diagnostic = MeshImportDiagnostic::FileOpenFailed;
        report.messages.push_back("Cannot import: failed to open file");
        return report;
    }
    const std::vector<char> buf((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());

    std::string jsonText;
    std::vector<uint8_t> glbBin;
    if (buf.size() >= 12 && std::memcmp(buf.data(), "glTF", 4) == 0) {
        const auto* p = reinterpret_cast<const unsigned char*>(buf.data());
        size_t off = 12;  // skip 12-byte GLB header
        while (off + 8 <= buf.size()) {
            const uint32_t clen  = static_cast<uint32_t>(readLEBytes(p + off, 4));
            const uint32_t ctype = static_cast<uint32_t>(readLEBytes(p + off + 4, 4));
            off += 8;
            if (off + clen > buf.size()) break;
            if (ctype == 0x4E4F534Au) {  // "JSON"
                jsonText.assign(reinterpret_cast<const char*>(p) + off, clen);
            } else if (ctype == 0x004E4942u) {  // "BIN\0"
                glbBin.assign(p + off, p + off + clen);
            }
            off += clen;
            off = (off + 3) & ~static_cast<size_t>(3);
        }
        if (jsonText.empty()) {
            report.diagnostic = MeshImportDiagnostic::ParseError;
            report.messages.push_back("GLB: missing JSON chunk");
            return report;
        }
    } else {
        jsonText.assign(buf.begin(), buf.end());
    }

    Json gltf;
    JsonParser jp(jsonText.data(), jsonText.data() + jsonText.size());
    if (!jp.parse(gltf) || !gltf.isObj()) {
        report.diagnostic = MeshImportDiagnostic::ParseError;
        report.messages.push_back("glTF: invalid JSON");
        return report;
    }

    // Resolve buffers (GLB bin chunk, data: URIs, or external files).
    std::vector<std::vector<uint8_t>> buffers;
    if (const Json* jbufs = gltf.get("buffers"); jbufs && jbufs->isArr()) {
        std::string dir;
        if (const size_t s = path.find_last_of("/\\"); s != std::string::npos) {
            dir = path.substr(0, s + 1);
        }
        for (const Json& jb : jbufs->a) {
            const Json* uri = jb.get("uri");
            if (!uri) {
                buffers.push_back(glbBin);
                continue;
            }
            const std::string& u = uri->sstr();
            if (u.rfind("data:", 0) == 0) {
                const size_t comma = u.find(',');
                if (comma == std::string::npos) {
                    report.diagnostic = MeshImportDiagnostic::ParseError;
                    report.messages.push_back("glTF: malformed data URI");
                    return report;
                }
                buffers.push_back(base64Decode(u.substr(comma + 1)));
            } else {
                std::ifstream bf(dir + u, std::ios::binary);
                if (!bf) {
                    report.diagnostic = MeshImportDiagnostic::ParseError;
                    report.messages.push_back("glTF: missing external buffer file");
                    return report;
                }
                std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(bf)),
                                           std::istreambuf_iterator<char>());
                buffers.push_back(std::move(bytes));
            }
        }
    }

    std::vector<Vec3> outPos, outNrm;
    std::vector<Vec2> outUV;
    std::vector<Face> faces;
    bool anyNrm = false, anyUV = false;

    const Json* meshes = gltf.get("meshes");
    if (!meshes || !meshes->isArr()) {
        report.diagnostic = MeshImportDiagnostic::EmptyMesh;
        report.messages.push_back("glTF: no meshes");
        return report;
    }
    for (const Json& m : meshes->a) {
        const Json* prims = m.get("primitives");
        if (!prims || !prims->isArr()) continue;
        for (const Json& prim : prims->a) {
            const int mode = prim.get("mode") ? static_cast<int>(prim.get("mode")->num()) : 4;
            if (mode != 4) continue;  // triangles only
            const Json* attrs = prim.get("attributes");
            if (!attrs || !attrs->isObj()) continue;
            const Json* posA = attrs->get("POSITION");
            if (!posA) continue;

            std::vector<double> pos;
            int pc = 0, pcount = 0;
            if (!gltfReadAccessor(gltf, buffers, static_cast<int>(posA->num()), pos, pc, pcount) || pc != 3) {
                report.diagnostic = MeshImportDiagnostic::ParseError;
                report.messages.push_back("glTF: bad POSITION accessor");
                return report;
            }
            const uint32_t baseV = static_cast<uint32_t>(outPos.size());
            for (int i = 0; i < pcount; ++i) {
                const auto x = static_cast<float>(pos[static_cast<size_t>(i) * 3 + 0]);
                const auto y = static_cast<float>(pos[static_cast<size_t>(i) * 3 + 1]);
                const auto z = static_cast<float>(pos[static_cast<size_t>(i) * 3 + 2]);
                if (!isFiniteFloat(x) || !isFiniteFloat(y) || !isFiniteFloat(z)) {
                    report.diagnostic = MeshImportDiagnostic::NonFiniteData;
                    report.messages.push_back("glTF: non-finite vertex position");
                    return report;
                }
                outPos.push_back(Vec3{x, y, z});
            }

            std::vector<double> nn;
            int nc = 0, ncount = 0;
            if (const Json* nA = attrs->get("NORMAL");
                nA && gltfReadAccessor(gltf, buffers, static_cast<int>(nA->num()), nn, nc, ncount) &&
                nc == 3 && ncount == pcount) {
                for (int i = 0; i < pcount; ++i) {
                    outNrm.push_back(Vec3{static_cast<float>(nn[static_cast<size_t>(i) * 3 + 0]),
                                          static_cast<float>(nn[static_cast<size_t>(i) * 3 + 1]),
                                          static_cast<float>(nn[static_cast<size_t>(i) * 3 + 2])});
                }
                anyNrm = true;
            } else {
                for (int i = 0; i < pcount; ++i) outNrm.push_back(Vec3{});
            }

            std::vector<double> tt;
            int tc = 0, tcount = 0;
            if (const Json* tA = attrs->get("TEXCOORD_0");
                tA && gltfReadAccessor(gltf, buffers, static_cast<int>(tA->num()), tt, tc, tcount) &&
                tc == 2 && tcount == pcount) {
                for (int i = 0; i < pcount; ++i) {
                    outUV.push_back(Vec2{static_cast<float>(tt[static_cast<size_t>(i) * 2 + 0]),
                                         static_cast<float>(tt[static_cast<size_t>(i) * 2 + 1])});
                }
                anyUV = true;
            } else {
                for (int i = 0; i < pcount; ++i) outUV.push_back(Vec2{0.f, 0.f});
            }

            if (const Json* idxA = prim.get("indices")) {
                std::vector<double> ids;
                int ic = 0, icount = 0;
                if (!gltfReadAccessor(gltf, buffers, static_cast<int>(idxA->num()), ids, ic, icount) || ic != 1) {
                    report.diagnostic = MeshImportDiagnostic::ParseError;
                    report.messages.push_back("glTF: bad index accessor");
                    return report;
                }
                for (int t = 0; t + 2 < icount; t += 3) {
                    Face fc;
                    fc.indices = {baseV + static_cast<uint32_t>(ids[static_cast<size_t>(t)]),
                                  baseV + static_cast<uint32_t>(ids[static_cast<size_t>(t) + 1]),
                                  baseV + static_cast<uint32_t>(ids[static_cast<size_t>(t) + 2])};
                    faces.push_back(std::move(fc));
                }
            } else {
                for (int i = 0; i + 2 < pcount; i += 3) {
                    Face fc;
                    fc.indices = {baseV + static_cast<uint32_t>(i), baseV + static_cast<uint32_t>(i + 1),
                                  baseV + static_cast<uint32_t>(i + 2)};
                    faces.push_back(std::move(fc));
                }
            }
        }
    }

    if (outPos.empty()) {
        report.diagnostic = MeshImportDiagnostic::EmptyMesh;
        report.messages.push_back("glTF: no triangle geometry found");
        return report;
    }

    out.attributes().setPositions(std::move(outPos));
    for (Face& f : faces) {
        out.topology().addFace(std::move(f));
    }
    if (anyUV) {
        out.attributes().setUVs(std::move(outUV));
    }
    if (anyNrm) {
        out.attributes().setNormals(std::move(outNrm));
    } else if (options.computeNormalsIfMissing) {
        (void)out.computeVertexNormals();
    }

    report.verticesRead = static_cast<uint32_t>(out.attributes().vertexCount());
    report.facesRead    = static_cast<uint32_t>(out.topology().faceCount());
    report.valid        = true;
    return report;
}

MeshExportReport exportGLTF(const Mesh& mesh, const std::string& path, const MeshExportOptions& options)
{
    MeshExportReport report{};

    Mesh tri = mesh;
    (void)tri.topology().triangulate();
    const auto& positions = tri.attributes().positions();
    const bool  hasN = options.includeNormals && tri.attributes().hasNormals();
    const bool  hasUV = options.includeUVs && tri.attributes().hasUVs();
    const auto& normals = tri.attributes().normals();
    const auto& uvs     = tri.attributes().uvs();
    const uint32_t V = static_cast<uint32_t>(positions.size());

    std::vector<uint32_t> indices;
    for (size_t f = 0; f < tri.topology().faceCount(); ++f) {
        const auto& fc = tri.topology().face(f);
        if (fc.indices.size() == 3) {
            indices.push_back(fc.indices[0]);
            indices.push_back(fc.indices[1]);
            indices.push_back(fc.indices[2]);
        }
    }
    const uint32_t I = static_cast<uint32_t>(indices.size());

    std::vector<uint8_t> bin;
    auto appF = [&](float v) {
        const uint32_t b = std::bit_cast<uint32_t>(v);
        bin.push_back(static_cast<uint8_t>(b & 0xFF));
        bin.push_back(static_cast<uint8_t>((b >> 8) & 0xFF));
        bin.push_back(static_cast<uint8_t>((b >> 16) & 0xFF));
        bin.push_back(static_cast<uint8_t>((b >> 24) & 0xFF));
    };
    auto appU = [&](uint32_t b) {
        bin.push_back(static_cast<uint8_t>(b & 0xFF));
        bin.push_back(static_cast<uint8_t>((b >> 8) & 0xFF));
        bin.push_back(static_cast<uint8_t>((b >> 16) & 0xFF));
        bin.push_back(static_cast<uint8_t>((b >> 24) & 0xFF));
    };
    auto pad4 = [&] { while (bin.size() % 4 != 0) bin.push_back(0); };

    float mn[3] = {0, 0, 0}, mx[3] = {0, 0, 0};
    if (V > 0) {
        mn[0] = mx[0] = positions[0].x;
        mn[1] = mx[1] = positions[0].y;
        mn[2] = mx[2] = positions[0].z;
    }
    const size_t posOff = bin.size();
    for (uint32_t i = 0; i < V; ++i) {
        const auto& p = positions[i];
        appF(p.x); appF(p.y); appF(p.z);
        mn[0] = std::min(mn[0], p.x); mn[1] = std::min(mn[1], p.y); mn[2] = std::min(mn[2], p.z);
        mx[0] = std::max(mx[0], p.x); mx[1] = std::max(mx[1], p.y); mx[2] = std::max(mx[2], p.z);
    }
    const size_t posLen = bin.size() - posOff;
    pad4();

    size_t nrmOff = 0, nrmLen = 0;
    if (hasN) {
        nrmOff = bin.size();
        for (uint32_t i = 0; i < V; ++i) { appF(normals[i].x); appF(normals[i].y); appF(normals[i].z); }
        nrmLen = bin.size() - nrmOff;
        pad4();
    }
    size_t uvOff = 0, uvLen = 0;
    if (hasUV) {
        uvOff = bin.size();
        for (uint32_t i = 0; i < V; ++i) { appF(uvs[i].u); appF(uvs[i].v); }
        uvLen = bin.size() - uvOff;
        pad4();
    }
    const size_t idxOff = bin.size();
    for (uint32_t idx : indices) appU(idx);
    const size_t idxLen = bin.size() - idxOff;
    pad4();

    int next = 1;
    int accNrm = -1, bvNrm = -1;
    if (hasN) { accNrm = next; bvNrm = next; ++next; }
    int accUV = -1, bvUV = -1;
    if (hasUV) { accUV = next; bvUV = next; ++next; }
    const int accIdx = next, bvIdx = next;

    auto f2 = [](float v) { return std::to_string(v); };
    std::string j = "{\"asset\":{\"version\":\"2.0\",\"generator\":\"Nexus-Modeling\"},";
    j += "\"scene\":0,\"scenes\":[{\"nodes\":[0]}],\"nodes\":[{\"mesh\":0}],";
    j += "\"meshes\":[{\"primitives\":[{\"attributes\":{\"POSITION\":0";
    if (hasN)  j += ",\"NORMAL\":" + std::to_string(accNrm);
    if (hasUV) j += ",\"TEXCOORD_0\":" + std::to_string(accUV);
    j += "},\"indices\":" + std::to_string(accIdx) + ",\"mode\":4}]}],";
    j += "\"accessors\":[";
    j += "{\"bufferView\":0,\"componentType\":5126,\"count\":" + std::to_string(V) +
         ",\"type\":\"VEC3\",\"min\":[" + f2(mn[0]) + "," + f2(mn[1]) + "," + f2(mn[2]) +
         "],\"max\":[" + f2(mx[0]) + "," + f2(mx[1]) + "," + f2(mx[2]) + "]}";
    if (hasN)  j += ",{\"bufferView\":" + std::to_string(bvNrm) + ",\"componentType\":5126,\"count\":" +
                    std::to_string(V) + ",\"type\":\"VEC3\"}";
    if (hasUV) j += ",{\"bufferView\":" + std::to_string(bvUV) + ",\"componentType\":5126,\"count\":" +
                    std::to_string(V) + ",\"type\":\"VEC2\"}";
    j += ",{\"bufferView\":" + std::to_string(bvIdx) + ",\"componentType\":5125,\"count\":" +
         std::to_string(I) + ",\"type\":\"SCALAR\"}";
    j += "],\"bufferViews\":[";
    j += "{\"buffer\":0,\"byteOffset\":" + std::to_string(posOff) + ",\"byteLength\":" + std::to_string(posLen) +
         ",\"target\":34962}";
    if (hasN)  j += ",{\"buffer\":0,\"byteOffset\":" + std::to_string(nrmOff) + ",\"byteLength\":" +
                    std::to_string(nrmLen) + ",\"target\":34962}";
    if (hasUV) j += ",{\"buffer\":0,\"byteOffset\":" + std::to_string(uvOff) + ",\"byteLength\":" +
                    std::to_string(uvLen) + ",\"target\":34962}";
    j += ",{\"buffer\":0,\"byteOffset\":" + std::to_string(idxOff) + ",\"byteLength\":" + std::to_string(idxLen) +
         ",\"target\":34963}";
    j += "],\"buffers\":[{\"byteLength\":" + std::to_string(bin.size()) + "}]}";

    while (j.size() % 4 != 0) j.push_back(' ');

    std::ofstream out(path, std::ios::binary);
    if (!out) {
        report.diagnostic = MeshExportDiagnostic::FileOpenFailed;
        report.messages.push_back("Cannot export: failed to open file for writing");
        std::sort(report.messages.begin(), report.messages.end());
        return report;
    }
    const uint32_t jsonLen = static_cast<uint32_t>(j.size());
    const uint32_t binLen  = static_cast<uint32_t>(bin.size());
    const uint32_t total   = 12 + 8 + jsonLen + 8 + binLen;
    out.write("glTF", 4);
    writeU32LE(out, 2);
    writeU32LE(out, total);
    writeU32LE(out, jsonLen);
    writeU32LE(out, 0x4E4F534Au);  // "JSON"
    out.write(j.data(), static_cast<std::streamsize>(jsonLen));
    writeU32LE(out, binLen);
    writeU32LE(out, 0x004E4942u);  // "BIN\0"
    out.write(reinterpret_cast<const char*>(bin.data()), static_cast<std::streamsize>(binLen));

    if (!out) {
        report.diagnostic = MeshExportDiagnostic::WriteError;
        report.messages.push_back("Write error while emitting glTF (.glb) data");
        std::sort(report.messages.begin(), report.messages.end());
        return report;
    }
    report.verticesWritten = V;
    report.facesWritten    = I / 3;
    report.valid           = true;
    std::sort(report.messages.begin(), report.messages.end());
    return report;
}

// ── USD ASCII (.usda) — hand-rolled mesh import/export ───────────────────────

// Finds "<name> = [ ... ]" within a USD prim block; returns the bracketed text.
// Matches on identifier boundaries so "points" never matches inside another token.
bool usdaFindArray(const std::string& block, const std::string& name, std::string& content)
{
    auto isIdent = [](char c) {
        return std::isalnum(static_cast<unsigned char>(c)) != 0 || c == '_' || c == ':';
    };
    size_t pos = 0;
    while ((pos = block.find(name, pos)) != std::string::npos) {
        const bool   leftOK  = (pos == 0) || !isIdent(block[pos - 1]);
        const size_t after   = pos + name.size();
        const bool   rightOK = (after >= block.size()) || !isIdent(block[after]);
        if (leftOK && rightOK) {
            const size_t eq = block.find('=', after);
            if (eq != std::string::npos) {
                const size_t br = block.find('[', eq);
                if (br != std::string::npos) {
                    const size_t end = block.find(']', br);
                    if (end != std::string::npos) {
                        content = block.substr(br + 1, end - br - 1);
                        return true;
                    }
                }
            }
        }
        pos = after;
    }
    return false;
}

// Parses every numeric token from an array's content (tuple parens/commas skipped).
std::vector<double> usdaParseScalars(const std::string& content)
{
    std::vector<double> out;
    const char* p = content.c_str();
    const char* e = p + content.size();
    while (p < e) {
        while (p < e && !((*p >= '0' && *p <= '9') || *p == '-' || *p == '+' || *p == '.')) ++p;
        if (p >= e) break;
        char* endp = nullptr;
        const double d = std::strtod(p, &endp);
        if (endp == p) { ++p; continue; }
        out.push_back(d);
        p = endp;
    }
    return out;
}

MeshImportReport importUSDA(const std::string& path, const MeshImportOptions& options, Mesh& out)
{
    MeshImportReport report{};
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        report.diagnostic = MeshImportDiagnostic::FileOpenFailed;
        report.messages.push_back("Cannot import: failed to open file");
        return report;
    }
    const std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    if (content.rfind("#usda", 0) != 0) {
        report.diagnostic = MeshImportDiagnostic::UnsupportedFormat;
        report.messages.push_back("USD: not ASCII .usda (binary USD crate is unsupported)");
        return report;
    }

    std::vector<Vec3> outPos, outNrm;
    std::vector<Vec2> outUV;
    std::vector<Face> faces;
    bool anyNrm = false, anyUV = false;

    size_t pos = 0;
    while ((pos = content.find("def Mesh", pos)) != std::string::npos) {
        const size_t brace = content.find('{', pos);
        if (brace == std::string::npos) break;
        int    depth = 0;
        size_t close = std::string::npos;
        for (size_t i = brace; i < content.size(); ++i) {
            if (content[i] == '{') ++depth;
            else if (content[i] == '}') { if (--depth == 0) { close = i; break; } }
        }
        if (close == std::string::npos) break;
        const std::string block = content.substr(brace + 1, close - brace - 1);
        pos = close + 1;

        std::string cPoints, cCounts, cIndices, cNormals, cST;
        if (!usdaFindArray(block, "points", cPoints) ||
            !usdaFindArray(block, "faceVertexIndices", cIndices) ||
            !usdaFindArray(block, "faceVertexCounts", cCounts)) {
            continue;  // not a renderable polygonal mesh block
        }
        const std::vector<double> pv     = usdaParseScalars(cPoints);
        const std::vector<double> counts = usdaParseScalars(cCounts);
        const std::vector<double> idx    = usdaParseScalars(cIndices);
        if (pv.size() < 3 || (pv.size() % 3) != 0) {
            report.diagnostic = MeshImportDiagnostic::ParseError;
            report.messages.push_back("USD: malformed points array");
            return report;
        }
        const uint32_t baseV  = static_cast<uint32_t>(outPos.size());
        const size_t   vcount = pv.size() / 3;
        for (size_t i = 0; i < vcount; ++i) {
            const auto x = static_cast<float>(pv[i * 3 + 0]);
            const auto y = static_cast<float>(pv[i * 3 + 1]);
            const auto z = static_cast<float>(pv[i * 3 + 2]);
            if (!isFiniteFloat(x) || !isFiniteFloat(y) || !isFiniteFloat(z)) {
                report.diagnostic = MeshImportDiagnostic::NonFiniteData;
                report.messages.push_back("USD: non-finite vertex position");
                return report;
            }
            outPos.push_back(Vec3{x, y, z});
        }

        bool blockHasN = false;
        if (usdaFindArray(block, "normals", cNormals)) {
            const std::vector<double> nv = usdaParseScalars(cNormals);
            if (nv.size() == vcount * 3) {
                for (size_t i = 0; i < vcount; ++i) {
                    outNrm.push_back(Vec3{static_cast<float>(nv[i * 3 + 0]),
                                          static_cast<float>(nv[i * 3 + 1]),
                                          static_cast<float>(nv[i * 3 + 2])});
                }
                blockHasN = true;
                anyNrm    = true;
            }
        }
        if (!blockHasN) {
            for (size_t i = 0; i < vcount; ++i) outNrm.push_back(Vec3{});
        }

        bool blockHasUV = false;
        if (usdaFindArray(block, "primvars:st", cST)) {
            const std::vector<double> uv = usdaParseScalars(cST);
            if (uv.size() == vcount * 2) {
                for (size_t i = 0; i < vcount; ++i) {
                    outUV.push_back(Vec2{static_cast<float>(uv[i * 2 + 0]), static_cast<float>(uv[i * 2 + 1])});
                }
                blockHasUV = true;
                anyUV      = true;
            }
        }
        if (!blockHasUV) {
            for (size_t i = 0; i < vcount; ++i) outUV.push_back(Vec2{0.f, 0.f});
        }

        size_t cursor = 0;
        for (double dc : counts) {
            const int c = static_cast<int>(dc);
            if (c < 0 || cursor + static_cast<size_t>(c) > idx.size()) {
                report.diagnostic = MeshImportDiagnostic::ParseError;
                report.messages.push_back("USD: faceVertexIndices/Counts mismatch");
                return report;
            }
            if (c >= 3) {
                Face fc;
                for (int k = 0; k < c; ++k) {
                    const long id = static_cast<long>(idx[cursor + static_cast<size_t>(k)]);
                    if (id < 0 || static_cast<size_t>(id) >= vcount) {
                        report.diagnostic = MeshImportDiagnostic::ParseError;
                        report.messages.push_back("USD: face index out of range");
                        return report;
                    }
                    fc.indices.push_back(baseV + static_cast<uint32_t>(id));
                }
                faces.push_back(std::move(fc));
            }
            cursor += static_cast<size_t>(c);
        }
    }

    if (outPos.empty()) {
        report.diagnostic = MeshImportDiagnostic::EmptyMesh;
        report.messages.push_back("USD: no polygonal mesh found");
        return report;
    }

    out.attributes().setPositions(std::move(outPos));
    for (Face& f : faces) {
        out.topology().addFace(std::move(f));
    }
    if (anyUV) {
        out.attributes().setUVs(std::move(outUV));
    }
    if (anyNrm) {
        out.attributes().setNormals(std::move(outNrm));
    } else if (options.computeNormalsIfMissing) {
        (void)out.computeVertexNormals();
    }

    report.verticesRead = static_cast<uint32_t>(out.attributes().vertexCount());
    report.facesRead    = static_cast<uint32_t>(out.topology().faceCount());
    report.valid        = true;
    return report;
}

MeshExportReport exportUSDA(const Mesh& mesh, const std::string& path, const MeshExportOptions& options)
{
    MeshExportReport report{};
    std::ofstream out(path);
    if (!out) {
        report.diagnostic = MeshExportDiagnostic::FileOpenFailed;
        report.messages.push_back("Cannot export: failed to open file for writing");
        std::sort(report.messages.begin(), report.messages.end());
        return report;
    }
    out.precision(9);

    const auto& positions = mesh.attributes().positions();
    const bool  hasN  = options.includeNormals && mesh.attributes().hasNormals();
    const bool  hasUV = options.includeUVs && mesh.attributes().hasUVs();
    const auto& normals = mesh.attributes().normals();
    const auto& uvs     = mesh.attributes().uvs();
    const auto& topo    = mesh.topology();

    out << "#usda 1.0\n(\n    defaultPrim = \"mesh\"\n    upAxis = \"Y\"\n)\n\n";
    out << "def Mesh \"mesh\"\n{\n";

    uint32_t faceCount = 0;
    out << "    int[] faceVertexCounts = [";
    for (size_t f = 0; f < topo.faceCount(); ++f) {
        const auto& fc = topo.face(f);
        if (fc.indices.size() < 3) continue;
        if (faceCount++ != 0) out << ", ";
        out << fc.indices.size();
    }
    out << "]\n";

    out << "    int[] faceVertexIndices = [";
    bool firstIdx = true;
    for (size_t f = 0; f < topo.faceCount(); ++f) {
        const auto& fc = topo.face(f);
        if (fc.indices.size() < 3) continue;
        for (const uint32_t id : fc.indices) {
            if (!firstIdx) out << ", ";
            firstIdx = false;
            out << id;
        }
    }
    out << "]\n";

    out << "    point3f[] points = [";
    for (size_t i = 0; i < positions.size(); ++i) {
        if (i != 0) out << ", ";
        out << "(" << positions[i].x << ", " << positions[i].y << ", " << positions[i].z << ")";
    }
    out << "]\n";

    if (hasN) {
        out << "    normal3f[] normals = [";
        for (size_t i = 0; i < normals.size(); ++i) {
            if (i != 0) out << ", ";
            out << "(" << normals[i].x << ", " << normals[i].y << ", " << normals[i].z << ")";
        }
        out << "] (\n        interpolation = \"vertex\"\n    )\n";
    }
    if (hasUV) {
        out << "    texCoord2f[] primvars:st = [";
        for (size_t i = 0; i < uvs.size(); ++i) {
            if (i != 0) out << ", ";
            out << "(" << uvs[i].u << ", " << uvs[i].v << ")";
        }
        out << "] (\n        interpolation = \"vertex\"\n    )\n";
    }

    out << "    uniform token subdivisionScheme = \"none\"\n";
    out << "}\n";

    if (!out) {
        report.diagnostic = MeshExportDiagnostic::WriteError;
        report.messages.push_back("Write error while emitting USD (.usda) data");
        std::sort(report.messages.begin(), report.messages.end());
        return report;
    }
    report.verticesWritten = static_cast<uint32_t>(positions.size());
    report.facesWritten    = faceCount;
    report.valid           = true;
    std::sort(report.messages.begin(), report.messages.end());
    return report;
}

} // namespace

MeshExportReport MeshIO::exportMesh(const Mesh&              mesh,
                                    const std::string&       path,
                                    const MeshExportOptions& options) noexcept
{
    MeshExportReport report{};

    if (!mesh.isValid()) {
        report.diagnostic = MeshExportDiagnostic::InvalidMesh;
        report.messages.push_back("Cannot export: mesh is invalid");
        std::sort(report.messages.begin(), report.messages.end());
        return report;
    }

    if (path.empty()) {
        report.diagnostic = MeshExportDiagnostic::FileOpenFailed;
        report.messages.push_back("Cannot export: path is empty");
        std::sort(report.messages.begin(), report.messages.end());
        return report;
    }

    if (!meshAttributesAreFinite(mesh, options, report)) {
        return report;
    }

    switch (options.format) {
        case MeshExportFormat::OBJ:
            return exportOBJ(mesh, path, options);
        case MeshExportFormat::PLY:
            return exportPLY(mesh, path, options);
        case MeshExportFormat::STL:
            return exportSTL(mesh, path, options);
        case MeshExportFormat::GLTF:
            return exportGLTF(mesh, path, options);
        case MeshExportFormat::USDA:
            return exportUSDA(mesh, path, options);
    }

    report.diagnostic = MeshExportDiagnostic::UnsupportedFormat;
    report.messages.push_back("Unknown export format");
    std::sort(report.messages.begin(), report.messages.end());
    return report;
}

namespace {

// Lower-cased file extension including the leading dot, or "" if none.
std::string lowerExtension(const std::string& path)
{
    const size_t dot = path.find_last_of('.');
    const size_t slash = path.find_last_of("/\\");
    if (dot == std::string::npos || (slash != std::string::npos && dot < slash)) {
        return {};
    }
    std::string ext = path.substr(dot);
    std::transform(ext.begin(), ext.end(), ext.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return ext;
}

} // namespace

MeshImportReport MeshIO::importMesh(const std::string&       path,
                                    const MeshImportOptions& options,
                                    Mesh&                    outMesh) noexcept
{
    outMesh = Mesh{};
    MeshImportReport report{};

    if (path.empty()) {
        report.diagnostic = MeshImportDiagnostic::FileOpenFailed;
        report.messages.push_back("Cannot import: path is empty");
        return report;
    }

    MeshImportFormat fmt = options.format;
    if (fmt == MeshImportFormat::Auto) {
        const std::string ext = lowerExtension(path);
        if (ext == ".obj") {
            fmt = MeshImportFormat::OBJ;
        } else if (ext == ".stl") {
            fmt = MeshImportFormat::STL;
        } else if (ext == ".ply") {
            fmt = MeshImportFormat::PLY;
        } else if (ext == ".gltf" || ext == ".glb") {
            fmt = MeshImportFormat::GLTF;
        } else if (ext == ".usda" || ext == ".usd") {
            fmt = MeshImportFormat::USDA;
        } else {
            report.diagnostic = MeshImportDiagnostic::UnsupportedFormat;
            report.messages.push_back("Cannot import: unrecognized file extension");
            return report;
        }
    }

    switch (fmt) {
        case MeshImportFormat::OBJ:
            report = importOBJ(path, options, outMesh);
            break;
        case MeshImportFormat::STL:
            report = importSTL(path, options, outMesh);
            break;
        case MeshImportFormat::PLY:
            report = importPLY(path, options, outMesh);
            break;
        case MeshImportFormat::GLTF:
            report = importGLTF(path, options, outMesh);
            break;
        case MeshImportFormat::USDA:
            report = importUSDA(path, options, outMesh);
            break;
        default:
            report.diagnostic = MeshImportDiagnostic::UnsupportedFormat;
            report.messages.push_back("Unsupported import format");
            break;
    }

    if (!report.valid) {
        outMesh = Mesh{};  // leave output cleared on any failure
    }
    std::sort(report.messages.begin(), report.messages.end());
    return report;
}

} // namespace nexus::geometry
