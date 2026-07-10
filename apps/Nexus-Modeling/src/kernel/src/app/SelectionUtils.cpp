#include <nexus/app/SelectionUtils.h>
#include <nexus/geometry/HalfEdgeMesh.h>
#include <nexus/render/Camera.h>
#include <unordered_set>
#include <queue>

namespace nexus::app {

using Vec3 = nexus::render::Vec3;

std::vector<uint32_t> SelectionUtils::selectEdgeLoop(nexus::geometry::HalfEdgeMesh& mesh, 
                                                   const std::vector<uint32_t>& seedEdges) {
    std::vector<uint32_t> result;
    std::unordered_set<uint32_t> visited;
    std::queue<uint32_t> queue;

    for (uint32_t edge : seedEdges) {
        queue.push(edge);
        visited.insert(edge);
    }

    while (!queue.empty()) {
        uint32_t curr = queue.front();
        queue.pop();
        result.push_back(curr);

        uint32_t v0 = mesh.edge(curr).src;
        uint32_t v1 = mesh.edge(mesh.edge(curr).twin).src;

        auto processVertex = [&](uint32_t v) {
            uint32_t startHe = mesh.vertex(v).edge;
            if (startHe == nexus::geometry::HalfEdgeMesh::kInvalid) return;
            
            uint32_t he = startHe;
            do {
                uint32_t edgeIdx = he; 
                if (visited.find(edgeIdx) == visited.end()) {
                    visited.insert(edgeIdx);
                    queue.push(edgeIdx);
                }
                he = mesh.edge(he).next;
            } while (he != startHe && he != nexus::geometry::HalfEdgeMesh::kInvalid);
        };

        processVertex(v0);
        processVertex(v1);
    }

    return result;
}

std::vector<uint32_t> SelectionUtils::selectEdgeRing(nexus::geometry::HalfEdgeMesh& mesh, 
                                                    const std::vector<uint32_t>& seedEdges) {
    std::vector<uint32_t> result;
    std::unordered_set<uint32_t> visited;
    std::queue<uint32_t> queue;

    for (uint32_t edge : seedEdges) {
        queue.push(edge);
        visited.insert(edge);
    }

    while (!queue.empty()) {
        uint32_t curr = queue.front();
        queue.pop();
        result.push_back(curr);

        uint32_t f0 = mesh.edge(curr).face;
        uint32_t f1 = mesh.edge(mesh.edge(curr).twin).face;

        auto processFace = [&](uint32_t face) {
            if (face == nexus::geometry::HalfEdgeMesh::kInvalid) return;
            uint32_t fe = mesh.face(face).edge;
            uint32_t he = fe;
            do {
                if (he != curr && he != mesh.edge(curr).twin) {
                    if (visited.find(he) == visited.end()) {
                        visited.insert(he);
                        queue.push(he);
                    }
                }
                he = mesh.edge(he).next;
            } while (he != fe && he != nexus::geometry::HalfEdgeMesh::kInvalid);
        };

        processFace(f0);
        processFace(f1);
    }

    return result;
}

std::vector<uint32_t> SelectionUtils::selectSimilarFaces(nexus::geometry::HalfEdgeMesh& mesh, 
                                                       uint32_t seedFace, 
                                                       float tolerance) {
    std::vector<uint32_t> result;
    if (seedFace >= mesh.faceCount()) return result;

    auto calculateArea = [&](uint32_t face) {
        uint32_t fe = mesh.face(face).edge;
        Vec3 a = mesh.positions()[mesh.edge(fe).src];
        Vec3 b = mesh.positions()[mesh.edge(mesh.edge(fe).next).src];
        Vec3 c = mesh.positions()[mesh.edge(mesh.edge(mesh.edge(fe).next).next).src];
        return (b - a).cross(c - a).length() * 0.5f;
    };

    float seedArea = calculateArea(seedFace);
    
    for (uint32_t i = 0; i < mesh.faceCount(); ++i) {
        float area = calculateArea(i);
        if (std::abs(area - seedArea) < tolerance) {
            result.push_back(i);
        }
    }

    return result;
}

} // namespace nexus::app
