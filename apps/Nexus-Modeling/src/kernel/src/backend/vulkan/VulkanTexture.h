#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  VulkanTexture — GPU image/view entry stored in the device resource pool.
// ─────────────────────────────────────────────────────────────────────────────
#include <nexus/gfx/Types.h>
#include <nexus/gfx/Device.h>
#include <vulkan/vulkan.h>

struct VmaAllocation_T;
using VmaAllocation = VmaAllocation_T*;
struct VmaAllocator_T;
using VmaAllocator = VmaAllocator_T*;

namespace nexus::gfx {

struct VulkanTexture {
    VkImage       image       = VK_NULL_HANDLE;
    VkImageView   defaultView = VK_NULL_HANDLE;  // full-range, mip 0
    VmaAllocation allocation  = VK_NULL_HANDLE;
    VkExtent3D    extent      = {};
    VkFormat      vkFormat    = VK_FORMAT_UNDEFINED;
    Format        format      = Format::Undefined;
    uint32_t      mipLevels   = 1;
    uint32_t      arrayLayers = 1;
    TextureUsage  usage       = TextureUsage::None;
    bool          isSparse         = false;  // tiled / reserved resource
    bool          isExternalImage  = false;  // swapchain images — owned by ISwapchain, never freed here
};

// ── Factory / destructor ──────────────────────────────────────────────────────
VulkanTexture vkCreateTexture (VmaAllocator vma, VkDevice device, const TextureDesc& desc);
void          vkDestroyTexture(VmaAllocator vma, VkDevice device, VulkanTexture& tex);

} // namespace nexus::gfx
