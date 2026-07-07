#pragma once

#include <vector>
#include <cmath>
#include <string>
#include <sstream>
#include <array>
#include <map>
#include <set>
#include <algorithm>
#include <limits>
#include <cstdint>
#include <iomanip>

namespace nexus::utility::procgen {

/// Validates procedurally generated meshes for common artifacts
class ProceduralMeshValidator {
public:
    struct Vertex {
        double x, y, z;
        Vertex() : x(0), y(0), z(0) {}
        Vertex(double x_, double y_, double z_) : x(x_), y(y_), z(z_) {}
        Vertex operator-(const Vertex& o) const { return {x - o.x, y - o.y, z - o.z}; }
        Vertex operator+(const Vertex& o) const { return {x + o.x, y + o.y, z + o.z}; }
        Vertex operator*(double s) const { return {x * s, y * s, z * s}; }
        double length() const { return std::sqrt(x*x + y*y + z*z); }
        Vertex cross(const Vertex& o) const {
            return {y * o.z - z * o.y, z * o.x - x * o.z, x * o.y - y * o.x};
        }
        double dot(const Vertex& o) const { return x * o.x + y * o.y + z * o.z; }
        Vertex normalize() const {
            double l = length();
            return l > 1e-12 ? Vertex{x/l, y/l, z/l} : Vertex{};
        }
    };

    struct Triangle {
        uint32_t v0, v1, v2;
    };

    struct MeshValidationResult {
        bool valid = true;
        size_t vertex_count = 0;
        size_t triangle_count = 0;

        size_t degenerate_triangles = 0;    // zero-area or line triangles
        size_t non_manifold_edges = 0;      // edges shared by >2 tris
        size_t boundary_edges = 0;          // edges on mesh boundary
        size_t flipped_normals = 0;         // inconsistent winding
        size_t duplicate_vertices = 0;      // vertices at same position
        size_t zero_length_edges = 0;       // edges with zero length
        size_t isolated_vertices = 0;       // vertices not used by any tri
        size_t self_intersections = 0;      // intersecting triangles

        double min_edge_length = std::numeric_limits<double>::max();
        double max_edge_length = 0;
        double avg_edge_length = 0;
        double min_face_area = std::numeric_limits<double>::max();
        double max_face_area = 0;
        double avg_face_area = 0;

        double bounding_box[6] = {}; // min_x, min_y, min_z, max_x, max_y, max_z
        double surface_area = 0;
        double volume = 0;  // for closed meshes

        std::vector<std::string> warnings;

        bool isWatertight() const {
            return boundary_edges == 0 && non_manifold_edges == 0;
        }
    };

    /// Validate a mesh from vertices and triangle indices
    static MeshValidationResult validate(
        const std::vector<Vertex>& vertices,
        const std::vector<Triangle>& triangles) {

        MeshValidationResult result;
        result.vertex_count = vertices.size();
        result.triangle_count = triangles.size();

        if (vertices.empty() || triangles.empty()) {
            result.valid = false;
            result.warnings.push_back("Empty mesh: no vertices or triangles");
            return result;
        }

        // Check bounding box
        result.bounding_box[0] = result.bounding_box[1] = result.bounding_box[2] =
            std::numeric_limits<double>::max();
        result.bounding_box[3] = result.bounding_box[4] = result.bounding_box[5] =
            std::numeric_limits<double>::lowest();
        for (auto& v : vertices) {
            if (v.x < result.bounding_box[0]) result.bounding_box[0] = v.x;
            if (v.y < result.bounding_box[1]) result.bounding_box[1] = v.y;
            if (v.z < result.bounding_box[2]) result.bounding_box[2] = v.z;
            if (v.x > result.bounding_box[3]) result.bounding_box[3] = v.x;
            if (v.y > result.bounding_box[4]) result.bounding_box[4] = v.y;
            if (v.z > result.bounding_box[5]) result.bounding_box[5] = v.z;
        }

        // Check duplicate vertices
        checkDuplicates(vertices, result);

        // Build edge map and validate triangles
        using Edge = std::pair<uint32_t, uint32_t>;
        std::map<Edge, uint32_t> edge_count;
        std::set<uint32_t> used_vertices;

        double total_edge_len = 0;
        size_t edge_count_total = 0;
        double total_face_area = 0;
        size_t face_count = 0;

        for (const auto& tri : triangles) {
            used_vertices.insert(tri.v0);
            used_vertices.insert(tri.v1);
            used_vertices.insert(tri.v2);

            // Check vertex indices
            if (tri.v0 >= vertices.size() || tri.v1 >= vertices.size() || tri.v2 >= vertices.size()) {
                result.warnings.push_back("Triangle references out-of-bounds vertex");
                result.valid = false;
                continue;
            }

            auto& v0 = vertices[tri.v0];
            auto& v1 = vertices[tri.v1];
            auto& v2 = vertices[tri.v2];

            // Check edge lengths
            double e01 = (v1 - v0).length();
            double e12 = (v2 - v1).length();
            double e20 = (v0 - v2).length();

            if (e01 < 1e-10 || e12 < 1e-10 || e20 < 1e-10) {
                result.zero_length_edges++;
            }

            result.min_edge_length = std::min({result.min_edge_length, e01, e12, e20});
            result.max_edge_length = std::max({result.max_edge_length, e01, e12, e20});
            total_edge_len += e01 + e12 + e20;
            edge_count_total += 3;

            // Check degenerate triangle (collinear vertices)
            auto cross = (v1 - v0).cross(v2 - v0);
            double area = cross.length() * 0.5;
            if (area < 1e-12) {
                result.degenerate_triangles++;
                result.warnings.push_back("Degenerate triangle at indices " +
                    std::to_string(tri.v0) + "," + std::to_string(tri.v1) + "," + std::to_string(tri.v2));
            }

            result.min_face_area = std::min(result.min_face_area, area);
            result.max_face_area = std::max(result.max_face_area, area);
            total_face_area += area;
            face_count++;
            result.surface_area += area;

            // Count edges
            auto addEdge = [&](uint32_t a, uint32_t b) {
                Edge e = {std::min(a, b), std::max(a, b)};
                edge_count[e]++;
            };
            addEdge(tri.v0, tri.v1);
            addEdge(tri.v1, tri.v2);
            addEdge(tri.v2, tri.v0);
        }

        if (edge_count_total > 0) result.avg_edge_length = total_edge_len / edge_count_total;
        if (face_count > 0) result.avg_face_area = total_face_area / face_count;

        // Check edge connectivity
        for (const auto& [edge, count] : edge_count) {
            if (count == 1) result.boundary_edges++;
            if (count > 2) {
                result.non_manifold_edges++;
                result.warnings.push_back("Non-manifold edge: " +
                    std::to_string(edge.first) + "-" + std::to_string(edge.second) +
                    " (shared by " + std::to_string(count) + " faces)");
            }
        }

        // Check isolated vertices
        for (size_t i = 0; i < vertices.size(); ++i) {
            if (used_vertices.find(static_cast<uint32_t>(i)) == used_vertices.end()) {
                result.isolated_vertices++;
            }
        }

        // Compute volume for closed meshes
        if (result.isWatertight()) {
            result.volume = computeVolume(vertices, triangles);
        }

        // Aggregate warnings
        if (result.degenerate_triangles > 0) result.valid = false;
        if (result.non_manifold_edges > 0) result.valid = false;
        if (result.isolated_vertices > 0) result.warnings.push_back(
            std::to_string(result.isolated_vertices) + " isolated vertices found");
        if (result.duplicate_vertices > 0) result.warnings.push_back(
            std::to_string(result.duplicate_vertices) + " duplicate vertices found");

        return result;
    }

    /// Generate a human-readable report
    static std::string report(const MeshValidationResult& r) {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(3);
        oss << "═══ Procedural Mesh Validation Report ═══\n";
        oss << "  Vertices:      " << r.vertex_count << "\n";
        oss << "  Triangles:     " << r.triangle_count << "\n";
        oss << "  Surface Area:  " << r.surface_area << "\n";
        oss << "  Volume:        " << r.volume << (r.isWatertight() ? "" : " (non-watertight)") << "\n";
        oss << "  Bounding Box:  [" << r.bounding_box[0] << "," << r.bounding_box[1] << ","
            << r.bounding_box[2] << "] → [" << r.bounding_box[3] << "," << r.bounding_box[4]
            << "," << r.bounding_box[5] << "]\n\n";

        oss << "  Edge Lengths:  " << r.min_edge_length << " – " << r.max_edge_length
            << " (avg " << r.avg_edge_length << ")\n";
        oss << "  Face Areas:    " << r.min_face_area << " – " << r.max_face_area
            << " (avg " << r.avg_face_area << ")\n\n";

        oss << "  Issues:\n";
        oss << "    Degenerate tris:  " << r.degenerate_triangles
            << (r.degenerate_triangles > 0 ? " ⚠" : " ✓") << "\n";
        oss << "    Non-manifold:     " << r.non_manifold_edges
            << (r.non_manifold_edges > 0 ? " ⚠" : " ✓") << "\n";
        oss << "    Boundary edges:   " << r.boundary_edges
            << (r.boundary_edges > 0 ? "" : " ✓ watertight") << "\n";
        oss << "    Duplicate verts:  " << r.duplicate_vertices
            << (r.duplicate_vertices > 0 ? " ⚠" : " ✓") << "\n";
        oss << "    Isolated verts:   " << r.isolated_vertices
            << (r.isolated_vertices > 0 ? " ⚠" : " ✓") << "\n";
        oss << "    Zero-len edges:   " << r.zero_length_edges
            << (r.zero_length_edges > 0 ? " ⚠" : " ✓") << "\n";

        if (!r.warnings.empty()) {
            oss << "\n  Warnings:\n";
            for (auto& w : r.warnings) oss << "    - " << w << "\n";
        }

        oss << "\n  Verdict: " << (r.valid ? "PASS ✓" : "FAIL ⚠") << "\n";
        return oss.str();
    }

private:
    static void checkDuplicates(const std::vector<Vertex>& verts,
                                  MeshValidationResult& r) {
        const double epsilon = 1e-8;
        for (size_t i = 0; i < verts.size(); ++i) {
            for (size_t j = i + 1; j < verts.size(); ++j) {
                double dx = verts[i].x - verts[j].x;
                double dy = verts[i].y - verts[j].y;
                double dz = verts[i].z - verts[j].z;
                if (dx*dx + dy*dy + dz*dz < epsilon*epsilon) {
                    r.duplicate_vertices++;
                    break; // count each vertex once
                }
            }
        }
    }

    static double computeVolume(const std::vector<Vertex>& verts,
                                 const std::vector<Triangle>& tris) {
        double vol = 0;
        for (auto& tri : tris) {
            auto& v0 = verts[tri.v0];
            auto& v1 = verts[tri.v1];
            auto& v2 = verts[tri.v2];
            vol += v0.x * (v1.y * v2.z - v1.z * v2.y)
                 + v0.y * (v1.z * v2.x - v1.x * v2.z)
                 + v0.z * (v1.x * v2.y - v1.y * v2.x);
        }
        return std::abs(vol) / 6.0;
    }
};

} // namespace nexus::utility::procgen
