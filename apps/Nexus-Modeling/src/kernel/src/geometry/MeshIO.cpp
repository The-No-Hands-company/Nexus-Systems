#include <nexus/geometry/MeshIO.h>

#include <algorithm>
#include <bit>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <cstdio>
#include <cstring>
#include <limits>
#include <sstream>
#include <string>

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

void sortMessages(MeshImportReport& report)
{
    std::sort(report.messages.begin(), report.messages.end());
}

bool parseFloatToken(const std::string& text, float& out) noexcept
{
    char* end = nullptr;
    out = std::strtof(text.c_str(), &end);
    if (end == text.c_str() || *end != '\0') {
        return false;
    }

    constexpr std::uint32_t kExpMask = 0x7F800000u;
    const std::uint32_t bits = std::bit_cast<std::uint32_t>(out);
    return (bits & kExpMask) != kExpMask;
}

bool parseIntToken(const std::string& text, int& out) noexcept
{
    char* end = nullptr;
    const long value = std::strtol(text.c_str(), &end, 10);
    if (end == text.c_str() || *end != '\0') {
        return false;
    }
    if (value < std::numeric_limits<int>::min() || value > std::numeric_limits<int>::max()) {
        return false;
    }
    out = static_cast<int>(value);
    return true;
}

std::vector<std::string> splitWs(const std::string& line)
{
    std::istringstream stream(line);
    std::vector<std::string> tokens;
    std::string token;
    while (stream >> token) {
        tokens.push_back(token);
    }
    return tokens;
}

bool parseObjVertexRef(const std::string& token, uint32_t& vertexIndex) noexcept
{
    const size_t slash = token.find('/');
    const std::string vertexText = (slash == std::string::npos) ? token : token.substr(0, slash);

    int parsed = 0;
    if (!parseIntToken(vertexText, parsed) || parsed <= 0) {
        return false;
    }
    vertexIndex = static_cast<uint32_t>(parsed - 1);
    return true;
}

MeshImportReport importOBJ(const std::string& path,
                           Mesh& outMesh,
                           const MeshImportOptions& options)
{
    MeshImportReport report{};

    std::ifstream in(path);
    if (!in.is_open()) {
        report.diagnostic = MeshImportDiagnostic::FileOpenFailed;
        report.messages.push_back("Failed to open file for reading: " + path);
        sortMessages(report);
        return report;
    }

    std::vector<nexus::render::Vec3> positions;
    std::vector<nexus::render::Vec3> normals;
    std::vector<Vec2> uvs;
    std::vector<Face> faces;

    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (line.empty() || line[0] == '#') {
            continue;
        }

        const std::vector<std::string> tokens = splitWs(line);
        if (tokens.empty()) {
            continue;
        }

        const std::string& tag = tokens[0];
        if (tag == "v") {
            if (tokens.size() != 4u) {
                report.diagnostic = MeshImportDiagnostic::InvalidData;
                report.messages.push_back("Invalid vertex record");
                sortMessages(report);
                return report;
            }
            nexus::render::Vec3 position{};
            if (!parseFloatToken(tokens[1], position.x) ||
                !parseFloatToken(tokens[2], position.y) ||
                !parseFloatToken(tokens[3], position.z)) {
                report.diagnostic = MeshImportDiagnostic::InvalidData;
                report.messages.push_back("Invalid vertex scalar");
                sortMessages(report);
                return report;
            }
            positions.push_back(position);
            continue;
        }

        if (tag == "vn") {
            if (tokens.size() != 4u) {
                report.diagnostic = MeshImportDiagnostic::InvalidData;
                report.messages.push_back("Invalid normal record");
                sortMessages(report);
                return report;
            }
            nexus::render::Vec3 normal{};
            if (!parseFloatToken(tokens[1], normal.x) ||
                !parseFloatToken(tokens[2], normal.y) ||
                !parseFloatToken(tokens[3], normal.z)) {
                report.diagnostic = MeshImportDiagnostic::InvalidData;
                report.messages.push_back("Invalid normal scalar");
                sortMessages(report);
                return report;
            }
            normals.push_back(normal);
            continue;
        }

        if (tag == "vt") {
            if (tokens.size() < 3u || tokens.size() > 4u) {
                report.diagnostic = MeshImportDiagnostic::InvalidData;
                report.messages.push_back("Invalid UV record");
                sortMessages(report);
                return report;
            }
            Vec2 uv{};
            if (!parseFloatToken(tokens[1], uv.u) || !parseFloatToken(tokens[2], uv.v)) {
                report.diagnostic = MeshImportDiagnostic::InvalidData;
                report.messages.push_back("Invalid UV scalar");
                sortMessages(report);
                return report;
            }
            uvs.push_back(uv);
            continue;
        }

        if (tag == "f") {
            if (tokens.size() < 4u) {
                report.diagnostic = MeshImportDiagnostic::InvalidData;
                report.messages.push_back("Face has fewer than 3 vertices");
                sortMessages(report);
                return report;
            }

            Face face;
            face.indices.reserve(tokens.size() - 1u);
            for (size_t i = 1; i < tokens.size(); ++i) {
                uint32_t vertexIndex = 0u;
                if (!parseObjVertexRef(tokens[i], vertexIndex)) {
                    report.diagnostic = MeshImportDiagnostic::InvalidData;
                    report.messages.push_back("Invalid face vertex reference");
                    sortMessages(report);
                    return report;
                }
                if (vertexIndex >= positions.size()) {
                    report.diagnostic = MeshImportDiagnostic::InvalidData;
                    report.messages.push_back("Face vertex index out of range");
                    sortMessages(report);
                    return report;
                }
                face.indices.push_back(vertexIndex);
            }
            faces.push_back(std::move(face));
            continue;
        }

        // Ignore unknown/unsupported OBJ directives to keep the baseline deterministic.
    }

    if (positions.empty() || faces.empty()) {
        report.diagnostic = MeshImportDiagnostic::InvalidData;
        report.messages.push_back("OBJ payload does not contain positions and faces");
        sortMessages(report);
        return report;
    }

    Mesh parsed;
    parsed.attributes().setPositions(std::move(positions));
    if (normals.size() == parsed.attributes().vertexCount()) {
        parsed.attributes().setNormals(std::move(normals));
    }
    if (uvs.size() == parsed.attributes().vertexCount()) {
        parsed.attributes().setUVs(std::move(uvs));
    }
    for (Face& face : faces) {
        parsed.topology().addFace(std::move(face));
    }

    if (options.triangulateFaces) {
        (void)parsed.topology().triangulate();
    }
    if (options.computeNormals && !parsed.attributes().hasNormals()) {
        (void)parsed.computeVertexNormals();
    }

    if (!parsed.isValid()) {
        report.diagnostic = MeshImportDiagnostic::InvalidMesh;
        report.messages.push_back("Imported OBJ mesh is invalid");
        sortMessages(report);
        return report;
    }

    outMesh = std::move(parsed);
    report.valid = true;
    report.verticesRead = static_cast<uint32_t>(outMesh.attributes().vertexCount());
    report.facesRead = static_cast<uint32_t>(outMesh.topology().faceCount());
    sortMessages(report);
    return report;
}

// ── OBJ ───────────────────────────────────────────────────────────────────────
MeshExportReport exportOBJ(const Mesh&              mesh,
                           const std::string&       path,
                           const MeshExportOptions& opts)
{
    MeshExportReport report{};

    std::FILE* fp = std::fopen(path.c_str(), "w");
    if (!fp) {
        report.diagnostic = MeshExportDiagnostic::FileOpenFailed;
        report.messages.push_back("Failed to open file for writing: " + path);
        std::sort(report.messages.begin(), report.messages.end());
        return report;
    }

    const auto& positions = mesh.attributes().positions();
    const bool  hasNormals = opts.includeNormals && mesh.attributes().hasNormals();
    const bool  hasUVs     = opts.includeUVs     && mesh.attributes().hasUVs();
    const auto& normals    = hasNormals ? mesh.attributes().normals()  : decltype(mesh.attributes().normals()){};
    const auto& uvs        = hasUVs     ? mesh.attributes().uvs()      : decltype(mesh.attributes().uvs()){};

    auto writeError = [&]() -> MeshExportReport {
        std::fclose(fp);
        report.diagnostic = MeshExportDiagnostic::WriteError;
        report.messages.push_back("Write error while exporting OBJ: " + path);
        std::sort(report.messages.begin(), report.messages.end());
        return report;
    };

    // Header comment
    if (std::fprintf(fp, "# Nexus OBJ export\n") < 0) { return writeError(); }

    // Vertex positions
    for (const auto& p : positions) {
        if (std::fprintf(fp, "v %.9g %.9g %.9g\n", p.x, p.y, p.z) < 0) {
            return writeError();
        }
    }

    // Vertex normals
    if (hasNormals) {
        for (const auto& n : normals) {
            if (std::fprintf(fp, "vn %.9g %.9g %.9g\n", n.x, n.y, n.z) < 0) {
                return writeError();
            }
        }
    }

    // Texture coords
    if (hasUVs) {
        for (const auto& uv : uvs) {
            if (std::fprintf(fp, "vt %.9g %.9g\n", uv.u, uv.v) < 0) {
                return writeError();
            }
        }
    }

    // Faces (OBJ is 1-indexed)
    const size_t faceCount = mesh.topology().faceCount();
    for (size_t fi = 0; fi < faceCount; ++fi) {
        const Face& f = mesh.topology().face(fi);
        if (f.indices.empty()) {
            continue;
        }

        if (std::fprintf(fp, "f") < 0) { return writeError(); }

        for (const uint32_t idx : f.indices) {
            const uint32_t vi = idx + 1u;  // 1-based

            int written = 0;
            if (hasNormals && hasUVs) {
                written = std::fprintf(fp, " %u/%u/%u", vi, vi, vi);
            } else if (hasNormals) {
                written = std::fprintf(fp, " %u//%u", vi, vi);
            } else if (hasUVs) {
                written = std::fprintf(fp, " %u/%u", vi, vi);
            } else {
                written = std::fprintf(fp, " %u", vi);
            }
            if (written < 0) { return writeError(); }
        }

        if (std::fprintf(fp, "\n") < 0) { return writeError(); }
        ++report.facesWritten;
    }

    std::fclose(fp);

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

    std::FILE* fp = std::fopen(path.c_str(), "w");
    if (!fp) {
        report.diagnostic = MeshExportDiagnostic::FileOpenFailed;
        report.messages.push_back("Failed to open file for writing: " + path);
        std::sort(report.messages.begin(), report.messages.end());
        return report;
    }

    const auto& positions  = mesh.attributes().positions();
    const bool  hasNormals = opts.includeNormals && mesh.attributes().hasNormals();
    const bool  hasUVs     = opts.includeUVs     && mesh.attributes().hasUVs();
    const auto& normals    = hasNormals ? mesh.attributes().normals() : decltype(mesh.attributes().normals()){};
    const auto& uvs        = hasUVs     ? mesh.attributes().uvs()     : decltype(mesh.attributes().uvs()){};

    const uint32_t vertexCount = static_cast<uint32_t>(positions.size());
    const uint32_t faceCount   = static_cast<uint32_t>(mesh.topology().faceCount());

    auto writeError = [&]() -> MeshExportReport {
        std::fclose(fp);
        report.diagnostic = MeshExportDiagnostic::WriteError;
        report.messages.push_back("Write error while exporting PLY: " + path);
        std::sort(report.messages.begin(), report.messages.end());
        return report;
    };

    // PLY header
    if (std::fprintf(fp,
            "ply\n"
            "format ascii 1.0\n"
            "comment Nexus PLY export\n"
            "element vertex %u\n"
            "property float x\n"
            "property float y\n"
            "property float z\n",
            vertexCount) < 0) { return writeError(); }

    if (hasNormals) {
        if (std::fprintf(fp,
                "property float nx\n"
                "property float ny\n"
                "property float nz\n") < 0) { return writeError(); }
    }
    if (hasUVs) {
        if (std::fprintf(fp,
                "property float s\n"
                "property float t\n") < 0) { return writeError(); }
    }

    if (std::fprintf(fp,
            "element face %u\n"
            "property list uchar int vertex_indices\n"
            "end_header\n",
            faceCount) < 0) { return writeError(); }

    // Vertex data
    for (uint32_t vi = 0; vi < vertexCount; ++vi) {
        int w = std::fprintf(fp, "%.9g %.9g %.9g",
                             positions[vi].x, positions[vi].y, positions[vi].z);
        if (w < 0) { return writeError(); }

        if (hasNormals) {
            w = std::fprintf(fp, " %.9g %.9g %.9g",
                             normals[vi].x, normals[vi].y, normals[vi].z);
            if (w < 0) { return writeError(); }
        }
        if (hasUVs) {
            w = std::fprintf(fp, " %.9g %.9g", uvs[vi].u, uvs[vi].v);
            if (w < 0) { return writeError(); }
        }

        if (std::fprintf(fp, "\n") < 0) { return writeError(); }
    }

    // Face data
    for (size_t fi = 0; fi < faceCount; ++fi) {
        const Face& f = mesh.topology().face(fi);
        if (f.indices.empty()) {
            continue;
        }

        if (std::fprintf(fp, "%zu", f.indices.size()) < 0) { return writeError(); }
        for (const uint32_t idx : f.indices) {
            if (std::fprintf(fp, " %u", idx) < 0) { return writeError(); }
        }
        if (std::fprintf(fp, "\n") < 0) { return writeError(); }
        ++report.facesWritten;
    }

    std::fclose(fp);

    report.verticesWritten = vertexCount;
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
    }

    report.diagnostic = MeshExportDiagnostic::UnsupportedFormat;
    report.messages.push_back("Unknown export format");
    std::sort(report.messages.begin(), report.messages.end());
    return report;
}

MeshImportReport MeshIO::importMesh(const std::string& path,
                                    Mesh& mesh,
                                    const MeshImportOptions& options) noexcept
{
    MeshImportReport report{};

    if (path.empty()) {
        report.diagnostic = MeshImportDiagnostic::FileOpenFailed;
        report.messages.push_back("Cannot import: path is empty");
        sortMessages(report);
        return report;
    }

    switch (options.format) {
        case MeshImportFormat::OBJ:
            return importOBJ(path, mesh, options);
    }

    report.diagnostic = MeshImportDiagnostic::UnsupportedFormat;
    report.messages.push_back("Unknown import format");
    sortMessages(report);
    return report;
}

} // namespace nexus::geometry
