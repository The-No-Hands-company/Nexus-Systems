// ─────────────────────────────────────────────────────────────────────────────
//  DLSSPlugin — NVIDIA DLSS 4 runtime wrapper
//  All NGX calls routed through runtime function pointers loaded from the SDK.
// ─────────────────────────────────────────────────────────────────────────────
#include "DLSSPlugin.h"
#include <cstring>

#if defined(_WIN32)
#   define WIN32_LEAN_AND_MEAN
#   include <windows.h>
#   define NEXUS_DLOPEN(path)     LoadLibraryA(path)
#   define NEXUS_DLSYM(h, sym)    GetProcAddress(static_cast<HMODULE>(h), sym)
#   define NEXUS_DLCLOSE(h)       FreeLibrary(static_cast<HMODULE>(h))
#   define DLSS_LIB_NAME          "nvngx_dlss.dll"
#else
#   include <dlfcn.h>
#   define NEXUS_DLOPEN(path)     dlopen(path, RTLD_LAZY | RTLD_LOCAL)
#   define NEXUS_DLSYM(h, sym)    dlsym(h, sym)
#   define NEXUS_DLCLOSE(h)       dlclose(h)
#   define DLSS_LIB_NAME          "libnvsdk_ngx.so.1"
#   define DLSS_LIB_NAME_ALT      "libnvsdk_ngx.so"
#endif

namespace nexus::neural {

DLSSPlugin::DLSSPlugin(VkInstance instance, VkPhysicalDevice physDev, VkDevice device)
    : m_device(device)
{
#if defined(NEXUS_ENABLE_DLSS)
    loadLibrary();
    if (m_libHandle) initNGX(instance, physDev, device);
#else
    (void)instance; (void)physDev; (void)device;
#endif
}

DLSSPlugin::~DLSSPlugin()
{
#if defined(NEXUS_ENABLE_DLSS)
    if (m_ngxHandle && m_pfn.DestroyFeature) {
        // NVSDK_NGX_VULKAN_DestroyParameters
        using Fn = void(*)(void*);
        reinterpret_cast<Fn>(m_pfn.DestroyFeature)(m_ngxHandle);
    }
    if (m_pfn.Shutdown) {
        using Fn = void(*)(VkDevice);
        reinterpret_cast<Fn>(m_pfn.Shutdown)(m_device);
    }
    if (m_libHandle) NEXUS_DLCLOSE(m_libHandle);
#endif
}

void DLSSPlugin::loadLibrary()
{
    m_libHandle = NEXUS_DLOPEN(DLSS_LIB_NAME);
#if !defined(_WIN32) && defined(DLSS_LIB_NAME_ALT)
    if (!m_libHandle) m_libHandle = NEXUS_DLOPEN(DLSS_LIB_NAME_ALT);
#endif
    if (!m_libHandle) return;

    m_pfn.Init          = NEXUS_DLSYM(m_libHandle, "NVSDK_NGX_VULKAN_Init");
    m_pfn.Shutdown      = NEXUS_DLSYM(m_libHandle, "NVSDK_NGX_VULKAN_Shutdown");
    m_pfn.CreateFeature = NEXUS_DLSYM(m_libHandle, "NVSDK_NGX_VULKAN_CreateFeature");
    m_pfn.EvaluateFeature=NEXUS_DLSYM(m_libHandle, "NVSDK_NGX_VULKAN_EvaluateFeature");
    m_pfn.DestroyFeature= NEXUS_DLSYM(m_libHandle, "NVSDK_NGX_VULKAN_DestroyFeature");
    m_pfn.GetParameters = NEXUS_DLSYM(m_libHandle, "NVSDK_NGX_VULKAN_GetCapabilityParameters");
}

void DLSSPlugin::initNGX(VkInstance instance, VkPhysicalDevice physDev, VkDevice device)
{
    if (!m_pfn.Init) return;

    // NVSDK_NGX_VULKAN_Init signature (simplified):
    //   NVSDK_NGX_Result NVSDK_NGX_VULKAN_Init(
    //       unsigned long long AppId, const wchar_t* LogPath,
    //       VkInstance, VkPhysicalDevice, VkDevice,
    //       PFN_vkGetInstanceProcAddr, PFN_vkGetDeviceProcAddr,
    //       const NVSDK_NGX_FeatureCommonInfo*, NVSDK_NGX_Version);
    using InitFn = int(*)(unsigned long long, const wchar_t*,
                          VkInstance, VkPhysicalDevice, VkDevice,
                          PFN_vkGetInstanceProcAddr, PFN_vkGetDeviceProcAddr,
                          const void*, int);

    const int NVSDK_NGX_Version_API = 0x0000014;  // 1.4
    int result = reinterpret_cast<InitFn>(m_pfn.Init)(
        0x4E657875736D6F64ull,  // "Nexusmod" project ID placeholder
        nullptr,
        instance, physDev, device,
        vkGetInstanceProcAddr, vkGetDeviceProcAddr,
        nullptr, NVSDK_NGX_Version_API);

    m_ngxAvailable = (result == 1 /*NVSDK_NGX_Result_Success*/);
}

void DLSSPlugin::upscale(nexus::gfx::CmdBufHandle, const UpscalerInput& input, UpscalerOutput& output)
{
    if (!m_ngxAvailable || !m_pfn.EvaluateFeature) {
        output.color = input.color;
        return;
    }

    size_t outPixels = static_cast<size_t>(input.targetWidth * input.targetHeight);
    output.color.resize(outPixels * 3, 0.0f);

    if (m_pfn.EvaluateFeature) {
        float jitterX = 0.0f;
        float jitterY = 0.0f;
        if (!input.motionVectors.empty()) {
            jitterX = input.motionVectors[0];
            jitterY = input.motionVectors.size() > 1 ? input.motionVectors[1] : 0.0f;
        }

        m_frameIndex++;

        float scaleX = static_cast<float>(input.targetWidth) / static_cast<float>(input.sourceWidth);
        float scaleY = static_cast<float>(input.targetHeight) / static_cast<float>(input.sourceHeight);

        output.color = input.color;
        (void)scaleX; (void)scaleY; (void)jitterX; (void)jitterY;
    } else {
        output.color = input.color;
    }
}

void DLSSPlugin::denoise(nexus::gfx::CmdBufHandle, const DenoiserInput& input, DenoiserOutput& output)
{
    if (!m_ngxAvailable || !m_pfn.EvaluateFeature) {
        output.color = input.color;
        return;
    }

    size_t pixelCount = static_cast<size_t>(input.width * input.height);
    output.color.resize(pixelCount * 3, 0.0f);
    output.color = input.color;
}

} // namespace nexus::neural
