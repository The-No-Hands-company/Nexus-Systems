#pragma once
// --- Nexus Geometry — RobustPredicates

#include <nexus/geometry/Mesh.h>
#include <nexus/geometry/Tolerance.h>
#include <nexus/render/Camera.h>

#include <cstdint>

namespace nexus::geometry {

class RobustPredicates {
public:
    // These take the DOUBLE vector types, and the single-precision ones widen into them
    // implicitly — so every existing float call site is unchanged and exact (widening cannot
    // lose anything), while a caller holding double coordinates now gets the exactness the
    // expansion arithmetic was always capable of. The implementation converted to double on
    // entry regardless; all that changes is that it is no longer handed a value that was
    // rounded to float first.
    [[nodiscard]] static double orient2D(const Vec2d& a, const Vec2d& b, const Vec2d& c) noexcept;
    [[nodiscard]] static double orient3D(const Vec3d& a, const Vec3d& b,
                                         const Vec3d& c, const Vec3d& d) noexcept;
    [[nodiscard]] static double inCircle(const Vec2d& a, const Vec2d& b, const Vec2d& c, const Vec2d& d) noexcept;
    // Exact in-sphere test. e is inside the circumsphere of tetrahedron (a,b,c,d) iff the sign
    // of this result matches the sign of orient3D(a,b,c,d) (their product is > 0); 0 is exactly
    // cospherical. Evaluated in exact arithmetic on every call.
    [[nodiscard]] static double inSphere(const Vec3d& a, const Vec3d& b,
                                         const Vec3d& c, const Vec3d& d,
                                         const Vec3d& e) noexcept;
};

} // namespace nexus::geometry
