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
    // Current OIDN integration does not yet execute a full GPU-texture readback
    // and writeback path, so it is not exposed as an executable denoiser backend.
    return DenoiserBackend::None;
}

void OIDNDenoiser::denoise(nexus::gfx::CmdBufHandle /*cmd*/, const DenoiserInput& input, DenoiserOutput& /*output*/)
{
#if defined(NEXUS_ENABLE_OIDN)
    if (!m_filter) return;
    auto dev    = static_cast<OIDNDevice>(m_device);
    auto filter = static_cast<OIDNFilter>(m_filter);

    // Runtime-safe behavior until readback/writeback integration is implemented:
    // retain a deterministic no-op denoise path.
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
