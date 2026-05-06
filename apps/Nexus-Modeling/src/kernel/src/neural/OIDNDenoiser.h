#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  OIDNDenoiser — Intel Open Image Denoise v2 CPU denoiser
//  Always available (CPU fallback) when NEXUS_ENABLE_OIDN is set.
// ─────────────────────────────────────────────────────────────────────────────
#include <nexus/neural/NeuralRenderer.h>

namespace nexus::neural {

class OIDNDenoiser final : public INeuralRenderer {
public:
    explicit OIDNDenoiser();
    ~OIDNDenoiser() override;

    [[nodiscard]] DenoiserBackend activeDenoiser() const noexcept override;
    [[nodiscard]] UpscalerBackend activeUpscaler() const noexcept override { return UpscalerBackend::None; }

    void denoise(nexus::gfx::CmdBufHandle cmd, const DenoiserInput& in, DenoiserOutput& out) override;
    void upscale(nexus::gfx::CmdBufHandle cmd, const UpscalerInput& in, UpscalerOutput& out) override;

private:
    void* m_device  = nullptr;  // oidnDeviceRef
    void* m_filter  = nullptr;  // oidnFilterRef
};

} // namespace nexus::neural
