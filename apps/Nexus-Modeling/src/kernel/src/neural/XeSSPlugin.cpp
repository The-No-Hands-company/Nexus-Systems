// ─────────────────────────────────────────────────────────────────────────────
//  XeSSPlugin — Intel XeSS runtime wrapper
// ─────────────────────────────────────────────────────────────────────────────
#include "XeSSPlugin.h"

#if defined(_WIN32)
#   define WIN32_LEAN_AND_MEAN
#   include <windows.h>
#   define NEXUS_DLOPEN(p)    LoadLibraryA(p)
#   define NEXUS_DLSYM(h, s)  GetProcAddress(static_cast<HMODULE>(h), s)
#   define NEXUS_DLCLOSE(h)   FreeLibrary(static_cast<HMODULE>(h))
#   define XESS_LIB_NAME      "libxess.dll"
#else
#   include <dlfcn.h>
#   define NEXUS_DLOPEN(p)    dlopen(p, RTLD_LAZY | RTLD_LOCAL)
#   define NEXUS_DLSYM(h, s)  dlsym(h, s)
#   define NEXUS_DLCLOSE(h)   dlclose(h)
#   define XESS_LIB_NAME      "libxess.so"
#   define XESS_LIB_NAME_ALT  "libxess.so.1"
#endif

namespace nexus::neural {

XeSSPlugin::XeSSPlugin(VkDevice device, VkPhysicalDevice physDev)
    : m_device(device)
{
#if defined(NEXUS_ENABLE_XESS)
    loadLibrary();
    if (m_libHandle) initXeSS(device, physDev);
#else
    (void)device; (void)physDev;
#endif
}

XeSSPlugin::~XeSSPlugin()
{
#if defined(NEXUS_ENABLE_XESS)
    if (m_xessCtx && m_pfn.DestroyContext) {
        using Fn = int(*)(void*);
        reinterpret_cast<Fn>(m_pfn.DestroyContext)(m_xessCtx);
    }
    if (m_libHandle) NEXUS_DLCLOSE(m_libHandle);
#endif
}

void XeSSPlugin::loadLibrary()
{
    m_libHandle = NEXUS_DLOPEN(XESS_LIB_NAME);
#if !defined(_WIN32) && defined(XESS_LIB_NAME_ALT)
    if (!m_libHandle) m_libHandle = NEXUS_DLOPEN(XESS_LIB_NAME_ALT);
#endif
    if (!m_libHandle) return;

    m_pfn.VKInit         = NEXUS_DLSYM(m_libHandle, "xessVKInit");
    m_pfn.GetInputRes    = NEXUS_DLSYM(m_libHandle, "xessGetInputResolution");
    m_pfn.VKExecute      = NEXUS_DLSYM(m_libHandle, "xessVKExecute");
    m_pfn.BuildPipelines = NEXUS_DLSYM(m_libHandle, "xessBuildPipelines");
    m_pfn.DestroyContext = NEXUS_DLSYM(m_libHandle, "xessDestroyContext");
}

void XeSSPlugin::initXeSS(VkDevice device, VkPhysicalDevice physDev)
{
    if (!m_pfn.VKInit) return;

    // xessVKInit(VkDevice, VkPhysicalDevice, xess_context_handle_t*)
    using InitFn = int(*)(VkDevice, VkPhysicalDevice, void**);
    int result = reinterpret_cast<InitFn>(m_pfn.VKInit)(device, physDev, &m_xessCtx);
    m_available = (result == 0 /*XESS_RESULT_SUCCESS*/);
}

void XeSSPlugin::upscale(nexus::gfx::CmdBufHandle /*cmd*/, const UpscalerInput& input, UpscalerOutput& output)
{
    if (!m_available || !m_pfn.VKExecute || !m_xessCtx) {
        output.color = input.color;
        return;
    }

    // Until full XeSS execute parameter wiring is integrated, keep behavior deterministic.
    output.color = input.color;
}

void XeSSPlugin::denoise(nexus::gfx::CmdBufHandle /*cmd*/, const DenoiserInput& /*input*/, DenoiserOutput& /*output*/)
{
    // XeSS is an upscaler, not a denoiser. No-op.
}

} // namespace nexus::neural
