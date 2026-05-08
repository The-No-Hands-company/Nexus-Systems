// ─────────────────────────────────────────────────────────────────────────────
//  VulkanAllocator — VMA 3.x integration
//  VMA_IMPLEMENTATION is defined in exactly this translation unit.
// ─────────────────────────────────────────────────────────────────────────────

// VMA implementation — must precede the include in exactly one TU
#define VMA_IMPLEMENTATION
#define VMA_STATIC_VULKAN_FUNCTIONS  0   // use runtime function pointers
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 1
#include <vk_mem_alloc.h>

#include "VulkanAllocator.h"
#include "VulkanCommandBuffer.h"
#include "VulkanUtils.h"
#include <stdexcept>
#include <algorithm>
#include <vector>

namespace nexus::gfx {

namespace {

uint32_t findMemoryType(VkPhysicalDevice physDev, uint32_t typeBits, VkMemoryPropertyFlags requiredFlags)
{
    VkPhysicalDeviceMemoryProperties memProps{};
    vkGetPhysicalDeviceMemoryProperties(physDev, &memProps);
    for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i) {
        const bool supported = (typeBits & (1u << i)) != 0;
        const bool hasFlags = (memProps.memoryTypes[i].propertyFlags & requiredFlags) == requiredFlags;
        if (supported && hasFlags) return i;
    }
    for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i) {
        if ((typeBits & (1u << i)) != 0) return i;
    }
    return UINT32_MAX;
}

VkImageAspectFlags defaultAspectForFormat(VkFormat format)
{
    switch (format) {
    case VK_FORMAT_D16_UNORM:
    case VK_FORMAT_X8_D24_UNORM_PACK32:
    case VK_FORMAT_D32_SFLOAT:
        return VK_IMAGE_ASPECT_DEPTH_BIT;
    case VK_FORMAT_S8_UINT:
        return VK_IMAGE_ASPECT_STENCIL_BIT;
    case VK_FORMAT_D16_UNORM_S8_UINT:
    case VK_FORMAT_D24_UNORM_S8_UINT:
    case VK_FORMAT_D32_SFLOAT_S8_UINT:
        return VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
    default:
        return VK_IMAGE_ASPECT_COLOR_BIT;
    }
}

} // namespace

VulkanAllocator::VulkanAllocator(VkInstance instance,
                                 VkPhysicalDevice physDev,
                                 VkDevice device,
                                 VulkanResourcePool* pool,
                                 VkQueue sparseQueue)
    : m_ownsVma(true),
      m_physDevice(physDev),
      m_device(device),
      m_pool(pool),
      m_sparseQueue(sparseQueue)
{
    VmaVulkanFunctions vkFns{};
    vkFns.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
    vkFns.vkGetDeviceProcAddr   = vkGetDeviceProcAddr;

    VmaAllocatorCreateInfo ci{};
    ci.instance         = instance;
    ci.physicalDevice   = physDev;
    ci.device           = device;
    ci.vulkanApiVersion = VK_API_VERSION_1_3;
    ci.pVulkanFunctions = &vkFns;
    // Enable Buffer Device Address so VMA can create BDA-capable buffers
    ci.flags            = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT
                        | VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT;

    VkResult res = vmaCreateAllocator(&ci, &m_vma);
    if (res != VK_SUCCESS)
        throw std::runtime_error("vmaCreateAllocator failed: " + std::to_string(res));
}

VulkanAllocator::VulkanAllocator(VmaAllocator existingVma,
                                 VkPhysicalDevice physDev,
                                 VkDevice device,
                                 VulkanResourcePool* pool,
                                 VkQueue sparseQueue) noexcept
    : m_vma(existingVma),
      m_ownsVma(false),
      m_physDevice(physDev),
      m_device(device),
      m_pool(pool),
      m_sparseQueue(sparseQueue)
{}

VulkanAllocator::~VulkanAllocator()
{
    if (m_device != VK_NULL_HANDLE && m_sparseQueue != VK_NULL_HANDLE && m_pool != nullptr) {
        for (const auto& [_, tile] : m_tileBindings) {
            if (!tile.texture.valid() || tile.texture.id >= m_pool->textures.size()) {
                if (tile.memory != VK_NULL_HANDLE) vkFreeMemory(m_device, tile.memory, nullptr);
                continue;
            }
            const auto& tex = m_pool->textures[tile.texture.id];
            if (tex.image != VK_NULL_HANDLE) {
                VkSparseImageMemoryBind unbind = tile.bind;
                unbind.memory = VK_NULL_HANDLE;
                unbind.memoryOffset = 0;
                submitSparseImageBind(tex.image, unbind);
            }
            if (tile.memory != VK_NULL_HANDLE) vkFreeMemory(m_device, tile.memory, nullptr);
        }
    }
    m_tileBindings.clear();
    if (m_ownsVma && m_vma) {
        vmaDestroyAllocator(m_vma);
    }
    m_vma = nullptr;
}

uint64_t VulkanAllocator::makeTileKey(TextureHandle tex, uint32_t tileX, uint32_t tileY, uint32_t mipLevel) const noexcept
{
    uint64_t h = static_cast<uint64_t>(tex.id);
    h ^= static_cast<uint64_t>(tileX) << 20;
    h ^= static_cast<uint64_t>(tileY) << 40;
    h ^= static_cast<uint64_t>(mipLevel) << 56;
    return h;
}

void VulkanAllocator::submitSparseImageBind(VkImage image, const VkSparseImageMemoryBind& bind) const
{
    if (m_sparseQueue == VK_NULL_HANDLE || m_device == VK_NULL_HANDLE || image == VK_NULL_HANDLE) return;

    VkSparseImageMemoryBindInfo imageBindInfo{};
    imageBindInfo.image = image;
    imageBindInfo.bindCount = 1;
    imageBindInfo.pBinds = &bind;

    VkBindSparseInfo bindInfo{VK_STRUCTURE_TYPE_BIND_SPARSE_INFO};
    bindInfo.imageBindCount = 1;
    bindInfo.pImageBinds = &imageBindInfo;

    VkFenceCreateInfo fi{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    VkFence fence = VK_NULL_HANDLE;
    if (vkCreateFence(m_device, &fi, nullptr, &fence) != VK_SUCCESS) return;

    if (vkQueueBindSparse(m_sparseQueue, 1, &bindInfo, fence) == VK_SUCCESS) {
        vkWaitForFences(m_device, 1, &fence, VK_TRUE, UINT64_MAX);
    }
    vkDestroyFence(m_device, fence, nullptr);
}

void* VulkanAllocator::alloc(const AllocationInfo& info)
{
    // Raw allocation (used for custom VkMemory not tied to a buffer/image)
    VkMemoryRequirements reqs{};
    reqs.size      = info.sizeBytes;
    reqs.alignment = info.alignment ? info.alignment : 1;

    VmaAllocationCreateInfo aci{};
    aci.usage = VMA_MEMORY_USAGE_AUTO;
    if (info.hint == MemoryHint::CpuToGpu)
        aci.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT
                  | VMA_ALLOCATION_CREATE_MAPPED_BIT;
    else if (info.hint == MemoryHint::GpuToCpu)
        aci.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT
                  | VMA_ALLOCATION_CREATE_MAPPED_BIT;

    VmaAllocation alloc = VK_NULL_HANDLE;
    VmaAllocationInfo ai{};
    vmaAllocateMemory(m_vma, &reqs, &aci, &alloc, &ai);
    return alloc;  // caller casts back
}

void VulkanAllocator::free(void* allocation)
{
    if (allocation)
        vmaFreeMemory(m_vma, static_cast<VmaAllocation>(allocation));
}

void VulkanAllocator::commitTile(TextureHandle texHandle, uint32_t tx,
                                 uint32_t ty, uint32_t mip)
{
    if (!texHandle.valid() || m_pool == nullptr || m_device == VK_NULL_HANDLE || m_physDevice == VK_NULL_HANDLE) return;
    if (texHandle.id >= m_pool->textures.size()) return;

    const uint64_t key = makeTileKey(texHandle, tx, ty, mip);
    if (m_tileBindings.find(key) != m_tileBindings.end()) return;

    auto& tex = m_pool->textures[texHandle.id];
    if (!tex.isSparse || tex.image == VK_NULL_HANDLE) return;
    if (mip >= tex.mipLevels) return;

    uint32_t reqCount = 0;
    vkGetImageSparseMemoryRequirements(m_device, tex.image, &reqCount, nullptr);
    if (reqCount == 0) return;

    std::vector<VkSparseImageMemoryRequirements> sparseReqs(reqCount);
    vkGetImageSparseMemoryRequirements(m_device, tex.image, &reqCount, sparseReqs.data());
    const auto& req = sparseReqs[0];

    VkExtent3D mipExtent{
        std::max(1u, tex.extent.width >> mip),
        std::max(1u, tex.extent.height >> mip),
        std::max(1u, tex.extent.depth >> mip)
    };

    const VkExtent3D granularity = req.formatProperties.imageGranularity;
    if (granularity.width == 0 || granularity.height == 0 || granularity.depth == 0) return;

    const uint32_t ox = tx * granularity.width;
    const uint32_t oy = ty * granularity.height;
    if (ox >= mipExtent.width || oy >= mipExtent.height) return;

    VkOffset3D offset{
        static_cast<int32_t>(ox),
        static_cast<int32_t>(oy),
        0
    };
    VkExtent3D extent{
        std::min(granularity.width, mipExtent.width - ox),
        std::min(granularity.height, mipExtent.height - oy),
        1
    };

    VkMemoryRequirements memReq{};
    vkGetImageMemoryRequirements(m_device, tex.image, &memReq);

    const uint32_t memoryTypeIndex = findMemoryType(m_physDevice, memReq.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (memoryTypeIndex == UINT32_MAX) return;

    VkDeviceSize allocSize = std::max<VkDeviceSize>(memReq.alignment, 64ull * 1024ull);
    VkMemoryAllocateInfo mai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    mai.allocationSize = allocSize;
    mai.memoryTypeIndex = memoryTypeIndex;

    VkDeviceMemory tileMemory = VK_NULL_HANDLE;
    if (vkAllocateMemory(m_device, &mai, nullptr, &tileMemory) != VK_SUCCESS) return;

    VkSparseImageMemoryBind bind{};
    bind.subresource.aspectMask = req.formatProperties.aspectMask != 0
        ? req.formatProperties.aspectMask
        : defaultAspectForFormat(tex.vkFormat);
    bind.subresource.mipLevel = mip;
    bind.subresource.arrayLayer = 0;
    bind.offset = offset;
    bind.extent = extent;
    bind.memory = tileMemory;
    bind.memoryOffset = 0;

    submitSparseImageBind(tex.image, bind);
    m_tileBindings.emplace(key, SparseTileBinding{texHandle, bind, tileMemory});
}

void VulkanAllocator::evictTile(TextureHandle texHandle, uint32_t tx,
                                uint32_t ty, uint32_t mip)
{
    if (!texHandle.valid() || m_pool == nullptr || texHandle.id >= m_pool->textures.size()) return;

    const uint64_t key = makeTileKey(texHandle, tx, ty, mip);
    auto it = m_tileBindings.find(key);
    if (it == m_tileBindings.end()) return;

    const auto& tex = m_pool->textures[texHandle.id];
    if (tex.image != VK_NULL_HANDLE) {
        VkSparseImageMemoryBind unbind = it->second.bind;
        unbind.memory = VK_NULL_HANDLE;
        unbind.memoryOffset = 0;
        submitSparseImageBind(tex.image, unbind);
    }

    if (it->second.memory != VK_NULL_HANDLE && m_device != VK_NULL_HANDLE)
        vkFreeMemory(m_device, it->second.memory, nullptr);

    m_tileBindings.erase(it);
}

uint64_t VulkanAllocator::budgetBytes() const noexcept
{
    if (!m_vma) return 0;
    VmaBudget budgets[VK_MAX_MEMORY_HEAPS]{};
    vmaGetHeapBudgets(m_vma, budgets);
    uint64_t total = 0;
    for (auto& b : budgets) total += b.budget;
    return total;
}

uint64_t VulkanAllocator::allocatedBytes() const noexcept
{
    if (!m_vma) return 0;
    VmaTotalStatistics stats{};
    vmaCalculateStatistics(m_vma, &stats);
    return stats.total.statistics.allocationBytes;
}

uint64_t VulkanAllocator::usedBytes() const noexcept
{
    if (!m_vma) return 0;
    VmaTotalStatistics stats{};
    vmaCalculateStatistics(m_vma, &stats);
    return stats.total.statistics.blockBytes;
}

} // namespace nexus::gfx
