// ─────────────────────────────────────────────────────────────────────────────
//  Nexus Render — Temporal Accumulation implementation
// ─────────────────────────────────────────────────────────────────────────────
#include <nexus/render/TemporalAccumulation.h>
#include <bit>
#include <cmath>
#include <cstdint>

namespace nexus::render {

// ─────────────────────────────────────────────────────────────────────────────
//  Halton low-discrepancy sequence helpers
// ─────────────────────────────────────────────────────────────────────────────
namespace {

bool isFiniteFloat(float v) noexcept
{
    constexpr std::uint32_t kExpMask = 0x7F800000u;
    const std::uint32_t bits = std::bit_cast<std::uint32_t>(v);
    return (bits & kExpMask) != kExpMask;
}

bool isValidConfig(const TemporalAccumulationConfig& cfg) noexcept
{
    if (!isFiniteFloat(cfg.blendFactor) || cfg.blendFactor < 0.f || cfg.blendFactor > 1.f)
        return false;
    if (!isFiniteFloat(cfg.velocityRejectionThreshold))
        return false;
    if (!isFiniteFloat(cfg.varianceClipGamma))
        return false;
    if (cfg.jitter.sampleCount == 0u)
        return false;
    return true;
}

[[nodiscard]] float halton(uint32_t index, uint32_t base) noexcept
{
    float f = 1.f;
    float r = 0.f;
    uint32_t i = index + 1;   // 1-based to avoid (0,0) at index 0
    while (i > 0) {
        f /= static_cast<float>(base);
        r += f * static_cast<float>(i % base);
        i /= base;
    }
    return r;
}

} // anonymous namespace

// ─────────────────────────────────────────────────────────────────────────────
//  TemporalAccumulator
// ─────────────────────────────────────────────────────────────────────────────
TemporalAccumulator::TemporalAccumulator(TemporalAccumulationConfig cfg) noexcept
    : m_cfg(isValidConfig(cfg) ? std::move(cfg) : TemporalAccumulationConfig{})
{}

void TemporalAccumulator::setConfig(TemporalAccumulationConfig cfg) noexcept
{
    if (!isValidConfig(cfg)) return;
    m_cfg = std::move(cfg);
}

void TemporalAccumulator::advanceFrame() noexcept
{
    ++m_state.frameIndex;
}

std::pair<float, float> TemporalAccumulator::currentJitterOffset() const noexcept
{
    const uint32_t count  = std::max(1u, m_cfg.jitter.sampleCount);
    const uint32_t slot   = m_state.frameIndex % count;

    if (m_cfg.jitter.type == JitterPattern::kUniform) {
        // Uniform NxN grid (N = sqrt of sampleCount, clamped to 8)
        const uint32_t n = static_cast<uint32_t>(std::sqrt(static_cast<float>(count)));
        const uint32_t nx = n > 0 ? n : 1u;
        const float dx = (static_cast<float>(slot % nx) + 0.5f) / static_cast<float>(nx) - 0.5f;
        const float dy = (static_cast<float>(slot / nx) + 0.5f) / static_cast<float>(nx) - 0.5f;
        return { dx, dy };
    }

    // Halton(2,3) — centred in [−0.5, 0.5]
    return { halton(slot, 2) - 0.5f, halton(slot, 3) - 0.5f };
}

float TemporalAccumulator::resolveBlendAlpha(bool motionRejected) const noexcept
{
    if (motionRejected && m_cfg.enableVelocityRejection) {
        // On motion rejection fall back toward 1.0 (use more current frame)
        return std::min(1.f, m_cfg.blendFactor + (1.f - m_cfg.blendFactor) * 0.5f);
    }
    return m_cfg.blendFactor;
}

} // namespace nexus::render
