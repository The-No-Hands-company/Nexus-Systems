// ─────────────────────────────────────────────────────────────────────────────
//  VulkanBuffer — VMA-backed buffer creation / destruction
// ─────────────────────────────────────────────────────────────────────────────
#include "VulkanBuffer.h"
#include "VulkanUtils.h"
#include <vk_mem_alloc.h>
#include <cstring>
#include <stdexcept>

namespace nexus::gfx {

VulkanBuffer vkCreateBuffer(VmaAllocator vma, VkDevice device, const BufferDesc& desc)
{
    VulkanBuffer result{};
    result.size  = desc.sizeBytes;
    result.usage = desc.usage;

    VkBufferCreateInfo bci{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bci.size        = desc.sizeBytes;
    bci.usage       = vkutil::toVkBufferUsage(desc.usage);
    bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    // Ensure AS scratch / build-input buffers have the correct extra flags
    if (hasFlag(desc.usage, BufferUsage::RayTracingAS)) {
        bci.usage |= VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR;
    }

    VmaAllocationCreateInfo aci{};
    switch (desc.memory) {
    case MemoryHint::GpuOnly:
        aci.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
        break;
    case MemoryHint::CpuToGpu:
        aci.usage = VMA_MEMORY_USAGE_AUTO;
        aci.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT
                  | VMA_ALLOCATION_CREATE_MAPPED_BIT;
        break;
    case MemoryHint::GpuToCpu:
        aci.usage = VMA_MEMORY_USAGE_AUTO;
        aci.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT
                  | VMA_ALLOCATION_CREATE_MAPPED_BIT;
        break;
    case MemoryHint::Unified:
        aci.usage = VMA_MEMORY_USAGE_AUTO;
        aci.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT
                  | VMA_ALLOCATION_CREATE_MAPPED_BIT
                  | VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT;
        break;
    }

    // Prefer dedicated allocation for large buffers (> 64 MB)
    if (desc.sizeBytes > 64 * 1024 * 1024)
        aci.flags |= VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;

    VmaAllocationInfo allocInfo{};
    VkResult res = vmaCreateBuffer(vma, &bci, &aci,
                                   &result.handle, &result.allocation, &allocInfo);
    if (res != VK_SUCCESS)
        throw std::runtime_error("vmaCreateBuffer failed: " + std::to_string(res));

    result.mappedPtr = allocInfo.pMappedData;

    // Obtain Buffer Device Address for bindless / RT
    if (bci.usage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT) {
        VkBufferDeviceAddressInfo bdai{VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO};
        bdai.buffer          = result.handle;
        result.deviceAddress = vkGetBufferDeviceAddress(device, &bdai);
    }

    // Name the object in debug builds
    if (desc.debugName) {
        VkDebugUtilsObjectNameInfoEXT ni{VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT};
        ni.objectType   = VK_OBJECT_TYPE_BUFFER;
        ni.objectHandle = reinterpret_cast<uint64_t>(result.handle);
        ni.pObjectName  = desc.debugName;
        // vkSetDebugUtilsObjectNameEXT is obtained via vkGetDeviceProcAddr —
        // called opportunistically; ignore if not loaded.
        auto fn = reinterpret_cast<PFN_vkSetDebugUtilsObjectNameEXT>(
            vkGetDeviceProcAddr(device, "vkSetDebugUtilsObjectNameEXT"));
        if (fn) fn(device, &ni);
    }

    return result;
}

void vkDestroyBuffer(VmaAllocator vma, VulkanBuffer& buf)
{
    if (buf.handle != VK_NULL_HANDLE && vma != VK_NULL_HANDLE)
        vmaDestroyBuffer(vma, buf.handle, buf.allocation);
    buf = {};
}

StagingUpload vkPrepareBufferUpload(VmaAllocator vma, VkDevice device,
                                    const void* data, uint64_t sizeBytes)
{
    BufferDesc stagingDesc{};
    stagingDesc.sizeBytes = sizeBytes;
    stagingDesc.usage     = BufferUsage::TransferSrc;
    stagingDesc.memory    = MemoryHint::CpuToGpu;

    StagingUpload result{};
    result.staging = vkCreateBuffer(vma, device, stagingDesc);

    // Map and copy
    if (result.staging.mappedPtr) {
        std::memcpy(result.staging.mappedPtr, data, sizeBytes);
        vmaFlushAllocation(vma, result.staging.allocation, 0, VK_WHOLE_SIZE);
    }

    result.region = VkBufferCopy{ .srcOffset = 0, .dstOffset = 0, .size = sizeBytes };
    return result;
}

} // namespace nexus::gfx
