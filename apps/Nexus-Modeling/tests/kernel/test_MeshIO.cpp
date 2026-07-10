#include <nexus/geometry/Mesh.h>
#include <nexus/geometry/MeshIO.h>

#include <gtest/gtest.h>

#include <cstdio>
#include <fstream>
#include <limits>
#include <string>

namespace nexus::geometry::testing {

using namespace nexus::geometry::primitives;

namespace {

// Reads file contents into a string; returns empty string on failure.
std::string readFile(const std::string& path)
{
    std::ifstream f(path);
    if (!f.is_open()) {
        return {};
    }
    return std::string(std::istreambuf_iterator<char>(f),
                       std::istreambuf_iterator<char>());
}

// Counts lines in a string that start with the given prefix.
size_t countLinesWithPrefix(const std::string& content, const std::string& prefix)
{
    size_t count = 0;
    size_t pos   = 0;
    while (pos < content.size()) {
        const size_t end = content.find('\n', pos);
        const size_t lineEnd = (end == std::string::npos) ? content.size() : end;
        const std::string line = content.substr(pos, lineEnd - pos);
        if (line.substr(0, prefix.size()) == prefix) {
            ++count;
        }
        pos = lineEnd + 1u;
    }
    return count;
}

// Temporary file path helper (unique enough for tests, cleaned up after)
std::string tmpPath(const std::string& suffix)
{
    return "/tmp/nexus_meshio_test_" + suffix;
}

} // namespace

// ─────────────────────────────────────────────────────────────────────────────

TEST(MeshIO, RejectsInvalidMesh)
{
    Mesh empty;
    MeshExportOptions opts{};
    const MeshExportReport report = MeshIO::exportMesh(empty, tmpPath("invalid.obj"), opts);

    EXPECT_FALSE(report.valid);
    EXPECT_TRUE(hasDiagnostic(report.diagnostic, MeshExportDiagnostic::InvalidMesh));
}

TEST(MeshIO, RejectsEmptyPath)
{
    Mesh box = makeBox(1.f, 1.f, 1.f);
    MeshExportOptions opts{};
    const MeshExportReport report = MeshIO::exportMesh(box, "", opts);

    EXPECT_FALSE(report.valid);
    EXPECT_TRUE(hasDiagnostic(report.diagnostic, MeshExportDiagnostic::FileOpenFailed));
}

TEST(MeshIO, ExportOBJProducesFile)
{
    const std::string path = tmpPath("box.obj");
    std::remove(path.c_str());

    Mesh box = makeBox(1.f, 1.f, 1.f);
    MeshExportOptions opts{};
    opts.format         = MeshExportFormat::OBJ;
    opts.includeNormals = false;
    opts.includeUVs     = false;

    const MeshExportReport report = MeshIO::exportMesh(box, path, opts);

    ASSERT_TRUE(report.valid);
    EXPECT_TRUE(report.isSuccess());
    EXPECT_GT(report.verticesWritten, 0u);
    EXPECT_GT(report.facesWritten,    0u);

    const std::string content = readFile(path);
    EXPECT_FALSE(content.empty());
    EXPECT_GT(countLinesWithPrefix(content, "v "), 0u);
    EXPECT_GT(countLinesWithPrefix(content, "f "), 0u);

    std::remove(path.c_str());
}

TEST(MeshIO, ExportOBJVertexAndFaceCountMatchMesh)
{
    const std::string path = tmpPath("box_counts.obj");
    std::remove(path.c_str());

    Mesh box = makeBox(1.f, 1.f, 1.f);
    MeshExportOptions opts{};
    opts.format         = MeshExportFormat::OBJ;
    opts.includeNormals = false;
    opts.includeUVs     = false;

    const MeshExportReport report = MeshIO::exportMesh(box, path, opts);
    ASSERT_TRUE(report.valid);

    const std::string content = readFile(path);
    const size_t vCount = countLinesWithPrefix(content, "v ");
    const size_t fCount = countLinesWithPrefix(content, "f ");

    EXPECT_EQ(vCount, box.attributes().vertexCount());
    EXPECT_EQ(fCount, box.topology().faceCount());
    EXPECT_EQ(report.verticesWritten, static_cast<uint32_t>(box.attributes().vertexCount()));
    EXPECT_EQ(report.facesWritten,    static_cast<uint32_t>(box.topology().faceCount()));

    std::remove(path.c_str());
}

TEST(MeshIO, ExportOBJIncludesNormalLinesWhenRequested)
{
    const std::string path = tmpPath("box_normals.obj");
    std::remove(path.c_str());

    Mesh box = makeBox(1.f, 1.f, 1.f);
    (void)box.computeVertexNormals();

    MeshExportOptions opts{};
    opts.format         = MeshExportFormat::OBJ;
    opts.includeNormals = true;
    opts.includeUVs     = false;

    const MeshExportReport report = MeshIO::exportMesh(box, path, opts);
    ASSERT_TRUE(report.valid);

    const std::string content = readFile(path);
    EXPECT_GT(countLinesWithPrefix(content, "vn "), 0u);

    std::remove(path.c_str());
}

TEST(MeshIO, ExportPLYProducesFile)
{
    const std::string path = tmpPath("box.ply");
    std::remove(path.c_str());

    Mesh box = makeBox(1.f, 1.f, 1.f);
    MeshExportOptions opts{};
    opts.format         = MeshExportFormat::PLY;
    opts.includeNormals = false;
    opts.includeUVs     = false;

    const MeshExportReport report = MeshIO::exportMesh(box, path, opts);

    ASSERT_TRUE(report.valid);
    EXPECT_GT(report.verticesWritten, 0u);
    EXPECT_GT(report.facesWritten,    0u);

    const std::string content = readFile(path);
    EXPECT_FALSE(content.empty());
    EXPECT_NE(content.find("ply"),         std::string::npos);
    EXPECT_NE(content.find("format ascii"),std::string::npos);
    EXPECT_NE(content.find("end_header"),  std::string::npos);

    std::remove(path.c_str());
}

TEST(MeshIO, ExportPLYHeaderMatchesMeshCounts)
{
    const std::string path = tmpPath("box_counts.ply");
    std::remove(path.c_str());

    Mesh box = makeBox(1.f, 1.f, 1.f);
    const size_t expectedVerts = box.attributes().vertexCount();
    const size_t expectedFaces = box.topology().faceCount();

    MeshExportOptions opts{};
    opts.format = MeshExportFormat::PLY;

    const MeshExportReport report = MeshIO::exportMesh(box, path, opts);
    ASSERT_TRUE(report.valid);

    const std::string content = readFile(path);

    // PLY header must declare the right vertex and face counts
    const std::string vertLine = "element vertex " + std::to_string(expectedVerts);
    const std::string faceLine = "element face "   + std::to_string(expectedFaces);
    EXPECT_NE(content.find(vertLine), std::string::npos);
    EXPECT_NE(content.find(faceLine), std::string::npos);

    std::remove(path.c_str());
}

TEST(MeshIO, ExportOBJIsDeterministic)
{
    const std::string pathA = tmpPath("det_a.obj");
    const std::string pathB = tmpPath("det_b.obj");
    std::remove(pathA.c_str());
    std::remove(pathB.c_str());

    Mesh box = makeBox(1.f, 1.f, 1.f);
    (void)box.computeVertexNormals();

    MeshExportOptions opts{};
    opts.format         = MeshExportFormat::OBJ;
    opts.includeNormals = true;
    opts.includeUVs     = false;

    ASSERT_TRUE(MeshIO::exportMesh(box, pathA, opts).valid);
    ASSERT_TRUE(MeshIO::exportMesh(box, pathB, opts).valid);

    EXPECT_EQ(readFile(pathA), readFile(pathB));

    std::remove(pathA.c_str());
    std::remove(pathB.c_str());
}

TEST(MeshIO, MessagesAreDeterministicAndSorted)
{
    // Two identical failure calls must return identical, sorted message vectors.
    Mesh invalid{}; // default-constructed mesh is invalid
    MeshExportOptions opts{};
    opts.format = MeshExportFormat::OBJ;

    const MeshExportReport r1 = MeshIO::exportMesh(invalid, "out.obj", opts);
    const MeshExportReport r2 = MeshIO::exportMesh(invalid, "out.obj", opts);

    EXPECT_FALSE(r1.valid);
    EXPECT_EQ(r1.messages, r2.messages);

    auto sorted = r1.messages;
    std::sort(sorted.begin(), sorted.end());
    EXPECT_EQ(r1.messages, sorted);
}

TEST(MeshIO, ReportMessagesAreSortedOnUnsupportedFormat)
{
    // Trigger UnsupportedFormat path and verify message ordering contract.
    Mesh box = makeBox(1.f, 1.f, 1.f);
    (void)box.computeVertexNormals();
    MeshExportOptions opts{};
    // Cast an out-of-range value to reach the default switch branch.
    opts.format = static_cast<MeshExportFormat>(255);

    const MeshExportReport r = MeshIO::exportMesh(box, "out.xyz", opts);
    EXPECT_FALSE(r.valid);
    EXPECT_FALSE(r.messages.empty());

    auto sorted = r.messages;
    std::sort(sorted.begin(), sorted.end());
    EXPECT_EQ(r.messages, sorted);
}

TEST(MeshIO, ExportRejectsNonFiniteAttributeData)
{
    {
        const std::string path = tmpPath("export_nan_position.obj");
        std::remove(path.c_str());

        Mesh mesh = makeTriangle(1.f);
        auto positions = mesh.attributes().positions();
        positions[0].x = std::numeric_limits<float>::quiet_NaN();
        mesh.attributes().setPositions(std::move(positions));

        MeshExportOptions opts{};
        opts.format = MeshExportFormat::OBJ;

        const MeshExportReport rep = MeshIO::exportMesh(mesh, path, opts);
        EXPECT_FALSE(rep.valid);
        EXPECT_TRUE(hasDiagnostic(rep.diagnostic, MeshExportDiagnostic::InvalidMesh));
        EXPECT_EQ(rep.messages,
                  std::vector<std::string>{"Cannot export: mesh positions contain non-finite values"});
        EXPECT_TRUE(readFile(path).empty());
        std::remove(path.c_str());
    }

    {
        const std::string path = tmpPath("export_inf_uv.ply");
        std::remove(path.c_str());

        Mesh mesh = makePlane(1.f, 1.f, 1u, 1u);
        auto uvs = mesh.attributes().uvs();
        ASSERT_FALSE(uvs.empty());
        uvs[0].u = std::numeric_limits<float>::infinity();
        mesh.attributes().setUVs(std::move(uvs));

        MeshExportOptions opts{};
        opts.format = MeshExportFormat::PLY;
        opts.includeUVs = true;

        const MeshExportReport rep = MeshIO::exportMesh(mesh, path, opts);
        EXPECT_FALSE(rep.valid);
        EXPECT_TRUE(hasDiagnostic(rep.diagnostic, MeshExportDiagnostic::InvalidMesh));
        EXPECT_EQ(rep.messages,
                  std::vector<std::string>{"Cannot export: mesh UVs contain non-finite values"});
        EXPECT_TRUE(readFile(path).empty());
        std::remove(path.c_str());
    }
}

// ─── STL export ──────────────────────────────────────────────────────────────

TEST(MeshIO, ExportSTLProducesBinaryFileWithCorrectSize)
{
    const std::string path = tmpPath("box.stl");
    std::remove(path.c_str());

    Mesh box = makeBox(1.f, 1.f, 1.f);
    MeshExportOptions opts{};
    opts.format = MeshExportFormat::STL;

    const MeshExportReport report = MeshIO::exportMesh(box, path, opts);
    ASSERT_TRUE(report.valid);
    EXPECT_GT(report.facesWritten, 0u);
    EXPECT_EQ(report.verticesWritten, report.facesWritten * 3u);

    // Binary STL size == 80-byte header + 4-byte count + 50 bytes per triangle.
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    ASSERT_TRUE(f.is_open());
    const auto size = static_cast<size_t>(f.tellg());
    EXPECT_EQ(size, 84u + static_cast<size_t>(report.facesWritten) * 50u);

    std::remove(path.c_str());
}

// ─── Import: OBJ ─────────────────────────────────────────────────────────────

TEST(MeshIO, ImportOBJRoundTripsExportedBox)
{
    const std::string path = tmpPath("rt_box.obj");
    std::remove(path.c_str());

    Mesh box = makeBox(1.f, 1.f, 1.f);
    MeshExportOptions eopts{};
    eopts.format         = MeshExportFormat::OBJ;
    eopts.includeNormals = false;
    eopts.includeUVs     = false;
    ASSERT_TRUE(MeshIO::exportMesh(box, path, eopts).valid);

    Mesh loaded;
    MeshImportOptions iopts{};
    const MeshImportReport rep = MeshIO::importMesh(path, iopts, loaded);

    ASSERT_TRUE(rep.valid);
    EXPECT_TRUE(rep.isSuccess());
    EXPECT_EQ(loaded.attributes().vertexCount(), box.attributes().vertexCount());
    EXPECT_EQ(loaded.topology().faceCount(),     box.topology().faceCount());
    EXPECT_EQ(rep.verticesRead, static_cast<uint32_t>(box.attributes().vertexCount()));
    EXPECT_EQ(rep.facesRead,    static_cast<uint32_t>(box.topology().faceCount()));

    std::remove(path.c_str());
}

TEST(MeshIO, ImportOBJHandlesQuadAndNegativeIndices)
{
    const std::string path = tmpPath("quad_neg.obj");
    std::remove(path.c_str());
    {
        std::ofstream f(path);
        // Unit quad; face uses negative (relative) indices referencing the last 4 verts.
        f << "v 0 0 0\n" << "v 1 0 0\n" << "v 1 1 0\n" << "v 0 1 0\n";
        f << "f -4 -3 -2 -1\n";
    }

    Mesh loaded;
    MeshImportOptions iopts{};
    const MeshImportReport rep = MeshIO::importMesh(path, iopts, loaded);

    ASSERT_TRUE(rep.valid);
    EXPECT_EQ(loaded.attributes().vertexCount(), 4u);
    ASSERT_EQ(loaded.topology().faceCount(), 1u);
    EXPECT_EQ(loaded.topology().face(0).indices.size(), 4u);  // preserved as a quad
    EXPECT_EQ(loaded.topology().face(0).indices[0], 0u);
    EXPECT_EQ(loaded.topology().face(0).indices[3], 3u);

    std::remove(path.c_str());
}

TEST(MeshIO, ImportOBJPreservesPerCornerUVs)
{
    const std::string path = tmpPath("uv_corners.obj");
    std::remove(path.c_str());
    {
        std::ofstream f(path);
        f << "v 0 0 0\nv 1 0 0\nv 0 1 0\n";
        f << "vt 0 0\nvt 1 0\nvt 0 1\n";
        f << "f 1/1 2/2 3/3\n";
    }

    Mesh loaded;
    MeshImportOptions iopts{};
    const MeshImportReport rep = MeshIO::importMesh(path, iopts, loaded);

    ASSERT_TRUE(rep.valid);
    EXPECT_EQ(loaded.attributes().vertexCount(), 3u);
    EXPECT_TRUE(loaded.attributes().hasUVs());
    EXPECT_EQ(loaded.attributes().uvs().size(), 3u);

    std::remove(path.c_str());
}

TEST(MeshIO, ImportRejectsOutOfRangeFaceIndex)
{
    const std::string path = tmpPath("oob.obj");
    std::remove(path.c_str());
    {
        std::ofstream f(path);
        f << "v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 9\n";  // vertex 9 does not exist
    }

    Mesh loaded;
    MeshImportOptions iopts{};
    const MeshImportReport rep = MeshIO::importMesh(path, iopts, loaded);

    EXPECT_FALSE(rep.valid);
    EXPECT_TRUE(hasDiagnostic(rep.diagnostic, MeshImportDiagnostic::ParseError));
    EXPECT_EQ(loaded.attributes().vertexCount(), 0u);  // output cleared on failure

    std::remove(path.c_str());
}

TEST(MeshIO, ImportRejectsNonFiniteSTL)
{
    const std::string path = tmpPath("nan.stl");
    std::remove(path.c_str());
    {
        // Craft a 1-triangle binary STL whose first vertex X is a NaN, exercising
        // the non-finite hardening path deterministically (bit-exact, no text parse).
        std::ofstream f(path, std::ios::binary);
        const char header[80] = {0};
        f.write(header, 80);
        auto wu32 = [&](uint32_t v) {
            const char b[4] = {static_cast<char>(v & 0xFF), static_cast<char>((v >> 8) & 0xFF),
                               static_cast<char>((v >> 16) & 0xFF), static_cast<char>((v >> 24) & 0xFF)};
            f.write(b, 4);
        };
        wu32(1);                                    // triangle count
        wu32(0); wu32(0); wu32(0);                  // facet normal 0,0,0
        wu32(0x7FC00000u); wu32(0); wu32(0);        // v0 = (NaN, 0, 0)
        wu32(0x3F800000u); wu32(0); wu32(0);        // v1 = (1, 0, 0)
        wu32(0); wu32(0x3F800000u); wu32(0);        // v2 = (0, 1, 0)
        const char attr[2] = {0, 0};
        f.write(attr, 2);
    }

    Mesh loaded;
    MeshImportOptions iopts{};
    const MeshImportReport rep = MeshIO::importMesh(path, iopts, loaded);

    EXPECT_FALSE(rep.valid);
    EXPECT_TRUE(hasDiagnostic(rep.diagnostic, MeshImportDiagnostic::NonFiniteData));
    EXPECT_EQ(loaded.attributes().vertexCount(), 0u);  // output cleared on failure

    std::remove(path.c_str());
}

// ─── Import: STL ─────────────────────────────────────────────────────────────

TEST(MeshIO, ImportSTLRoundTripsExportedBox)
{
    const std::string path = tmpPath("rt_box.stl");
    std::remove(path.c_str());

    Mesh box = makeBox(1.f, 1.f, 1.f);
    MeshExportOptions eopts{};
    eopts.format = MeshExportFormat::STL;
    const MeshExportReport erep = MeshIO::exportMesh(box, path, eopts);
    ASSERT_TRUE(erep.valid);

    Mesh loaded;
    MeshImportOptions iopts{};  // no weld: STL is triangle soup, 3 verts per tri
    const MeshImportReport rep = MeshIO::importMesh(path, iopts, loaded);

    ASSERT_TRUE(rep.valid);
    EXPECT_EQ(rep.facesRead, erep.facesWritten);
    EXPECT_EQ(rep.verticesRead, erep.facesWritten * 3u);
    EXPECT_TRUE(loaded.attributes().hasNormals());

    std::remove(path.c_str());
}

TEST(MeshIO, ImportSTLWeldReducesVertexCount)
{
    const std::string path = tmpPath("weld_box.stl");
    std::remove(path.c_str());

    Mesh box = makeBox(1.f, 1.f, 1.f);
    MeshExportOptions eopts{};
    eopts.format = MeshExportFormat::STL;
    const MeshExportReport erep = MeshIO::exportMesh(box, path, eopts);
    ASSERT_TRUE(erep.valid);

    Mesh loaded;
    MeshImportOptions iopts{};
    iopts.weldVertices = true;
    const MeshImportReport rep = MeshIO::importMesh(path, iopts, loaded);

    ASSERT_TRUE(rep.valid);
    EXPECT_EQ(rep.facesRead, erep.facesWritten);
    // Welding a closed box collapses the 3-per-tri soup to far fewer shared verts.
    EXPECT_LT(loaded.attributes().vertexCount(), static_cast<size_t>(erep.facesWritten) * 3u);

    std::remove(path.c_str());
}

// ─── Import: dispatch / errors ───────────────────────────────────────────────

TEST(MeshIO, ImportAutoDetectsFormatFromExtension)
{
    Mesh box = makeBox(1.f, 1.f, 1.f);
    for (const auto& fmt : {std::pair{MeshExportFormat::OBJ, std::string(".obj")},
                            std::pair{MeshExportFormat::STL, std::string(".stl")}}) {
        const std::string path = tmpPath("auto" + fmt.second);
        std::remove(path.c_str());
        MeshExportOptions eopts{};
        eopts.format = fmt.first;
        ASSERT_TRUE(MeshIO::exportMesh(box, path, eopts).valid);

        Mesh loaded;
        MeshImportOptions iopts{};  // Auto
        const MeshImportReport rep = MeshIO::importMesh(path, iopts, loaded);
        EXPECT_TRUE(rep.valid) << "format " << fmt.second;
        EXPECT_GT(loaded.topology().faceCount(), 0u);
        std::remove(path.c_str());
    }
}

TEST(MeshIO, ImportRejectsMissingFile)
{
    Mesh loaded;
    MeshImportOptions iopts{};
    const MeshImportReport rep = MeshIO::importMesh(tmpPath("does_not_exist.obj"), iopts, loaded);
    EXPECT_FALSE(rep.valid);
    EXPECT_TRUE(hasDiagnostic(rep.diagnostic, MeshImportDiagnostic::FileOpenFailed));
}

TEST(MeshIO, ImportRejectsUnsupportedExtension)
{
    Mesh loaded;
    MeshImportOptions iopts{};
    const MeshImportReport rep = MeshIO::importMesh("model.xyz", iopts, loaded);
    EXPECT_FALSE(rep.valid);
    EXPECT_TRUE(hasDiagnostic(rep.diagnostic, MeshImportDiagnostic::UnsupportedFormat));
}

TEST(MeshIO, ImportMessagesAreSorted)
{
    Mesh loaded;
    MeshImportOptions iopts{};
    const MeshImportReport rep = MeshIO::importMesh("model.xyz", iopts, loaded);
    auto sorted = rep.messages;
    std::sort(sorted.begin(), sorted.end());
    EXPECT_EQ(rep.messages, sorted);
}

} // namespace nexus::geometry::testing
