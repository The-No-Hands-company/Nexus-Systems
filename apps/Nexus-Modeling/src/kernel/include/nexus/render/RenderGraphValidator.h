#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  Nexus Render — RenderGraphValidator
//
//  Validates render pass ordering and resource-state transition correctness
//  against the kernel's deferred rendering pipeline contract.
//
//  Rules enforced:
//    1. PassOrder — shadow passes must precede geometry passes; geometry passes
//       must precede composite passes.
//    2. GBufferState — GBuffer color targets must be in ColorAttachment before
//       the geometry pass and in ShaderRead before the composite pass.
//    3. ShadowAtlasState — shadow atlas must be DepthWrite during shadow passes
//       and DepthRead before lighting/composite sampling.
//
//  Usage:
//
//      RenderGraphDesc desc;
//      desc.record(RenderPassType::Shadow,    shadowAtlasLayout);
//      desc.record(RenderPassType::Geometry,  gbufferLayout);
//      desc.record(RenderPassType::Composite, gbufferLayout);
//
//      auto report = RenderGraphValidator::validate(desc);
//      if (!report.valid) { for (auto& e : report.issues) { ... } }
//
// ─────────────────────────────────────────────────────────────────────────────
#include <nexus/gfx/Types.h>
#include <string>
#include <vector>

namespace nexus::render {

// ── Pass types ────────────────────────────────────────────────────────────────
enum class RenderPassType : uint8_t {
    Shadow,     // depth-only shadow atlas rendering
    Geometry,   // GBuffer deferred geometry pass
    Composite,  // lighting/composite pass reading GBuffer + shadow
    RayTracing, // optional ray tracing pass reading GBuffer / shadow inputs
};

// ── State snapshot for a single pass ─────────────────────────────────────────
struct RenderPassStateSnapshot {
    RenderPassType          passType;
    nexus::gfx::TextureLayout gbufferColorLayout = nexus::gfx::TextureLayout::Undefined;
    nexus::gfx::TextureLayout shadowAtlasLayout  = nexus::gfx::TextureLayout::Undefined;
};

// ── Render graph description (ordered sequence of passes) ─────────────────────
struct RenderGraphDesc {
    std::vector<RenderPassStateSnapshot> passes;

    // Append a pass with its resource state snapshots.
    void record(RenderPassType type,
                nexus::gfx::TextureLayout gbufferColorLayout,
                nexus::gfx::TextureLayout shadowAtlasLayout =
                    nexus::gfx::TextureLayout::Undefined)
    {
        passes.push_back({type, gbufferColorLayout, shadowAtlasLayout});
    }
};

// ── Validation issue codes ────────────────────────────────────────────────────
enum class RenderGraphIssueCode : uint8_t {
    // Pass ordering violations
    GeometryBeforeShadow,    // geometry pass recorded before any shadow pass
    CompositeBeforeGeometry, // composite pass recorded before any geometry pass
    CompositeBeforeShadow,   // composite pass recorded before any shadow pass

    // GBuffer state violations
    GBufferNotColorAttachmentAtGeometry, // layout != ColorAttachment at geometry pass
    GBufferNotShaderReadAtComposite,     // layout != ShaderRead at composite pass

    // Shadow atlas state violations
    ShadowAtlasNotDepthWriteAtShadow,    // layout != DepthWrite at shadow pass
    ShadowAtlasNotDepthReadAtComposite,  // layout != DepthRead at composite pass
};

struct RenderGraphIssue {
    RenderGraphIssueCode code       = RenderGraphIssueCode::GeometryBeforeShadow;
    uint32_t             passIndex  = 0;
    std::string          message;
};

// ── Validation report ─────────────────────────────────────────────────────────
struct RenderGraphValidationReport {
    bool                          valid = true;
    std::vector<RenderGraphIssue> issues;
};

// ── Validator ────────────────────────────────────────────────────────────────
class RenderGraphValidator {
public:
    // Validate a fully-recorded render graph description.
    // Returns a report with `valid == true` and empty issues when the graph
    // conforms to all rules.
    [[nodiscard]] static RenderGraphValidationReport validate(
        const RenderGraphDesc& desc) noexcept;
};

} // namespace nexus::render
