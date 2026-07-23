#pragma once

#include <cstdint>
#include <limits>

namespace nexus::geometry::internal {

class SplitMix64 {
public:
    explicit constexpr SplitMix64(uint64_t seed) noexcept
        : m_state(seed) {}

    [[nodiscard]] uint64_t next() noexcept {
        m_state += 0x9E3779B97F4A7C15ULL;
        uint64_t z = m_state;
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
        return z ^ (z >> 31);
    }

    // Uniform in [0, 1).
    //
    // The shift discards the low 11 bits to leave a 53-bit integer — exactly a double's
    // mantissa — so the scale factor must be 2^-53. Dividing by 2^64 instead, as this did,
    // returned values in [0, 2^-11]: never more than 0.000488, a constant for every
    // practical purpose. Everything drawing "random" numbers here was drawing ~0, which
    // left Poisson-disk sampling throwing every dart in the same direction at the same
    // distance and returning a single point.
    [[nodiscard]] double uniform01() noexcept {
        constexpr double kInv53 = 1.0 / static_cast<double>(1ULL << 53);
        return static_cast<double>(next() >> 11) * kInv53;
    }

private:
    uint64_t m_state;
};

} // namespace nexus::geometry::internal
