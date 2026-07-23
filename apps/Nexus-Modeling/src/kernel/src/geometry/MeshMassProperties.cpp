#include <nexus/geometry/MeshMassProperties.h>
#include "EigenSolver3x3.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace nexus::geometry {

using Vec3 = nexus::render::Vec3;

// Exact mass properties of a closed triangle mesh by the divergence theorem — Eberly's
// "Polyhedral Mass Properties" surface integrals.
//
// The previous implementation produced correct volume and centroid but an inertia tensor
// that was simply wrong: a centred 2x3x4 box, whose moments are 50, 40 and 26 about its
// axes with zero products, reported -10, -8 and -5.2 with products of -22.8, -45.6 and
// -30.4. NEGATIVE diagonal moments are not an approximation error, they are impossible —
// a moment of inertia integrates a squared distance against a positive mass. The
// tetrahedron weight it accumulated with was the NEGATION of the signed volume, which
// `fabs` hid in the volume result and nothing corrected in the moments; the quadratic
// accumulations and their 1/20 divisor matched no consistent formulation either.
//
// It had been wrong long enough that the analytic B-rep, needing the same quantities,
// worked around it by integrating its own. So this is not a new formulation — it is the
// one already proven exact there, brought to the mesh side so the two agree and neither
// has to route around the other.
std::array<double, 10> MeshMassProperties::integrals(const Mesh& mesh) {
    // volume, then the integrals of x y z, x^2 y^2 z^2, xy yz zx (un-normalised).
    std::array<double, 10> intg{0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    if (!mesh.isValid()) return intg;

    const auto& topo = mesh.topology();
    const auto& pos = mesh.attributes().positions();
    if (pos.empty()) return intg;

    auto sub = [](double w0, double w1, double w2, double& f1, double& f2, double& f3,
                  double& g0, double& g1, double& g2) {
        const double t0 = w0 + w1, t1 = w0 * w0, t2 = t1 + w1 * t0;
        f1 = t0 + w2;
        f2 = t2 + w2 * f1;
        f3 = w0 * t1 + w1 * t2 + w2 * f2;
        g0 = f2 + w0 * (f1 + w0);
        g1 = f2 + w1 * (f1 + w1);
        g2 = f2 + w2 * (f1 + w2);
    };

    for (size_t fi = 0; fi < topo.faceCount(); ++fi) {
        const Face& face = topo.face(fi);
        if (!face.indicesInBounds(pos.size())) continue;
        if (face.indices.size() < 3) continue;

        // Fan-triangulate n-gons; a boundary integral is additive over the pieces.
        for (size_t vi = 0; vi + 2 < face.indices.size(); ++vi) {
            const Vec3 p0 = pos[face.indices[0]];
            const Vec3 p1 = pos[face.indices[vi + 1]];
            const Vec3 p2 = pos[face.indices[vi + 2]];

            const double x0 = p0.x, y0 = p0.y, z0 = p0.z;
            const double x1 = p1.x, y1 = p1.y, z1 = p1.z;
            const double x2 = p2.x, y2 = p2.y, z2 = p2.z;
            const double a1 = x1 - x0, b1 = y1 - y0, c1 = z1 - z0;
            const double a2 = x2 - x0, b2 = y2 - y0, c2 = z2 - z0;
            const double d0 = b1 * c2 - b2 * c1;  // (p1-p0) x (p2-p0), un-normalised normal
            const double d1 = a2 * c1 - a1 * c2;
            const double d2 = a1 * b2 - a2 * b1;

            double f1x, f2x, f3x, g0x, g1x, g2x;
            double f1y, f2y, f3y, g0y, g1y, g2y;
            double f1z, f2z, f3z, g0z, g1z, g2z;
            sub(x0, x1, x2, f1x, f2x, f3x, g0x, g1x, g2x);
            sub(y0, y1, y2, f1y, f2y, f3y, g0y, g1y, g2y);
            sub(z0, z1, z2, f1z, f2z, f3z, g0z, g1z, g2z);

            intg[0] += d0 * f1x;
            intg[1] += d0 * f2x;
            intg[2] += d1 * f2y;
            intg[3] += d2 * f2z;
            intg[4] += d0 * f3x;
            intg[5] += d1 * f3y;
            intg[6] += d2 * f3z;
            intg[7] += d0 * (y0 * g0x + y1 * g1x + y2 * g2x);
            intg[8] += d1 * (z0 * g0y + z1 * g1y + z2 * g2y);
            intg[9] += d2 * (x0 * g0z + x1 * g1z + x2 * g2z);
        }
    }

    static constexpr double mult[10] = {1.0 / 6,  1.0 / 24, 1.0 / 24,  1.0 / 24,  1.0 / 60,
                                        1.0 / 60, 1.0 / 60, 1.0 / 120, 1.0 / 120, 1.0 / 120};
    for (int i = 0; i < 10; ++i) intg[i] *= mult[i];
    return intg;
}

MassProperties MeshMassProperties::fromIntegrals(const std::array<double, 10>& raw,
                                                 float density) {
    MassProperties result;
    std::array<double, 10> intg = raw;

    // An inward-wound boundary integrates to a negative volume. Negating every integral is
    // exactly equivalent to reversing the winding, so the moments stay correct either way
    // rather than silently inheriting the sign.
    double vol = intg[0];
    if (vol < 0.0) {
        for (int i = 0; i < 10; ++i) intg[i] = -intg[i];
        vol = -vol;
    }
    if (vol < 1e-12) return result;  // open, degenerate, or zero-volume

    result.volume = static_cast<float>(vol);
    const double cx = intg[1] / vol, cy = intg[2] / vol, cz = intg[3] / vol;
    result.centroid = {static_cast<float>(cx), static_cast<float>(cy), static_cast<float>(cz)};

    // Inertia about the CENTROID (parallel-axis applied to the origin-referenced integrals).
    const double Ixx = intg[5] + intg[6] - vol * (cy * cy + cz * cz);
    const double Iyy = intg[4] + intg[6] - vol * (cz * cz + cx * cx);
    const double Izz = intg[4] + intg[5] - vol * (cx * cx + cy * cy);
    const double Ixy = -(intg[7] - vol * cx * cy);
    const double Iyz = -(intg[8] - vol * cy * cz);
    const double Ixz = -(intg[9] - vol * cz * cx);

    const double d = static_cast<double>(density);
    result.inertia[0][0] = static_cast<float>(d * Ixx);
    result.inertia[1][1] = static_cast<float>(d * Iyy);
    result.inertia[2][2] = static_cast<float>(d * Izz);
    result.inertia[0][1] = result.inertia[1][0] = static_cast<float>(d * Ixy);
    result.inertia[1][2] = result.inertia[2][1] = static_cast<float>(d * Iyz);
    result.inertia[0][2] = result.inertia[2][0] = static_cast<float>(d * Ixz);

    const float mPacked[6] = {
        result.inertia[0][0], result.inertia[1][1], result.inertia[2][2],
        result.inertia[0][1], result.inertia[0][2], result.inertia[1][2]
    };
    nexus::geometry::internal::Eigen3x3 eig =
        nexus::geometry::internal::solveEigenSymmetric3x3(mPacked);

    int order[3] = {0, 1, 2};
    std::sort(order, order + 3, [&](int a, int b) { return eig.val[a] > eig.val[b]; });

    for (int i = 0; i < 3; ++i) {
        result.principalMoments[i] = eig.val[order[i]];
        result.principalAxes[i] = eig.vec[order[i]];
    }

    return result;
}

MassProperties MeshMassProperties::compute(const Mesh& mesh) {
    return fromIntegrals(integrals(mesh), 1.f);
}

} // namespace nexus::geometry
