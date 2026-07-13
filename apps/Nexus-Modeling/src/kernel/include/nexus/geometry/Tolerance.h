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
[[nodiscard]] constexpr bool coincident(const nexus::render::Vec3& a,
                                        const nexus::render::Vec3& b,
                                        const Tolerance& tol = {}) noexcept
{
    const float dx = a.x - b.x, dy = a.y - b.y, dz = a.z - b.z;
    const float d2 = dx * dx + dy * dy + dz * dz;
    const float t = tol.absolute;
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
