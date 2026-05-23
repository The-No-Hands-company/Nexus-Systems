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

} // namespace nexus::geometry::testing
