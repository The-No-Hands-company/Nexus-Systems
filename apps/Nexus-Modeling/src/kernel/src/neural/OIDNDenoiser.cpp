// ─────────────────────────────────────────────────────────────────────────────
//  OIDNDenoiser — Intel Open Image Denoise v2 wrapper
//  CPU-side, no Vulkan dependency.
// ─────────────────────────────────────────────────────────────────────────────
#include "OIDNDenoiser.h"

#if defined(NEXUS_ENABLE_OIDN)
#   include <OpenImageDenoise/oidn.h>
#endif

namespace nexus::neural {

OIDNDenoiser::OIDNDenoiser()
{
#if defined(NEXUS_ENABLE_OIDN)
    m_device = oidnNewDevice(OIDN_DEVICE_TYPE_DEFAULT);
    if (m_device) {
        oidnCommitDevice(static_cast<OIDNDevice>(m_device));
        m_filter = oidnNewFilter(static_cast<OIDNDevice>(m_device), "RT");
        if (m_filter) {
            oidnSetFilter1b(static_cast<OIDNFilter>(m_filter), "hdr", true);
            oidnSetFilter1b(static_cast<OIDNFilter>(m_filter), "cleanAux", true);
        }
    }
#endif
}

OIDNDenoiser::~OIDNDenoiser()
{
#if defined(NEXUS_ENABLE_OIDN)
    if (m_filter) oidnReleaseFilter(static_cast<OIDNFilter>(m_filter));
    if (m_device) oidnReleaseDevice(static_cast<OIDNDevice>(m_device));
#endif
}

DenoiserBackend OIDNDenoiser::activeDenoiser() const noexcept {
    return m_device ? DenoiserBackend::OIDN_CPU : DenoiserBackend::None;
}

void OIDNDenoiser::denoise(nexus::gfx::CmdBufHandle /*cmd*/, const DenoiserInput& input, DenoiserOutput& /*output*/)
{
#if defined(NEXUS_ENABLE_OIDN)
    if (!m_filter) return;
    auto dev    = static_cast<OIDNDevice>(m_device);
    auto filter = static_cast<OIDNFilter>(m_filter);

    // NOTE: For GPU-side denoising, color/albedo/normal TextureHandles
    //       need to be read back to CPU buffers first, or use the OIDN CUDA/HIP path.
    // For now: only proceeds when CPU-readable pointers are attached.
    // NOTE: OIDN operates on CPU-visible float3 buffers.
    // TextureHandles are GPU-resident; a readback pass is needed for CPU denoising.
    // This path is a placeholder — full integration requires a readback blit.
    (void)input; (void)dev; (void)filter;
#else
    (void)input;
#endif
}

void OIDNDenoiser::upscale(nexus::gfx::CmdBufHandle /*cmd*/, const UpscalerInput& /*input*/, UpscalerOutput& /*output*/)
{
    // OIDN does not provide spatial upscaling; this is a denoiser-only plugin.
}

} // namespace nexus::neural
