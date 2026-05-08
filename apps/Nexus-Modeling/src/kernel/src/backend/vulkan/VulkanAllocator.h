#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  VulkanAllocator — wraps VMA (Vulkan Memory Allocator)
//  VMA header is included lazily so the interface compiles without the SDK.
// ─────────────────────────────────────────────────────────────────────────────
#include <nexus/gfx/Allocator.h>
#include <vulkan/vulkan.h>
#include <unordered_map>

// Forward-declare VmaAllocator so the header doesn't need vk_mem_alloc.h
struct VmaAllocator_T;
using VmaAllocator = VmaAllocator_T*;

namespace nexus::gfx {

struct VulkanResourcePool;

class VulkanAllocator final : public IGPUAllocator {
public:
    VulkanAllocator(VkInstance instance,
                    VkPhysicalDevice physDev,
                    VkDevice device,
                    VulkanResourcePool* pool,
                    VkQueue sparseQueue);
    VulkanAllocator(VmaAllocator existingVma,
                    VkPhysicalDevice physDev,
                    VkDevice device,
                    VulkanResourcePool* pool,
                    VkQueue sparseQueue) noexcept;
    ~VulkanAllocator() override;

    void* alloc  (const AllocationInfo& info) override;
    void  free   (void* allocation)           override;

    void  commitTile(TextureHandle, uint32_t, uint32_t, uint32_t) override;
    void  evictTile (TextureHandle, uint32_t, uint32_t, uint32_t) override;

    [[nodiscard]] uint64_t budgetBytes()    const noexcept override;
    [[nodiscard]] uint64_t allocatedBytes() const noexcept override;
    [[nodiscard]] uint64_t usedBytes()      const noexcept override;

    [[nodiscard]] VmaAllocator vma() const noexcept { return m_vma; }

private:
    struct SparseTileBinding {
        TextureHandle            texture{};
        VkSparseImageMemoryBind bind{};
        VkDeviceMemory          memory = VK_NULL_HANDLE;
    };

    [[nodiscard]] uint64_t makeTileKey(TextureHandle tex, uint32_t tileX, uint32_t tileY, uint32_t mipLevel) const noexcept;
    void submitSparseImageBind(VkImage image, const VkSparseImageMemoryBind& bind) const;

    VmaAllocator m_vma = nullptr;
    bool m_ownsVma = true;
    VkPhysicalDevice m_physDevice = VK_NULL_HANDLE;
    VkDevice         m_device     = VK_NULL_HANDLE;
    VulkanResourcePool* m_pool    = nullptr;
    VkQueue          m_sparseQueue = VK_NULL_HANDLE;
    std::unordered_map<uint64_t, SparseTileBinding> m_tileBindings;
};

// ── NullAllocator — used by NullDevice / headless ─────────────────────────────
class NullAllocator final : public IGPUAllocator {
public:
    void* alloc  (const AllocationInfo&) override { return nullptr; }
    void  free   (void*)                 override {}
    void  commitTile(TextureHandle, uint32_t, uint32_t, uint32_t) override {}
    void  evictTile (TextureHandle, uint32_t, uint32_t, uint32_t) override {}
    [[nodiscard]] uint64_t budgetBytes()    const noexcept override { return 0; }
    [[nodiscard]] uint64_t allocatedBytes() const noexcept override { return 0; }
    [[nodiscard]] uint64_t usedBytes()      const noexcept override { return 0; }
};

} // namespace nexus::gfx
