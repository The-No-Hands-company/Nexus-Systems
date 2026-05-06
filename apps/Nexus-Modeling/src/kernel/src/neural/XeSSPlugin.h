#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  XeSSPlugin — Intel XeSS runtime plugin (xess SDK)
//  Loaded at runtime; gracefully absent if SDK not installed.
// ─────────────────────────────────────────────────────────────────────────────
#include <nexus/neural/NeuralRenderer.h>
#include <vulkan/vulkan.h>

namespace nexus::neural {

class XeSSPlugin final : public INeuralRenderer {
public:
    XeSSPlugin(VkDevice device, VkPhysicalDevice physDev);
    ~XeSSPlugin() override;

    [[nodiscard]] bool available() const noexcept { return m_available; }
    [[nodiscard]] DenoiserBackend activeDenoiser() const noexcept override { return DenoiserBackend::None; }
    [[nodiscard]] UpscalerBackend activeUpscaler() const noexcept override { return m_available ? UpscalerBackend::XeSS : UpscalerBackend::None; }

    void denoise(nexus::gfx::CmdBufHandle cmd, const DenoiserInput& in, DenoiserOutput& out) override;
    void upscale(nexus::gfx::CmdBufHandle cmd, const UpscalerInput& in, UpscalerOutput& out) override;

private:
    bool        m_available = false;
    void*       m_libHandle = nullptr;
    void*       m_xessCtx   = nullptr;
    VkDevice    m_device    = VK_NULL_HANDLE;

    struct XeSSPfn {
        void* VKInit         = nullptr;  // xessVKInit
        void* GetInputRes    = nullptr;  // xessGetInputResolution
        void* VKExecute      = nullptr;  // xessVKExecute
        void* BuildPipelines = nullptr;  // xessBuildPipelines
        void* DestroyContext = nullptr;  // xessDestroyContext
    } m_pfn;

    void loadLibrary();
    void initXeSS(VkDevice device, VkPhysicalDevice physDev);
};

} // namespace nexus::neural
