#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  Nexus Render — FrameCaptureExporter
//
//  Collects per-frame metadata for debugging and CI artifact production.
//
//  A `FramePassEntry` records one render pass within a frame.
//  A `FrameCaptureSnapshot` aggregates all entries for one frame.
//  `IFrameCaptureExporter` is the abstract hook wired into Renderer.
//  `NullFrameCaptureExporter` is the headless/CI implementation.
//
//  Usage:
//
//      NullFrameCaptureExporter exporter;
//      renderer.setFrameCaptureExporter(&exporter);
//
//      // ... render loop ...
//
//      const auto& snap = exporter.lastSnapshot();
//      ASSERT_EQ(snap.frameIndex, 1u);
//      ASSERT_EQ(snap.passes.size(), 3u);   // shadow + geometry + composite
//
// ─────────────────────────────────────────────────────────────────────────────
#include <nexus/render/RenderGraphValidator.h>
#include <cstdint>
#include <string>
#include <vector>

namespace nexus::render {

// ── Per-pass entry recorded during a frame ────────────────────────────────────
struct FramePassEntry {
    RenderPassType            passType;
    uint32_t                  drawCalls    = 0;
    uint32_t                  triangles    = 0;
    nexus::gfx::TextureLayout gbufferColorLayout = nexus::gfx::TextureLayout::Undefined;
    nexus::gfx::TextureLayout shadowAtlasLayout  = nexus::gfx::TextureLayout::Undefined;
    // Monotonic CPU timestamp in nanoseconds (0 on Null backend).
    uint64_t                  cpuTimestampNs = 0;
};

// ── Snapshot of a complete captured frame ─────────────────────────────────────
struct FrameCaptureSnapshot {
    uint64_t                   frameIndex  = 0;
    std::vector<FramePassEntry> passes;
    // Cumulative totals for the frame.
    uint32_t                   totalDrawCalls = 0;
    uint32_t                   totalTriangles = 0;
};

// ── Abstract exporter interface ───────────────────────────────────────────────
class IFrameCaptureExporter {
public:
    virtual ~IFrameCaptureExporter() = default;

    // Called by Renderer at the start of each frame.
    virtual void beginCapture(uint64_t frameIndex) = 0;

    // Called by Renderer once per pass during render().
    virtual void recordPass(const FramePassEntry& entry) = 0;

    // Called by Renderer at endFrame().  Finalises the current snapshot.
    virtual void endCapture() = 0;
};

// ── Null (headless) implementation ───────────────────────────────────────────
class NullFrameCaptureExporter final : public IFrameCaptureExporter {
public:
    void beginCapture(uint64_t frameIndex) override;
    void recordPass(const FramePassEntry& entry) override;
    void endCapture() override;

    // Returns the most recently completed snapshot.
    // The snapshot is only valid after at least one endCapture() call.
    [[nodiscard]] const FrameCaptureSnapshot& lastSnapshot() const noexcept
    {
        return m_completed;
    }

    // Returns true if at least one complete frame has been exported.
    [[nodiscard]] bool hasSnapshot() const noexcept { return m_hasSnapshot; }

    // Total number of frames exported since construction.
    [[nodiscard]] uint64_t totalFramesCaptured() const noexcept { return m_totalFrames; }

private:
    FrameCaptureSnapshot m_inProgress;
    FrameCaptureSnapshot m_completed;
    bool                 m_hasSnapshot   = false;
    bool                 m_capturing     = false;
    uint64_t             m_totalFrames   = 0;
};

} // namespace nexus::render
