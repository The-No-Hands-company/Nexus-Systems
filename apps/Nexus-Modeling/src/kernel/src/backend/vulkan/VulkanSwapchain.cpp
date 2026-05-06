// ─────────────────────────────────────────────────────────────────────────────
//  VulkanSwapchain — full surface + swapchain implementation
//  Platform surface creation: Linux xcb/Wayland, Windows Win32, macOS Metal
// ─────────────────────────────────────────────────────────────────────────────
#include "VulkanSwapchain.h"
#include "VulkanUtils.h"
#include <stdexcept>
#include <algorithm>
#include <limits>

// Platform-specific surface extensions
#if defined(NEXUS_PLATFORM_LINUX)
#   if defined(VK_USE_PLATFORM_WAYLAND_KHR)
#       include <vulkan/vulkan_wayland.h>
#   endif
#   if defined(VK_USE_PLATFORM_XCB_KHR)
#       include <vulkan/vulkan_xcb.h>
#   endif
#   if defined(VK_USE_PLATFORM_XLIB_KHR)
#       include <vulkan/vulkan_xlib.h>
#   endif
#elif defined(NEXUS_PLATFORM_WINDOWS)
#   include <vulkan/vulkan_win32.h>
#elif defined(NEXUS_PLATFORM_MACOS)
#   include <vulkan/vulkan_metal.h>
#endif

namespace nexus::gfx {

VulkanSwapchain::VulkanSwapchain(VkInstance instance, VkPhysicalDevice physDev,
                                 VkDevice device, const SwapchainDesc& desc,
                                 uint32_t graphicsFamily)
    : m_instance(instance), m_physDev(physDev), m_device(device),
      m_extent(desc.extent), m_hdr(desc.hdrEnabled)
{
    create(desc, graphicsFamily);
}

VulkanSwapchain::~VulkanSwapchain()
{
    destroy();
}

// ── Surface creation ──────────────────────────────────────────────────────────
static VkSurfaceKHR createPlatformSurface(VkInstance instance,
                                           void* windowHandle,
                                           void* displayHandle)
{
    VkSurfaceKHR surface = VK_NULL_HANDLE;

#if defined(NEXUS_PLATFORM_WINDOWS)
    VkWin32SurfaceCreateInfoKHR ci{VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR};
    ci.hinstance = GetModuleHandle(nullptr);
    ci.hwnd      = static_cast<HWND>(windowHandle);
    if (vkCreateWin32SurfaceKHR(instance, &ci, nullptr, &surface) != VK_SUCCESS)
        throw std::runtime_error("vkCreateWin32SurfaceKHR failed");

#elif defined(NEXUS_PLATFORM_MACOS)
    VkMetalSurfaceCreateInfoEXT ci{VK_STRUCTURE_TYPE_METAL_SURFACE_CREATE_INFO_EXT};
    ci.pLayer = windowHandle;  // CAMetalLayer*
    auto fn = reinterpret_cast<PFN_vkCreateMetalSurfaceEXT>(
        vkGetInstanceProcAddr(instance, "vkCreateMetalSurfaceEXT"));
    if (!fn || fn(instance, &ci, nullptr, &surface) != VK_SUCCESS)
        throw std::runtime_error("vkCreateMetalSurfaceEXT failed");

#elif defined(NEXUS_PLATFORM_LINUX)
    // Try Wayland first (preferred on modern Linux)
    bool tried = false;
#   if defined(VK_USE_PLATFORM_WAYLAND_KHR)
    if (displayHandle) {
        tried = true;
        auto fn = reinterpret_cast<PFN_vkCreateWaylandSurfaceKHR>(
            vkGetInstanceProcAddr(instance, "vkCreateWaylandSurfaceKHR"));
        if (fn) {
            VkWaylandSurfaceCreateInfoKHR ci{VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR};
            ci.display = static_cast<wl_display*>(displayHandle);
            ci.surface = static_cast<wl_surface*>(windowHandle);
            if (fn(instance, &ci, nullptr, &surface) == VK_SUCCESS) return surface;
        }
    }
#   endif

#   if defined(VK_USE_PLATFORM_XCB_KHR)
    if (!tried || surface == VK_NULL_HANDLE) {
        auto fn = reinterpret_cast<PFN_vkCreateXcbSurfaceKHR>(
            vkGetInstanceProcAddr(instance, "vkCreateXcbSurfaceKHR"));
        if (fn) {
            VkXcbSurfaceCreateInfoKHR ci{VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR};
            ci.connection = static_cast<xcb_connection_t*>(displayHandle);
            ci.window     = static_cast<xcb_window_t>(reinterpret_cast<uintptr_t>(windowHandle));
            if (fn(instance, &ci, nullptr, &surface) == VK_SUCCESS) return surface;
        }
    }
#   endif

#   if defined(VK_USE_PLATFORM_XLIB_KHR)
    if (surface == VK_NULL_HANDLE) {
        auto fn = reinterpret_cast<PFN_vkCreateXlibSurfaceKHR>(
            vkGetInstanceProcAddr(instance, "vkCreateXlibSurfaceKHR"));
        if (fn) {
            VkXlibSurfaceCreateInfoKHR ci{VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR};
            ci.dpy    = static_cast<Display*>(displayHandle);
            ci.window = static_cast<Window>(reinterpret_cast<uintptr_t>(windowHandle));
            if (fn(instance, &ci, nullptr, &surface) == VK_SUCCESS) return surface;
        }
    }
#   endif

    if (surface == VK_NULL_HANDLE)
        throw std::runtime_error("No suitable Vulkan surface extension available on Linux");
#else
    (void)instance; (void)windowHandle; (void)displayHandle;
    throw std::runtime_error("createPlatformSurface: unsupported platform");
#endif

    return surface;
}

// ── Format / present mode selection ──────────────────────────────────────────
static VkSurfaceFormatKHR chooseSurfaceFormat(VkPhysicalDevice pd, VkSurfaceKHR surface,
                                               bool hdr)
{
    uint32_t count = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(pd, surface, &count, nullptr);
    std::vector<VkSurfaceFormatKHR> formats(count);
    vkGetPhysicalDeviceSurfaceFormatsKHR(pd, surface, &count, formats.data());

    // HDR: prefer A2R10G10B10_UNORM_PACK32 or R16G16B16A16_SFLOAT with ST2084
    if (hdr) {
        for (auto& f : formats) {
            if ((f.format == VK_FORMAT_A2R10G10B10_UNORM_PACK32 ||
                 f.format == VK_FORMAT_R16G16B16A16_SFLOAT) &&
                 f.colorSpace == VK_COLOR_SPACE_HDR10_ST2084_EXT)
                return f;
        }
    }
    // SDR: prefer B8G8R8A8_SRGB or R8G8B8A8_SRGB
    for (auto& f : formats)
        if (f.format == VK_FORMAT_B8G8R8A8_SRGB && f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
            return f;
    for (auto& f : formats)
        if (f.format == VK_FORMAT_R8G8B8A8_SRGB && f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
            return f;
    return formats[0];
}

static VkPresentModeKHR choosePresentMode(VkPhysicalDevice pd, VkSurfaceKHR surface,
                                           PresentMode preferred)
{
    uint32_t count = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(pd, surface, &count, nullptr);
    std::vector<VkPresentModeKHR> modes(count);
    vkGetPhysicalDeviceSurfacePresentModesKHR(pd, surface, &count, modes.data());

    VkPresentModeKHR want = vkutil::toVkPresentMode(preferred);
    for (auto m : modes) if (m == want) return m;
    return VK_PRESENT_MODE_FIFO_KHR;  // always supported
}

// ── Create ────────────────────────────────────────────────────────────────────
void VulkanSwapchain::create(const SwapchainDesc& desc, uint32_t graphicsFamily)
{
    // 1. Surface
    if (desc.nativeWindowHandle) {
        m_surface = createPlatformSurface(m_instance, desc.nativeWindowHandle,
                                          desc.nativeDisplayHandle);
    }

    if (m_surface == VK_NULL_HANDLE) {
        // Headless path — no surface, no swapchain (e.g. offscreen rendering)
        m_extent = desc.extent;
        return;
    }

    // Validate presentation support
    VkBool32 presentSupport = VK_FALSE;
    vkGetPhysicalDeviceSurfaceSupportKHR(m_physDev, graphicsFamily, m_surface, &presentSupport);
    if (!presentSupport)
        throw std::runtime_error("Selected queue family does not support presentation");

    // 2. Surface capabilities
    VkSurfaceCapabilitiesKHR caps{};
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_physDev, m_surface, &caps);

    // Clamp extent
    if (caps.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
        m_extent = { caps.currentExtent.width, caps.currentExtent.height };
    } else {
        m_extent.width  = std::clamp(desc.extent.width,  caps.minImageExtent.width,  caps.maxImageExtent.width);
        m_extent.height = std::clamp(desc.extent.height, caps.minImageExtent.height, caps.maxImageExtent.height);
    }

    uint32_t imageCount = std::clamp(desc.imageCount,
                                     caps.minImageCount,
                                     caps.maxImageCount == 0 ? 8u : caps.maxImageCount);

    // 3. Choose format + present mode
    VkSurfaceFormatKHR surfFmt = chooseSurfaceFormat(m_physDev, m_surface, desc.hdrEnabled);
    VkPresentModeKHR   presMode= choosePresentMode(m_physDev, m_surface, desc.presentMode);

    m_format = vkutil::fromVkFormat(surfFmt.format);
    m_hdr    = desc.hdrEnabled && (surfFmt.colorSpace == VK_COLOR_SPACE_HDR10_ST2084_EXT);

    // 4. Swapchain
    VkSwapchainCreateInfoKHR sci{VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
    sci.surface          = m_surface;
    sci.minImageCount    = imageCount;
    sci.imageFormat      = surfFmt.format;
    sci.imageColorSpace  = surfFmt.colorSpace;
    sci.imageExtent      = { m_extent.width, m_extent.height };
    sci.imageArrayLayers = 1;
    sci.imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    sci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    sci.preTransform     = caps.currentTransform;
    sci.compositeAlpha   = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    sci.presentMode      = presMode;
    sci.clipped          = VK_TRUE;

    if (vkCreateSwapchainKHR(m_device, &sci, nullptr, &m_swapchain) != VK_SUCCESS)
        throw std::runtime_error("vkCreateSwapchainKHR failed");

    // 5. Retrieve images + create views
    uint32_t imgCount = 0;
    vkGetSwapchainImagesKHR(m_device, m_swapchain, &imgCount, nullptr);
    m_images.resize(imgCount);
    vkGetSwapchainImagesKHR(m_device, m_swapchain, &imgCount, m_images.data());

    m_imageViews.resize(imgCount);
    for (uint32_t i = 0; i < imgCount; ++i) {
        VkImageViewCreateInfo vci{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        vci.image                           = m_images[i];
        vci.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
        vci.format                          = surfFmt.format;
        vci.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        vci.subresourceRange.levelCount     = 1;
        vci.subresourceRange.layerCount     = 1;
        vkCreateImageView(m_device, &vci, nullptr, &m_imageViews[i]);
    }

    // 6. Per-frame sync objects
    m_imageAvailSems.resize(imgCount);
    m_renderDoneSems.resize(imgCount);
    VkSemaphoreCreateInfo si{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    for (uint32_t i = 0; i < imgCount; ++i) {
        vkCreateSemaphore(m_device, &si, nullptr, &m_imageAvailSems[i]);
        vkCreateSemaphore(m_device, &si, nullptr, &m_renderDoneSems[i]);
    }

    // 7. HDR metadata (if supported)
    if (m_hdr) {
        auto fn = reinterpret_cast<PFN_vkSetHdrMetadataEXT>(
            vkGetDeviceProcAddr(m_device, "vkSetHdrMetadataEXT"));
        if (fn) {
            VkHdrMetadataEXT meta{VK_STRUCTURE_TYPE_HDR_METADATA_EXT};
            meta.maxLuminance              = desc.hdrMeta.maxLuminance;
            meta.minLuminance              = desc.hdrMeta.minLuminance;
            meta.maxContentLightLevel      = desc.hdrMeta.maxCll;
            meta.maxFrameAverageLightLevel = desc.hdrMeta.maxFall;
            fn(m_device, 1, &m_swapchain, &meta);
        }
    }
}

// ── Destroy ───────────────────────────────────────────────────────────────────
void VulkanSwapchain::destroy()
{
    for (auto v : m_imageViews)     if (v) vkDestroyImageView (m_device, v, nullptr);
    for (auto s : m_imageAvailSems) if (s) vkDestroySemaphore (m_device, s, nullptr);
    for (auto s : m_renderDoneSems) if (s) vkDestroySemaphore (m_device, s, nullptr);
    m_imageViews.clear(); m_imageAvailSems.clear(); m_renderDoneSems.clear(); m_images.clear();
    if (m_swapchain) { vkDestroySwapchainKHR(m_device, m_swapchain, nullptr); m_swapchain = VK_NULL_HANDLE; }
    if (m_surface)   { vkDestroySurfaceKHR  (m_instance, m_surface,  nullptr); m_surface   = VK_NULL_HANDLE; }
}

// ── Acquire ───────────────────────────────────────────────────────────────────
AcquiredFrame VulkanSwapchain::acquire()
{
    if (m_swapchain == VK_NULL_HANDLE) {
        // Headless
        AcquiredFrame f{};
        f.imageIndex = m_frameIndex++ % 3u;
        return f;
    }

    const uint32_t frame = m_frameIndex % static_cast<uint32_t>(m_images.size());
    uint32_t imageIndex = 0;

    VkResult res = vkAcquireNextImageKHR(m_device, m_swapchain, UINT64_MAX,
                                          m_imageAvailSems[frame], VK_NULL_HANDLE,
                                          &imageIndex);

    AcquiredFrame f{};
    f.imageIndex = imageIndex;

    if (res == VK_ERROR_OUT_OF_DATE_KHR) { f.result = AcquireResult::OutOfDate; return f; }
    if (res == VK_SUBOPTIMAL_KHR)        { f.result = AcquireResult::Suboptimal; }

    // Wrap Vk semaphores as SemaphoreHandle (store index in id — NOT a real pool entry)
    f.imageAvailable.id = frame;               // caller interprets via getImageAvailSem()
    f.renderFinished.id = frame;

    ++m_frameIndex;
    return f;
}

// ── Present ───────────────────────────────────────────────────────────────────
PresentResult VulkanSwapchain::present(uint32_t imageIndex, SemaphoreHandle waitSem)
{
    if (m_swapchain == VK_NULL_HANDLE) return PresentResult::Ok;

    // waitSem.id is the frame index into m_renderDoneSems
    VkSemaphore wait = (waitSem.valid() && waitSem.id < m_renderDoneSems.size())
                     ? m_renderDoneSems[waitSem.id] : VK_NULL_HANDLE;

    VkPresentInfoKHR pi{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
    if (wait != VK_NULL_HANDLE) { pi.waitSemaphoreCount = 1; pi.pWaitSemaphores = &wait; }
    pi.swapchainCount = 1;
    pi.pSwapchains    = &m_swapchain;
    pi.pImageIndices  = &imageIndex;

    // Present queue — stored externally; we need it from VulkanDevice.
    // For now use a cached queue obtained at construction. TODO: inject queue.
    VkResult res = vkQueuePresentKHR(m_presentQueue, &pi);

    if (res == VK_ERROR_OUT_OF_DATE_KHR) return PresentResult::OutOfDate;
    if (res == VK_SUBOPTIMAL_KHR)        return PresentResult::Suboptimal;
    return PresentResult::Ok;
}

// ── Resize ────────────────────────────────────────────────────────────────────
void VulkanSwapchain::resize(Extent2D newExtent)
{
    vkDeviceWaitIdle(m_device);
    SwapchainDesc desc{};
    desc.extent      = newExtent;
    desc.imageCount  = static_cast<uint32_t>(m_images.size());
    desc.hdrEnabled  = m_hdr;
    destroy();
    create(desc, m_graphicsFamily);
}

// ── Queue injection (called by VulkanDevice after construction) ───────────────
void VulkanSwapchain::setPresentQueue(VkQueue queue, uint32_t family)
{
    m_presentQueue   = queue;
    m_graphicsFamily = family;
}

VkFormat VulkanSwapchain::vkColorFormat() const noexcept
{
    return vkutil::toVkFormat(m_format);
}

} // namespace nexus::gfx
