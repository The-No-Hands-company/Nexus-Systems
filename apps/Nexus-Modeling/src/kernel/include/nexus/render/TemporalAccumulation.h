#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  Nexus Render — Temporal Accumulation (TAA / TSR)
//
//  Configuration and stateful accumulator for temporal anti-aliasing and
//  denoising integration.  Provides jitter sequences and blend resolution;
//  GPU-side history buffer lifetime is managed by the caller.
// ─────────────────────────────────────────────────────────────────────────────
#include <nexus/gfx/Types.h>
#include <nexus/render/Camera.h>
#include <cstdint>
#include <utility>

namespace nexus::render {

// ── Jitter pattern configuration ─────────────────────────────────────────────
struct JitterPattern {
    static constexpr uint32_t kHalton  = 0;  // Halton(2,3) — low-discrepancy, standard TAA
    static constexpr uint32_t kUniform = 1;  // uniform NxN grid

    uint32_t type        = kHalton;
    uint32_t sampleCount = 8;      // number of positions in the jitter sequence
};

// ── Per-session TAA configuration ────────────────────────────────────────────
struct TemporalAccumulationConfig {
    /// Weight of the *current* frame in the exponential moving average.
    /// 0.0 = keep all history, 1.0 = replace with current frame.
    float blendFactor = 0.1f;

    bool  enableVelocityRejection          = true;
    float velocityRejectionThreshold       = 0.05f;  // NDC units

    bool  enableNeighborhoodClamping       = true;
    float varianceClipGamma                = 1.0f;   // variance clip radius multiplier

    JitterPattern jitter;
};

// ── Per-frame accumulation state ──────────────────────────────────────────────
struct TemporalAccumulationState {
    /// GPU history colour buffer; null handle on the first frame.
    nexus::gfx::TextureHandle historyBuffer {};
    uint32_t                  frameIndex    = 0;

    /// Previous frame view/proj matrices for velocity-based reprojection.
    nexus::render::Mat4 prevView {};
    nexus::render::Mat4 prevProj {};
};

// ── Temporal accumulator ──────────────────────────────────────────────────────
class TemporalAccumulator {
public:
    explicit TemporalAccumulator(TemporalAccumulationConfig cfg = {}) noexcept;

    /// Advance to the next frame: increments frameIndex, updates jitter slot.
    void advanceFrame() noexcept;

    /// Sub-pixel jitter offset in NDC space for the current frame.
    [[nodiscard]] std::pair<float, float> currentJitterOffset() const noexcept;

    /// Blend alpha for resolve pass.
    /// @param motionRejected  true when per-pixel velocity exceeds the rejection threshold.
    [[nodiscard]] float resolveBlendAlpha(bool motionRejected = false) const noexcept;

    [[nodiscard]] const TemporalAccumulationConfig& config() const noexcept { return m_cfg;   }
    [[nodiscard]] const TemporalAccumulationState&  state()  const noexcept { return m_state; }
          TemporalAccumulationState&                state()        noexcept { return m_state; }

    void setConfig(TemporalAccumulationConfig cfg) noexcept;

private:
    TemporalAccumulationConfig m_cfg;
    TemporalAccumulationState  m_state;
};

} // namespace nexus::render
