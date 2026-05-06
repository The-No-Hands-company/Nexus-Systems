#pragma once
#include <nexus/gfx/Swapchain.h>
#include <vulkan/vulkan.h>
#include <vector>

namespace nexus::gfx {

class VulkanSwapchain final : public ISwapchain {
public:
    VulkanSwapchain(VkInstance instance, VkPhysicalDevice physDev,
                    VkDevice device, const SwapchainDesc& desc,
                    uint32_t graphicsFamily);
    ~VulkanSwapchain() override;

    [[nodiscard]] AcquiredFrame acquire()         override;
    [[nodiscard]] PresentResult present(uint32_t imageIndex, SemaphoreHandle waitSemaphore) override;
    void resize(Extent2D newExtent)               override;

    [[nodiscard]] Extent2D  extent()      const noexcept override { return m_extent; }
    [[nodiscard]] Format    colorFormat() const noexcept override { return m_format; }
    [[nodiscard]] uint32_t  imageCount()  const noexcept override { return static_cast<uint32_t>(m_images.size()); }
    [[nodiscard]] bool      isHdrActive() const noexcept override { return m_hdr; }

    // Inject the present queue after construction (called by VulkanDevice)
    void setPresentQueue(VkQueue queue, uint32_t family);

    // Raw image/view access (used by VulkanFrameScheduler to register images in the pool)
    [[nodiscard]] VkImage     image    (uint32_t i) const noexcept { return i < m_images.size()     ? m_images[i]     : VK_NULL_HANDLE; }
    [[nodiscard]] VkImageView imageView(uint32_t i) const noexcept { return i < m_imageViews.size() ? m_imageViews[i] : VK_NULL_HANDLE; }
    [[nodiscard]] VkFormat    vkColorFormat() const noexcept;  // computed from m_format

    // Resolve semaphore index to Vk handle (used by VulkanDevice::submit)
    [[nodiscard]] VkSemaphore imageAvailSem(uint32_t frameIdx) const noexcept {
        return frameIdx < m_imageAvailSems.size() ? m_imageAvailSems[frameIdx] : VK_NULL_HANDLE;
    }
    [[nodiscard]] VkSemaphore renderDoneSem(uint32_t frameIdx) const noexcept {
        return frameIdx < m_renderDoneSems.size() ? m_renderDoneSems[frameIdx] : VK_NULL_HANDLE;
    }

private:
    void create(const SwapchainDesc& desc, uint32_t graphicsFamily);
    void destroy();

    VkInstance       m_instance   = VK_NULL_HANDLE;
    VkPhysicalDevice m_physDev    = VK_NULL_HANDLE;
    VkDevice         m_device     = VK_NULL_HANDLE;
    VkSurfaceKHR     m_surface    = VK_NULL_HANDLE;
    VkSwapchainKHR   m_swapchain  = VK_NULL_HANDLE;
    VkQueue          m_presentQueue   = VK_NULL_HANDLE;
    uint32_t         m_graphicsFamily = 0;

    std::vector<VkImage>     m_images;
    std::vector<VkImageView> m_imageViews;
    std::vector<VkSemaphore> m_imageAvailSems;
    std::vector<VkSemaphore> m_renderDoneSems;

    Extent2D m_extent;
    Format   m_format = Format::B8G8R8A8_Srgb;
    bool     m_hdr    = false;
    uint32_t m_frameIndex = 0;
};

} // namespace nexus::gfx
