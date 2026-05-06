#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  DLSSPlugin — NVIDIA DLSS 4 (NGX) runtime plugin
//  Loaded at runtime via dlopen/LoadLibrary; gracefully absent if not installed.
// ─────────────────────────────────────────────────────────────────────────────
#include <nexus/neural/NeuralRenderer.h>
#include <vulkan/vulkan.h>

namespace nexus::neural {

class DLSSPlugin final : public INeuralRenderer {
public:
    // vkDevice/instance needed for NGX Vulkan init
    DLSSPlugin(VkInstance instance, VkPhysicalDevice physDev, VkDevice device);
    ~DLSSPlugin() override;

    [[nodiscard]] bool available() const noexcept { return m_ngxAvailable; }
    [[nodiscard]] DenoiserBackend activeDenoiser() const noexcept override { return m_ngxAvailable ? DenoiserBackend::DLSS4 : DenoiserBackend::None; }
    [[nodiscard]] UpscalerBackend activeUpscaler() const noexcept override { return m_ngxAvailable ? UpscalerBackend::DLSS4 : UpscalerBackend::None; }

    void denoise(nexus::gfx::CmdBufHandle cmd, const DenoiserInput& in, DenoiserOutput& out) override;
    void upscale(nexus::gfx::CmdBufHandle cmd, const UpscalerInput& in, UpscalerOutput& out) override;

private:
    bool        m_ngxAvailable = false;
    void*       m_libHandle    = nullptr;  // dlopen handle
    void*       m_ngxHandle    = nullptr;  // NVSDK_NGX_Handle
    VkDevice    m_device       = VK_NULL_HANDLE;

    // Function pointer table
    struct NGXPfn {
        void* Init        = nullptr;  // NVSDK_NGX_VULKAN_Init
        void* Shutdown    = nullptr;  // NVSDK_NGX_VULKAN_Shutdown
        void* CreateFeature  = nullptr;
        void* EvaluateFeature= nullptr;
        void* DestroyFeature = nullptr;
        void* GetParameters  = nullptr;
    } m_pfn;

    void loadLibrary();
    void initNGX(VkInstance instance, VkPhysicalDevice physDev, VkDevice device);
};

} // namespace nexus::neural
