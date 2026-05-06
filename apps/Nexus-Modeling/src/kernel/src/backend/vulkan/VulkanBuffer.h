#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  VulkanBuffer — GPU buffer entry stored in the device resource pool.
//  Uses VMA for sub-allocation.  Exposed as an opaque handle externally.
// ─────────────────────────────────────────────────────────────────────────────
#include <nexus/gfx/Types.h>
#include <nexus/gfx/Device.h>
#include <vulkan/vulkan.h>

// Forward-declare VMA types (full header only in .cpp with VMA_IMPLEMENTATION)
struct VmaAllocation_T;
using VmaAllocation = VmaAllocation_T*;
struct VmaAllocator_T;
using VmaAllocator = VmaAllocator_T*;

namespace nexus::gfx {

struct VulkanBuffer {
    VkBuffer      handle     = VK_NULL_HANDLE;
    VmaAllocation allocation = VK_NULL_HANDLE;
    uint64_t      size       = 0;
    BufferUsage   usage      = BufferUsage::None;
    void*         mappedPtr  = nullptr;  // non-null for CpuToGpu/GpuToCpu/Unified
    VkDeviceAddress deviceAddress = 0;   // BDA for bindless / RT
};

// ── Factory / destructor (called from VulkanDevice) ───────────────────────────
VulkanBuffer vkCreateBuffer(VmaAllocator vma, VkDevice device, const BufferDesc& desc);
void         vkDestroyBuffer(VmaAllocator vma, VulkanBuffer& buf);

// ── Staged upload ─────────────────────────────────────────────────────────────
// Allocates a transient staging buffer, copies data, then issues a copy command.
// Caller must submit and wait before the staging buffer is freed.
struct StagingUpload {
    VulkanBuffer staging;
    VkBufferCopy region;
};
StagingUpload vkPrepareBufferUpload(VmaAllocator vma, VkDevice device,
                                    const void* data, uint64_t sizeBytes);

} // namespace nexus::gfx
