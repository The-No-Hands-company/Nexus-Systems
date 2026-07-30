#pragma once
// --- Nexus Geometry — Tolerance
//
//  A single scale/unit-aware tolerance the geometry ops consult, replacing the
//  scattered scale-blind magic epsilons (1e-10, 1e-8, 1e-12, …) that made the
//  same operation behave differently on a 0.5 mm part and a 5 km terrain.
//
//  A Tolerance combines an *absolute* floor (in model units) with a *relative*
//  fraction of the operands' magnitude. The effective linear tolerance for
//  comparing a magnitude m is  max(absolute, relative * |m|).  Scalar
//  comparisons (dot products, determinants, parameters) benefit most from the
//  relative term; point coincidence is an absolute-distance test whose floor
//  should be sized to the model via forCharacteristicLength().
//
//  Design note (-ffast-math is enabled): all helpers are branchless-ternary
//  constexpr and use squared distances / manual fabs, so they neither depend on
//  std::fabs/std::sqrt lowering nor on non-finite semantics the fast-math flag
//  makes unreliable. Callers at public API boundaries reject non-finite inputs
//  before reaching these predicates.

#include <nexus/render/Camera.h>  // nexus::render::Vec3

#include <bit>
#include <cmath>
#include <cstdint>

namespace nexus::geometry {

// Canonical non-finite check for the geometry kernel. `-ffast-math` (enabled
// project-wide) lets the compiler assume operands are finite, so std::isfinite /
// std::isnan are unreliable — detect NaN and ±Inf by inspecting the IEEE-754
// exponent field directly (all-ones exponent ⇒ non-finite). Public API entry
// points use this to reject non-finite float inputs.
[[nodiscard]] constexpr bool isFinite(float v) noexcept
{
    return (std::bit_cast<std::uint32_t>(v) & 0x7F800000u) != 0x7F800000u;
}
[[nodiscard]] constexpr bool isFinite(const nexus::render::Vec3& v) noexcept
{
    return isFinite(v.x) && isFinite(v.y) && isFinite(v.z);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Vec3d — double-precision position, for the analytic B-rep
//
//  `render::Vec3` is single precision because it is the type the GPU consumes: a mesh
//  vertex reaches a vertex buffer without conversion, at half the memory of the largest
//  arrays in the system. That is the right trade for a mesh and the wrong one for an
//  analytic solid. Single precision gives about six usable decimal digits, so a part a
//  kilometre across resolves to roughly 0.06 mm and cannot carry a micron feature at all —
//  and every industrial kernel this one is measured against (Parasolid, ACIS, OCCT, Rhino)
//  is double throughout for exactly that reason.
//
//  Computing in double and storing in float was measured before being rejected: against a
//  long-double reference over 200,000 circle evaluations it buys 1.2x at unit scale, 1.4x
//  at 100, and 1.6x at 10,000 — under 2x, because rounding the RESULT to float is the
//  floor. The ceiling is the storage, not the arithmetic.
//
//  Conversion from `render::Vec3` is implicit because it is widening and cannot lose
//  anything, which keeps every existing call site and every `{1.f, 2.f, 3.f}` literal
//  compiling. Conversion back is `toFloat()` and deliberately explicit: it is the narrowing
//  direction, it belongs at the render boundary, and it should be visible when it happens.
struct Vec3d {
    double x = 0.0, y = 0.0, z = 0.0;

    constexpr Vec3d() noexcept = default;
    constexpr Vec3d(double px, double py, double pz) noexcept : x(px), y(py), z(pz) {}
    constexpr Vec3d(const nexus::render::Vec3& v) noexcept : x(v.x), y(v.y), z(v.z) {}

    [[nodiscard]] constexpr nexus::render::Vec3 toFloat() const noexcept
    {
        return {static_cast<float>(x), static_cast<float>(y), static_cast<float>(z)};
    }

    [[nodiscard]] constexpr Vec3d operator+(const Vec3d& o) const noexcept { return {x+o.x, y+o.y, z+o.z}; }
    [[nodiscard]] constexpr Vec3d operator-(const Vec3d& o) const noexcept { return {x-o.x, y-o.y, z-o.z}; }
    [[nodiscard]] constexpr Vec3d operator*(double s)       const noexcept { return {x*s, y*s, z*s}; }
    [[nodiscard]] constexpr Vec3d operator/(double s)       const noexcept { return {x/s, y/s, z/s}; }
    [[nodiscard]] constexpr Vec3d operator-()               const noexcept { return {-x, -y, -z}; }
    constexpr Vec3d& operator+=(const Vec3d& o) noexcept { x+=o.x; y+=o.y; z+=o.z; return *this; }
    constexpr Vec3d& operator-=(const Vec3d& o) noexcept { x-=o.x; y-=o.y; z-=o.z; return *this; }
    constexpr Vec3d& operator*=(double s)       noexcept { x*=s; y*=s; z*=s; return *this; }

    [[nodiscard]] constexpr double dot(const Vec3d& o) const noexcept { return x*o.x + y*o.y + z*o.z; }
    [[nodiscard]] constexpr Vec3d cross(const Vec3d& o) const noexcept
    {
        return {y*o.z - z*o.y, z*o.x - x*o.z, x*o.y - y*o.x};
    }
    [[nodiscard]] constexpr double lengthSq() const noexcept { return dot(*this); }
    [[nodiscard]] double length() const noexcept { return std::sqrt(lengthSq()); }
    [[nodiscard]] Vec3d normalize() const noexcept
    {
        const double l = length();
        return (l > 1e-300) ? (*this * (1.0 / l)) : Vec3d{0.0, 0.0, 1.0};
    }
    bool operator==(const Vec3d&) const = default;
};
[[nodiscard]] inline constexpr Vec3d operator*(double s, const Vec3d& v) noexcept { return v * s; }

// The 2D companion, for the exact predicates. Its converting constructor is a template so
// this header does not have to include Mesh.h (where the float Vec2 lives) merely to accept
// one — anything with `.u` and `.v` widens into it, which is the same implicit-widening rule
// Vec3d follows.
struct Vec2d {
    double u = 0.0, v = 0.0;

    constexpr Vec2d() noexcept = default;
    constexpr Vec2d(double pu, double pv) noexcept : u(pu), v(pv) {}
    template <class T>
        requires requires(const T& t) { t.u; t.v; }
    constexpr Vec2d(const T& p) noexcept : u(p.u), v(p.v) {}

    bool operator==(const Vec2d&) const = default;
};

// Non-finite check for a double, by the same exponent-bit inspection the float overload
// uses — see the note above isFinite(float).
[[nodiscard]] constexpr bool isFinite(double v) noexcept
{
    return (std::bit_cast<std::uint64_t>(v) & 0x7FF0000000000000ull) != 0x7FF0000000000000ull;
}
[[nodiscard]] constexpr bool isFinite(const Vec3d& v) noexcept
{
    return isFinite(v.x) && isFinite(v.y) && isFinite(v.z);
}

struct Tolerance {
    // Absolute floor, in model units. Two magnitudes closer than this are
    // considered equal regardless of their size.
    float absolute = kDefaultAbsolute;
    // Relative term: fraction of the compared magnitude that also counts as
    // "equal". Keeps predicates stable as coordinates grow large.
    float relative = kDefaultRelative;

    // Defaults are in the spirit of a CAD "confusion" tolerance for unit-scale
    // (~1 model unit) geometry. Rescale for very small / very large models with
    // forCharacteristicLength().
    static constexpr float kDefaultAbsolute = 1e-5f;
    static constexpr float kDefaultRelative = 1e-6f;

    constexpr Tolerance() noexcept = default;
    constexpr Tolerance(float abs, float rel) noexcept : absolute(abs), relative(rel) {}

    // Tolerance proportioned to a model whose characteristic size is `length`
    // (e.g. its bounding-box diagonal). The absolute floor becomes
    // relative * length, so every predicate behaves the same *in proportion* at
    // any scale: a 0.5 mm part and a 5 km terrain get floors of 5e-10 m and
    // 5e-3 m respectively with the default relative term.
    [[nodiscard]] static constexpr Tolerance forCharacteristicLength(
        float length, float rel = kDefaultRelative) noexcept
    {
        const float mag = (length < 0.f ? -length : length);
        const float floor = rel * (mag > 0.f ? mag : 1.f);
        return Tolerance{floor, rel};
    }

    // Effective linear tolerance at a given characteristic magnitude.
    [[nodiscard]] constexpr float at(float magnitude) const noexcept
    {
        const float m = (magnitude < 0.f ? -magnitude : magnitude);
        const float r = relative * m;
        return (r > absolute ? r : absolute);
    }

    // |a - b| within the effective tolerance at max(|a|,|b|).
    [[nodiscard]] constexpr bool nearlyEqual(float a, float b) const noexcept
    {
        float d = a - b;
        d = (d < 0.f ? -d : d);
        const float ma = (a < 0.f ? -a : a);
        const float mb = (b < 0.f ? -b : b);
        return d <= at(ma > mb ? ma : mb);
    }

    // |v| within the absolute floor. Zero has no magnitude to scale against, so
    // only the absolute term applies.
    [[nodiscard]] constexpr bool isZero(float v) const noexcept
    {
        const float m = (v < 0.f ? -v : v);
        return m <= absolute;
    }
};

// Two points coincide when their separation is within the absolute floor.
// Uses squared distance to avoid a sqrt (and its fast-math approximation).
// Takes the double vector, which the single-precision one widens into implicitly, so both
// callers are served by one implementation and neither loses anything to the conversion.
[[nodiscard]] constexpr bool coincident(const Vec3d& a, const Vec3d& b,
                                        const Tolerance& tol = {}) noexcept
{
    const double dx = a.x - b.x, dy = a.y - b.y, dz = a.z - b.z;
    const double d2 = dx * dx + dy * dy + dz * dz;
    const double t = tol.absolute;
    return d2 <= t * t;
}

// Squared separation, exposed for callers that already batch distance work.
[[nodiscard]] constexpr float distanceSquared(const nexus::render::Vec3& a,
                                              const nexus::render::Vec3& b) noexcept
{
    const float dx = a.x - b.x, dy = a.y - b.y, dz = a.z - b.z;
    return dx * dx + dy * dy + dz * dz;
}

}  // namespace nexus::geometry
