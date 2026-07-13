// Foundation: a central scale/unit-aware tolerance replacing scattered
// scale-blind epsilons. The defining property is that the *same* predicate
// behaves correctly in proportion whether the model is a 0.5 mm part or a 5 km
// terrain — proven below at both extremes.

#include <nexus/geometry/Tolerance.h>

#include <gtest/gtest.h>

#include <bit>
#include <cstdint>

namespace nexus::geometry::testing {

using nexus::render::Vec3;

// Non-finite detection must survive -ffast-math. Construct NaN/±Inf via bit
// patterns (not 0.0/0.0, which the optimizer may fold) so the test genuinely
// exercises the bit-inspection path and guards against a regression to
// std::isfinite (which -ffast-math renders unreliable).
TEST(Tolerance, IsFiniteRejectsNonFiniteUnderFastMath)
{
    const float qnan = std::bit_cast<float>(0x7FC00000u);
    const float posInf = std::bit_cast<float>(0x7F800000u);
    const float negInf = std::bit_cast<float>(0xFF800000u);
    EXPECT_FALSE(isFinite(qnan));
    EXPECT_FALSE(isFinite(posInf));
    EXPECT_FALSE(isFinite(negInf));

    EXPECT_TRUE(isFinite(0.f));
    EXPECT_TRUE(isFinite(-0.f));
    EXPECT_TRUE(isFinite(1e30f));
    EXPECT_TRUE(isFinite(-1e-30f));
    EXPECT_TRUE(isFinite(std::bit_cast<float>(0x00000001u)));  // smallest denormal

    EXPECT_TRUE(isFinite(Vec3{1.f, 2.f, 3.f}));
    EXPECT_FALSE(isFinite(Vec3{1.f, qnan, 3.f}));
    EXPECT_FALSE(isFinite(Vec3{posInf, 2.f, 3.f}));
}

TEST(Tolerance, DefaultsAreConfusionScale)
{
    constexpr Tolerance t;
    EXPECT_FLOAT_EQ(t.absolute, Tolerance::kDefaultAbsolute);
    EXPECT_FLOAT_EQ(t.relative, Tolerance::kDefaultRelative);
}

TEST(Tolerance, NearlyEqualUsesAbsoluteFloorNearZero)
{
    constexpr Tolerance t;  // absolute 1e-5
    EXPECT_TRUE(t.nearlyEqual(0.f, 5e-6f));
    EXPECT_FALSE(t.nearlyEqual(0.f, 5e-5f));
}

TEST(Tolerance, NearlyEqualScalesWithMagnitude)
{
    constexpr Tolerance t;  // relative 1e-6
    // At magnitude 1e6 the relative term (1.0) dominates the 1e-5 floor.
    EXPECT_TRUE(t.nearlyEqual(1'000'000.0f, 1'000'000.5f));
    EXPECT_FALSE(t.nearlyEqual(1'000'000.0f, 1'000'002.0f));
    // The same absolute gap that is "equal" at large magnitude is "different"
    // near zero — that's the whole point of the relative term.
    EXPECT_FALSE(t.nearlyEqual(0.f, 0.5f));
}

TEST(Tolerance, IsZeroUsesAbsoluteOnly)
{
    constexpr Tolerance t;
    EXPECT_TRUE(t.isZero(9e-6f));
    EXPECT_TRUE(t.isZero(-9e-6f));
    EXPECT_FALSE(t.isZero(2e-5f));
}

TEST(Tolerance, AtReturnsMaxOfAbsoluteAndRelative)
{
    constexpr Tolerance t{1e-5f, 1e-3f};
    EXPECT_FLOAT_EQ(t.at(0.f), 1e-5f);      // absolute floor wins
    EXPECT_FLOAT_EQ(t.at(1.f), 1e-3f);      // relative wins
    EXPECT_FLOAT_EQ(t.at(-1000.f), 1.f);    // magnitude is |.|
}

// The headline property: forCharacteristicLength proportions the floor to the
// model size so coincidence behaves identically in proportion at any scale.
TEST(Tolerance, ScaleAwareCoincidenceHoldsAcrossSevenOrdersOfMagnitude)
{
    struct Case { float length; const char* name; };
    for (const Case c : {Case{5e-4f, "0.5mm part"}, Case{5e3f, "5km terrain"}}) {
        const Tolerance tol = Tolerance::forCharacteristicLength(c.length);
        const float L = c.length;

        // Two points a hair apart *relative to the model* must coincide…
        const Vec3 a{L, -L, L};
        const Vec3 near{L + tol.absolute * 0.5f, -L, L};
        EXPECT_TRUE(coincident(a, near, tol)) << c.name << ": near-coincident missed";

        // …and two points a meaningful fraction of the model apart must not.
        const Vec3 far{L + L * 1e-3f, -L, L};
        EXPECT_FALSE(coincident(a, far, tol)) << c.name << ": distinct points merged";
    }
}

// A fixed absolute epsilon (the status quo) fails one of the two scales: prove
// the module is not just re-expressing a constant.
TEST(Tolerance, FixedEpsilonWouldFailOneScaleButToleranceDoesNot)
{
    constexpr float legacyEps = 1e-5f;  // a typical scattered literal
    const Tolerance small = Tolerance::forCharacteristicLength(5e-4f);   // 0.5 mm
    const Tolerance large = Tolerance::forCharacteristicLength(5e3f);    // 5 km

    // On the tiny part, 1e-5 is HUGE — it would merge genuinely distinct
    // features (1e-6 apart). The scaled tolerance keeps them distinct.
    const Vec3 p{0.f, 0.f, 0.f};
    const Vec3 q{1e-6f, 0.f, 0.f};
    EXPECT_LT(distanceSquared(p, q), legacyEps * legacyEps);  // legacy merges them
    EXPECT_FALSE(coincident(p, q, small));                    // module does not

    // On the huge terrain, 1e-5 is absurdly tight — float noise at km scale far
    // exceeds it, so real coincidences are missed. The scaled tolerance sees them.
    const Vec3 r{5e3f, 5e3f, 5e3f};
    const Vec3 s{5e3f + 1e-3f, 5e3f, 5e3f};
    EXPECT_GT(distanceSquared(r, s), legacyEps * legacyEps);  // legacy splits them
    EXPECT_TRUE(coincident(r, s, large));                     // module merges them
}

}  // namespace nexus::geometry::testing
