// ─────────────────────────────────────────────────────────────────────────────
//  VulkanTexture — VMA-backed image / image-view creation
// ─────────────────────────────────────────────────────────────────────────────
#include "VulkanTexture.h"
#include "VulkanUtils.h"
#include <vk_mem_alloc.h>
#include <stdexcept>

namespace nexus::gfx {

VulkanTexture vkCreateTexture(VmaAllocator vma, VkDevice device, const TextureDesc& desc)
{
    VulkanTexture result{};
    result.extent      = { desc.extent.width, desc.extent.height, desc.extent.depth };
    result.vkFormat    = vkutil::toVkFormat(desc.format);
    result.format      = desc.format;
    result.mipLevels   = desc.mipLevels;
    result.arrayLayers = desc.arrayLayers;
    result.usage       = desc.usage;
    result.isSparse    = desc.isTiled;

    const bool isDepth = vkutil::isDepthFormat(desc.format);
    const bool isCube  = desc.isCubemap;

    VkImageCreateInfo ici{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    ici.imageType     = (desc.extent.depth > 1) ? VK_IMAGE_TYPE_3D : VK_IMAGE_TYPE_2D;
    ici.format        = result.vkFormat;
    ici.extent        = result.extent;
    ici.mipLevels     = desc.mipLevels;
    ici.arrayLayers   = isCube ? 6 * desc.arrayLayers : desc.arrayLayers;
    ici.samples       = vkutil::toVkSampleCount(desc.samples);
    ici.tiling        = VK_IMAGE_TILING_OPTIMAL;
    ici.usage         = vkutil::toVkImageUsage(desc.usage);
    ici.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;
    ici.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    if (isCube) ici.flags |= VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
    if (desc.isTiled) {
        ici.flags |= VK_IMAGE_CREATE_SPARSE_BINDING_BIT
                  |  VK_IMAGE_CREATE_SPARSE_RESIDENCY_BIT;
    }

    VmaAllocationCreateInfo aci{};
    aci.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
    // Dedicated allocation for render targets / large textures
    if (static_cast<uint32_t>(desc.usage) & (static_cast<uint32_t>(TextureUsage::ColorAttachment)
                                           | static_cast<uint32_t>(TextureUsage::DepthAttachment)))
        aci.flags |= VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;

    if (desc.isTiled) {
        // Sparse images cannot be managed by VMA; allocate image only (no memory bound yet)
        VkResult res = vkCreateImage(device, &ici, nullptr, &result.image);
        if (res != VK_SUCCESS)
            throw std::runtime_error("vkCreateImage (sparse) failed: " + std::to_string(res));
    } else {
        VkResult res = vmaCreateImage(vma, &ici, &aci,
                                      &result.image, &result.allocation, nullptr);
        if (res != VK_SUCCESS)
            throw std::runtime_error("vmaCreateImage failed: " + std::to_string(res));
    }

    // ── Default image view ────────────────────────────────────────────────────
    VkImageViewCreateInfo vci{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    vci.image    = result.image;
    vci.viewType = isCube  ? VK_IMAGE_VIEW_TYPE_CUBE
                 : (ici.arrayLayers > 1) ? VK_IMAGE_VIEW_TYPE_2D_ARRAY
                 : (desc.extent.depth > 1) ? VK_IMAGE_VIEW_TYPE_3D
                 : VK_IMAGE_VIEW_TYPE_2D;
    vci.format   = result.vkFormat;

    vci.subresourceRange.aspectMask     = isDepth
        ? (vkutil::hasStencil(desc.format)
           ? VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT
           : VK_IMAGE_ASPECT_DEPTH_BIT)
        : VK_IMAGE_ASPECT_COLOR_BIT;
    vci.subresourceRange.baseMipLevel   = 0;
    vci.subresourceRange.levelCount     = desc.mipLevels;
    vci.subresourceRange.baseArrayLayer = 0;
    vci.subresourceRange.layerCount     = ici.arrayLayers;

    VkResult vres = vkCreateImageView(device, &vci, nullptr, &result.defaultView);
    if (vres != VK_SUCCESS)
        throw std::runtime_error("vkCreateImageView failed: " + std::to_string(vres));

    if (desc.debugName) {
        auto fn = reinterpret_cast<PFN_vkSetDebugUtilsObjectNameEXT>(
            vkGetDeviceProcAddr(device, "vkSetDebugUtilsObjectNameEXT"));
        if (fn) {
            VkDebugUtilsObjectNameInfoEXT ni{VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT};
            ni.objectType   = VK_OBJECT_TYPE_IMAGE;
            ni.objectHandle = reinterpret_cast<uint64_t>(result.image);
            ni.pObjectName  = desc.debugName;
            fn(device, &ni);
        }
    }

    return result;
}

void vkDestroyTexture(VmaAllocator vma, VkDevice device, VulkanTexture& tex)
{
    if (tex.defaultView != VK_NULL_HANDLE)
        vkDestroyImageView(device, tex.defaultView, nullptr);

    if (tex.image != VK_NULL_HANDLE) {
        if (tex.isSparse)
            vkDestroyImage(device, tex.image, nullptr);
        else if (vma != VK_NULL_HANDLE)
            vmaDestroyImage(vma, tex.image, tex.allocation);
    }
    tex = {};
}

} // namespace nexus::gfx
