// ─────────────────────────────────────────────────────────────────────────────
//  NeuralRenderer — factory implementation
//  Priority: DLSS4 → XeSS → OIDN → deterministic software fallback
// ─────────────────────────────────────────────────────────────────────────────
#include <nexus/neural/NeuralRenderer.h>
#include <nexus/gfx/Device.h>
#include "OIDNDenoiser.h"
#include "DLSSPlugin.h"
#include "XeSSPlugin.h"
#include <memory>

#if defined(NEXUS_BACKEND_VULKAN)
#include "../backend/vulkan/VulkanDevice.h"
#endif

namespace nexus::neural {

namespace {

class DeterministicFallbackNeuralRenderer final : public INeuralRenderer {
public:
    [[nodiscard]] DenoiserBackend activeDenoiser() const noexcept override
    {
        return DenoiserBackend::None;
    }

    [[nodiscard]] UpscalerBackend activeUpscaler() const noexcept override
    {
        return UpscalerBackend::Bilinear;
    }

    void denoise(nexus::gfx::CmdBufHandle,
                 const DenoiserInput& in,
                 DenoiserOutput& out) override
    {
        // Deterministic fallback path: preserve the source color when no executable
        // neural denoiser is available in the current runtime.
        out.color = in.color;
    }

    void upscale(nexus::gfx::CmdBufHandle,
                 const UpscalerInput& in,
                 UpscalerOutput& out) override
    {
        // Deterministic fallback path: preserve the source color when no executable
        // neural upscaler is available in the current runtime.
        out.color = in.color;
    }
};

[[nodiscard]] bool hasExecutablePath(const INeuralRenderer& renderer) noexcept
{
    return renderer.activeDenoiser() != DenoiserBackend::None
        || renderer.activeUpscaler() != UpscalerBackend::None;
}

} // namespace

std::unique_ptr<INeuralRenderer> createNeuralRenderer(
    nexus::gfx::IDevice& device,
    bool preferDLSS,
    bool preferXeSS,
    bool preferFSR)
{
    (void)preferFSR;

    // Obtain Vulkan handles if the backend is Vulkan.
    VkInstance       instance  = VK_NULL_HANDLE;
    VkPhysicalDevice physDev   = VK_NULL_HANDLE;
    VkDevice         vkDevice  = VK_NULL_HANDLE;

    if (device.backend() == nexus::gfx::Backend::Vulkan) {
#if defined(NEXUS_BACKEND_VULKAN)
        if (auto* vd = dynamic_cast<nexus::gfx::VulkanDevice*>(&device)) {
            instance = vd->instance();
            physDev  = vd->physical();
            vkDevice = vd->logical();
        }
#endif
    }

    // ── DLSS 4 ────────────────────────────────────────────────────────────────
    if (preferDLSS) {
        auto dlss = std::make_unique<DLSSPlugin>(instance, physDev, vkDevice);
        if (dlss->available()) return dlss;
    }

    // ── XeSS ─────────────────────────────────────────────────────────────────
    if (preferXeSS) {
        auto xess = std::make_unique<XeSSPlugin>(vkDevice, physDev);
        if (xess->available()) return xess;
    }

    // ── OIDN (CPU fallback candidate) ─────────────────────────────────────────
    {
        auto oidn = std::make_unique<OIDNDenoiser>();
        if (hasExecutablePath(*oidn)) return oidn;
    }

    // Deterministic baseline renderer for unsupported or unavailable runtimes.
    return std::make_unique<DeterministicFallbackNeuralRenderer>();
}

} // namespace nexus::neural

