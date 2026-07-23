#pragma once

#include <nexus/geometry/Mesh.h>

#include <array>
#include <cstdint>

namespace nexus::geometry {

struct MassProperties {
    float volume = 0.f;
    nexus::render::Vec3 centroid = {0.f, 0.f, 0.f};
    float inertia[3][3] = {};
    float principalMoments[3] = {};
    nexus::render::Vec3 principalAxes[3] = {};
};

class MeshMassProperties {
public:
    [[nodiscard]] static MassProperties compute(const Mesh& mesh);

    // The raw boundary integrals the divergence theorem produces, before they are turned
    // into volume/centroid/inertia:
    //   [0]      volume
    //   [1..3]   integral of x, y, z
    //   [4..6]   integral of x^2, y^2, z^2
    //   [7..9]   integral of xy, yz, zx
    //
    // Exposed because they are ADDITIVE over disjoint pieces of a boundary, which lets a
    // caller integrate some faces by a better method and the rest from triangles, then sum.
    // The analytic B-rep does exactly that: it integrates its curved faces exactly over
    // their own parameter domain and takes the flat ones from here, so there is one
    // implementation of the triangle case rather than a copy on each side.
    [[nodiscard]] static std::array<double, 10> integrals(const Mesh& mesh);

    // Turn raw integrals into mass properties. `density` scales the inertia only —
    // volume and centroid are geometric.
    [[nodiscard]] static MassProperties fromIntegrals(const std::array<double, 10>& intg,
                                                      float density = 1.f);
};

} // namespace nexus::geometry
