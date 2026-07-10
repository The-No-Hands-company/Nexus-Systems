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
