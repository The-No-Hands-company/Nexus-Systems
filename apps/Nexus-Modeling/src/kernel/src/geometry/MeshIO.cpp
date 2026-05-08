#include <nexus/geometry/MeshIO.h>

#include <cstdio>
#include <cstring>
#include <sstream>
#include <string>

namespace nexus::geometry {

namespace {

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
        return report;
    }

    if (path.empty()) {
        report.diagnostic = MeshExportDiagnostic::FileOpenFailed;
        report.messages.push_back("Cannot export: path is empty");
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
    return report;
}

} // namespace nexus::geometry
