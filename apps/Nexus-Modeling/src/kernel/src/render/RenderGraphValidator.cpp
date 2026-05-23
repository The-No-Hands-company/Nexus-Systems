#include <nexus/render/RenderGraphValidator.h>

#include <algorithm>

namespace nexus::render {

namespace {

void addIssue(RenderGraphValidationReport& report,
              RenderGraphIssueCode code,
              uint32_t passIndex,
              const char* message)
{
    report.valid = false;
    report.issues.push_back({code, passIndex, message});
}

} // anonymous namespace

RenderGraphValidationReport RenderGraphValidator::validate(
    const RenderGraphDesc& desc) noexcept
{
    RenderGraphValidationReport report;

    bool seenShadow    = false;
    bool seenGeometry  = false;
    bool seenComposite = false;

    for (uint32_t i = 0; i < static_cast<uint32_t>(desc.passes.size()); ++i) {
        const auto& p = desc.passes[i];

        switch (p.passType) {
        case RenderPassType::Shadow:
            // ── Pass ordering ─────────────────────────────────────────────
            // Shadow pass has no ordering pre-requisites here — it always runs
            // first.  Violations are detected when geometry/composite appear too
            // early; track the shadow flag.
            seenShadow = true;

            // ── Shadow atlas state ────────────────────────────────────────
            if (p.shadowAtlasLayout != nexus::gfx::TextureLayout::DepthWrite) {
                addIssue(report, RenderGraphIssueCode::ShadowAtlasNotDepthWriteAtShadow, i,
                    "shadow atlas must be in DepthWrite layout during shadow pass");
            }
            break;

        case RenderPassType::Geometry:
            // ── Pass ordering ─────────────────────────────────────────────
            if (!seenShadow) {
                addIssue(report, RenderGraphIssueCode::GeometryBeforeShadow, i,
                    "geometry pass recorded before any shadow pass");
            }
            seenGeometry = true;

            // ── GBuffer state ─────────────────────────────────────────────
            if (p.gbufferColorLayout != nexus::gfx::TextureLayout::ColorAttachment) {
                addIssue(report, RenderGraphIssueCode::GBufferNotColorAttachmentAtGeometry, i,
                    "GBuffer color targets must be in ColorAttachment layout at geometry pass");
            }
            break;

        case RenderPassType::Composite:
            // ── Pass ordering ─────────────────────────────────────────────
            if (!seenGeometry) {
                addIssue(report, RenderGraphIssueCode::CompositeBeforeGeometry, i,
                    "composite pass recorded before any geometry pass");
            }
            if (!seenShadow) {
                addIssue(report, RenderGraphIssueCode::CompositeBeforeShadow, i,
                    "composite pass recorded before any shadow pass");
            }
            seenComposite = true;

            // ── GBuffer state ─────────────────────────────────────────────
            if (p.gbufferColorLayout != nexus::gfx::TextureLayout::ShaderRead) {
                addIssue(report, RenderGraphIssueCode::GBufferNotShaderReadAtComposite, i,
                    "GBuffer color targets must be in ShaderRead layout at composite pass");
            }

            // ── Shadow atlas state ────────────────────────────────────────
            if (p.shadowAtlasLayout != nexus::gfx::TextureLayout::DepthRead) {
                addIssue(report, RenderGraphIssueCode::ShadowAtlasNotDepthReadAtComposite, i,
                    "shadow atlas must be in DepthRead layout at composite pass");
            }
            break;

        case RenderPassType::RayTracing:
            // ── RT pass state ────────────────────────────────────────────
            // Ray tracing is optional and runs after composite in the deferred
            // rendering flow. No strict ordering constraints beyond that.
            // GBuffer and shadow atlas should be in readable state.
            // (Validation is currently permissive for stub implementations.)
            break;
        }

        (void)seenComposite; // unused after the loop; silence potential warning
    }

    return report;
}

} // namespace nexus::render
