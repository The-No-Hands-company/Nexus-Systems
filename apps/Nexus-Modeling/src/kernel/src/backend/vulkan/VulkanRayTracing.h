#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  VulkanRayTracing — BLAS / TLAS build helpers
//
//  Requires VK_KHR_acceleration_structure.
//  Only called when caps().rayTracingTier >= 1.
// ─────────────────────────────────────────────────────────────────────────────
#include <nexus/gfx/Types.h>
#include "VulkanBuffer.h"
#include <vulkan/vulkan.h>
#include <span>
#include <vector>

struct VmaAllocator_T;
using VmaAllocator = VmaAllocator_T*;

namespace nexus::gfx {

// ── Stored acceleration structure ────────────────────────────────────────────
struct VulkanAccelStruct {
    VkAccelerationStructureKHR handle       = VK_NULL_HANDLE;
    VulkanBuffer               buffer;          // backing VRAM storage
    VkDeviceAddress            deviceAddress  = 0;
};

// ── RT function pointer table (loaded once from the device) ──────────────────
struct RTPfnTable {
    PFN_vkCreateAccelerationStructureKHR        createAS    = nullptr;
    PFN_vkDestroyAccelerationStructureKHR       destroyAS   = nullptr;
    PFN_vkGetAccelerationStructureBuildSizesKHR buildSizes  = nullptr;
    PFN_vkCmdBuildAccelerationStructuresKHR     cmdBuild    = nullptr;
    PFN_vkGetAccelerationStructureDeviceAddressKHR getAddr  = nullptr;

    void load(VkDevice device);
    [[nodiscard]] bool ready() const noexcept { return createAS != nullptr; }
};

// ── BLAS build ────────────────────────────────────────────────────────────────
// Synchronous: submits to the given queue and waits idle.
VulkanAccelStruct buildBLAS(
    VmaAllocator     vma,
    VkDevice         device,
    VkQueue          computeQueue,
    VkCommandPool    cmdPool,
    const RTPfnTable& pfn,
    VkBuffer          vertexBuffer,
    VkDeviceSize      vertexStride,
    uint32_t          vertexCount,
    VkBuffer          indexBuffer,
    uint32_t          indexCount
);

// ── TLAS build ────────────────────────────────────────────────────────────────
// instances: device addresses of BLASes + per-instance transforms
struct TLASInstance {
    VkDeviceAddress blasDeviceAddress;
    VkTransformMatrixKHR transform;  // column-major 3×4
    uint32_t instanceCustomIndex  = 0;
    uint8_t  mask                 = 0xFF;
    uint32_t shaderBindingOffset  = 0;
    VkGeometryInstanceFlagsKHR flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
};

VulkanAccelStruct buildTLAS(
    VmaAllocator      vma,
    VkDevice          device,
    VkQueue           computeQueue,
    VkCommandPool     cmdPool,
    const RTPfnTable& pfn,
    std::span<const TLASInstance> instances
);

void destroyAccelStruct(VmaAllocator vma, VkDevice device,
                         const RTPfnTable& pfn,
                         VulkanAccelStruct& as);

} // namespace nexus::gfx
