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

void OIDNDenoiser::denoise(nexus::gfx::CmdBufHandle, const DenoiserInput& input, DenoiserOutput& output)
{
#if defined(NEXUS_ENABLE_OIDN)
    if (!m_filter) {
        output.color = input.color;
        return;
    }

    auto dev    = static_cast<OIDNDevice>(m_device);
    auto filter = static_cast<OIDNFilter>(m_filter);

    size_t pixelCount = static_cast<size_t>(input.width * input.height);
    size_t bufferSize = pixelCount * 3 * sizeof(float);

    oidnSetSharedFilterImage(filter, "color",
        const_cast<float*>(input.color.data()), OIDN_FORMAT_FLOAT3,
        input.width, input.height, 0, sizeof(float) * 3, sizeof(float) * 3 * input.width);

    if (!input.albedo.empty()) {
        oidnSetSharedFilterImage(filter, "albedo",
            const_cast<float*>(input.albedo.data()), OIDN_FORMAT_FLOAT3,
            input.width, input.height, 0, sizeof(float) * 3, sizeof(float) * 3 * input.width);
    }

    if (!input.normal.empty()) {
        oidnSetSharedFilterImage(filter, "normal",
            const_cast<float*>(input.normal.data()), OIDN_FORMAT_FLOAT3,
            input.width, input.height, 0, sizeof(float) * 3, sizeof(float) * 3 * input.width);
    }

    output.color.resize(pixelCount * 3, 0.0f);
    oidnSetSharedFilterImage(filter, "output",
        output.color.data(), OIDN_FORMAT_FLOAT3,
        input.width, input.height, 0, sizeof(float) * 3, sizeof(float) * 3 * input.width);

    oidnCommitFilter(filter);
    oidnExecuteFilter(filter);

    const char* errorMsg = nullptr;
    if (oidnGetDeviceError(dev, &errorMsg) != OIDN_ERROR_NONE) {
        output.color = input.color;
    }
#else
    output.color = input.color;
    (void)input;
#endif
}

void OIDNDenoiser::upscale(nexus::gfx::CmdBufHandle /*cmd*/, const UpscalerInput& /*input*/, UpscalerOutput& /*output*/)
{
    // OIDN does not provide spatial upscaling; this is a denoiser-only plugin.
}

} // namespace nexus::neural
