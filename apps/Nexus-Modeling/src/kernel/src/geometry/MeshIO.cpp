#include <nexus/geometry/MeshIO.h>

#include <algorithm>
#include <bit>
#include <cstdint>
#include <fstream>
#include <limits>
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

} // namespace nexus::geometry
