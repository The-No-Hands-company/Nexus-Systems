#pragma once
// --- Nexus Geometry — RobustPredicates

#include <nexus/geometry/Mesh.h>
#include <nexus/render/Camera.h>

#include <cstdint>

namespace nexus::geometry {

class RobustPredicates {
public:
    [[nodiscard]] static double orient2D(const Vec2& a, const Vec2& b, const Vec2& c) noexcept;
    [[nodiscard]] static double orient3D(const nexus::render::Vec3& a, const nexus::render::Vec3& b,
                                         const nexus::render::Vec3& c, const nexus::render::Vec3& d) noexcept;
    [[nodiscard]] static double inCircle(const Vec2& a, const Vec2& b, const Vec2& c, const Vec2& d) noexcept;
    // Exact in-sphere test. e is inside the circumsphere of tetrahedron (a,b,c,d) iff the sign
    // of this result matches the sign of orient3D(a,b,c,d) (their product is > 0); 0 is exactly
    // cospherical. Evaluated in exact arithmetic on every call.
    [[nodiscard]] static double inSphere(const nexus::render::Vec3& a, const nexus::render::Vec3& b,
                                         const nexus::render::Vec3& c, const nexus::render::Vec3& d,
                                         const nexus::render::Vec3& e) noexcept;
};

} // namespace nexus::geometry
