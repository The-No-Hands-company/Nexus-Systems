// ─────────────────────────────────────────────────────────────────────────────
//  VulkanRayTracing — BLAS / TLAS build implementation
// ─────────────────────────────────────────────────────────────────────────────
#include "VulkanRayTracing.h"
#include "VulkanShaderBindingTable.h"
#include "VulkanUtils.h"
#include <vk_mem_alloc.h>
#include <stdexcept>
#include <cstring>

namespace nexus::gfx {

// ── Function pointer table ────────────────────────────────────────────────────
void RTPfnTable::load(VkDevice device)
{
#define LOADFN(field, vkname) \
    field = reinterpret_cast<decltype(field)>(vkGetDeviceProcAddr(device, #vkname))
    LOADFN(createAS,   vkCreateAccelerationStructureKHR);
    LOADFN(destroyAS,  vkDestroyAccelerationStructureKHR);
    LOADFN(buildSizes, vkGetAccelerationStructureBuildSizesKHR);
    LOADFN(cmdBuild,   vkCmdBuildAccelerationStructuresKHR);
    LOADFN(getAddr,    vkGetAccelerationStructureDeviceAddressKHR);
#undef LOADFN
}

// ── Internal helper: allocate and run a single-use command buffer ─────────────
static VkCommandBuffer beginSingleUse(VkDevice device, VkCommandPool pool)
{
    VkCommandBufferAllocateInfo ai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    ai.commandPool        = pool;
    ai.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    ai.commandBufferCount = 1;
    VkCommandBuffer cmd = VK_NULL_HANDLE;
    vkAllocateCommandBuffers(device, &ai, &cmd);

    VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &bi);
    return cmd;
}

static void endAndSubmitSingleUse(VkDevice device, VkCommandPool pool,
                                   VkQueue queue, VkCommandBuffer cmd)
{
    vkEndCommandBuffer(cmd);
    VkSubmitInfo si{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    si.commandBufferCount = 1;
    si.pCommandBuffers    = &cmd;

    VkFenceCreateInfo fi{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    VkFence fence = VK_NULL_HANDLE;
    vkCreateFence(device, &fi, nullptr, &fence);
    vkQueueSubmit(queue, 1, &si, fence);
    vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX);
    vkDestroyFence(device, fence, nullptr);
    vkFreeCommandBuffers(device, pool, 1, &cmd);
}

// ── Scratch buffer helper ─────────────────────────────────────────────────────
static VulkanBuffer allocScratch(VmaAllocator vma, VkDevice device, VkDeviceSize size)
{
    BufferDesc desc{};
    desc.sizeBytes = size;
    desc.usage     = BufferUsage::StorageBuffer;  // scratch needs shader-device-address
    desc.memory    = MemoryHint::GpuOnly;
    // Add AS scratch flag via direct creation (bypassing the usage enum for the AS bits)
    VkBufferCreateInfo bci{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bci.size        = size;
    bci.usage       = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
                    | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo aci{};
    aci.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

    VulkanBuffer buf{};
    buf.size = size;
    VmaAllocationInfo ai{};
    vmaCreateBuffer(vma, &bci, &aci, &buf.handle, &buf.allocation, &ai);

    VkBufferDeviceAddressInfo bdai{VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO};
    bdai.buffer       = buf.handle;
    buf.deviceAddress = vkGetBufferDeviceAddress(device, &bdai);
    return buf;
}

// ── BLAS ──────────────────────────────────────────────────────────────────────
VulkanAccelStruct buildBLAS(VmaAllocator vma, VkDevice device,
                             VkQueue computeQueue, VkCommandPool cmdPool,
                             const RTPfnTable& pfn,
                             VkBuffer vertexBuffer, VkDeviceSize vertexStride,
                             uint32_t vertexCount,
                             VkBuffer indexBuffer, uint32_t indexCount)
{
    if (!pfn.ready()) throw std::runtime_error("buildBLAS: RT extension not loaded");

    // Obtain device addresses for input data
    auto getAddr = [&](VkBuffer b) -> VkDeviceAddress {
        VkBufferDeviceAddressInfo i{VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO};
        i.buffer = b;
        return vkGetBufferDeviceAddress(device, &i);
    };

    // ── Geometry descriptor ───────────────────────────────────────────────────
    VkAccelerationStructureGeometryTrianglesDataKHR triData{
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR};
    triData.vertexFormat             = VK_FORMAT_R32G32B32_SFLOAT;
    triData.vertexData.deviceAddress = getAddr(vertexBuffer);
    triData.vertexStride             = vertexStride;
    triData.maxVertex                = vertexCount - 1;
    triData.indexType                = VK_INDEX_TYPE_UINT32;
    triData.indexData.deviceAddress  = getAddr(indexBuffer);

    VkAccelerationStructureGeometryKHR geom{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR};
    geom.geometryType       = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
    geom.flags              = VK_GEOMETRY_OPAQUE_BIT_KHR;
    geom.geometry.triangles = triData;

    // ── Build sizes ───────────────────────────────────────────────────────────
    VkAccelerationStructureBuildGeometryInfoKHR buildInfo{
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR};
    buildInfo.type          = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
    buildInfo.flags         = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR
                            | VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_COMPACTION_BIT_KHR;
    buildInfo.geometryCount = 1;
    buildInfo.pGeometries   = &geom;

    const uint32_t primCount = indexCount / 3;
    VkAccelerationStructureBuildSizesInfoKHR sizeInfo{
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR};
    pfn.buildSizes(device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
                   &buildInfo, &primCount, &sizeInfo);

    // ── Allocate AS storage buffer ────────────────────────────────────────────
    VkBufferCreateInfo asBufferCI{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    asBufferCI.size        = sizeInfo.accelerationStructureSize;
    asBufferCI.usage       = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR
                           | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    asBufferCI.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo asAllocCI{};
    asAllocCI.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

    VulkanAccelStruct result{};
    result.buffer.size = sizeInfo.accelerationStructureSize;
    vmaCreateBuffer(vma, &asBufferCI, &asAllocCI,
                    &result.buffer.handle, &result.buffer.allocation, nullptr);

    // ── Create acceleration structure ─────────────────────────────────────────
    VkAccelerationStructureCreateInfoKHR asCI{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR};
    asCI.buffer = result.buffer.handle;
    asCI.size   = sizeInfo.accelerationStructureSize;
    asCI.type   = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
    pfn.createAS(device, &asCI, nullptr, &result.handle);

    // ── Scratch buffer ────────────────────────────────────────────────────────
    VulkanBuffer scratch = allocScratch(vma, device, sizeInfo.buildScratchSize);

    // ── Record + submit build ─────────────────────────────────────────────────
    buildInfo.mode                    = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    buildInfo.dstAccelerationStructure= result.handle;
    buildInfo.scratchData.deviceAddress = scratch.deviceAddress;

    VkAccelerationStructureBuildRangeInfoKHR range{ primCount, 0, 0, 0 };
    const VkAccelerationStructureBuildRangeInfoKHR* pRange = &range;

    VkCommandBuffer cmd = beginSingleUse(device, cmdPool);
    pfn.cmdBuild(cmd, 1, &buildInfo, &pRange);
    endAndSubmitSingleUse(device, cmdPool, computeQueue, cmd);

    // ── Get device address ────────────────────────────────────────────────────
    VkAccelerationStructureDeviceAddressInfoKHR addrInfo{
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR};
    addrInfo.accelerationStructure = result.handle;
    result.deviceAddress = pfn.getAddr(device, &addrInfo);

    // Free scratch
    vkDestroyBuffer(device, scratch.handle, nullptr);
    vmaFreeMemory(vma, scratch.allocation);

    return result;
}

// ── TLAS ──────────────────────────────────────────────────────────────────────
VulkanAccelStruct buildTLAS(VmaAllocator vma, VkDevice device,
                             VkQueue computeQueue, VkCommandPool cmdPool,
                             const RTPfnTable& pfn,
                             std::span<const TLASInstance> instances)
{
    if (!pfn.ready()) throw std::runtime_error("buildTLAS: RT extension not loaded");
    if (instances.empty()) return {};

    // ── Pack VkAccelerationStructureInstanceKHR array into a GPU buffer ───────
    std::vector<VkAccelerationStructureInstanceKHR> vkInsts;
    vkInsts.reserve(instances.size());
    for (const auto& inst : instances) {
        VkAccelerationStructureInstanceKHR vi{};
        vi.transform                              = inst.transform;
        vi.instanceCustomIndex                    = inst.instanceCustomIndex;
        vi.mask                                   = inst.mask;
        vi.instanceShaderBindingTableRecordOffset = inst.shaderBindingOffset;
        vi.flags                                  = inst.flags;
        vi.accelerationStructureReference         = inst.blasDeviceAddress;
        vkInsts.push_back(vi);
    }

    const VkDeviceSize instBufSize = vkInsts.size() * sizeof(VkAccelerationStructureInstanceKHR);
    BufferDesc instBufDesc{};
    instBufDesc.sizeBytes = instBufSize;
    instBufDesc.usage     = BufferUsage::RayTracingAS | BufferUsage::TransferDst;
    instBufDesc.memory    = MemoryHint::GpuOnly;

    // Upload via staging
    VkBufferCreateInfo instBCI{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    instBCI.size    = instBufSize;
    instBCI.usage   = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR
                    | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
                    | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    instBCI.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo instACI{.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE};
    VulkanBuffer instBuf{};
    instBuf.size = instBufSize;
    vmaCreateBuffer(vma, &instBCI, &instACI, &instBuf.handle, &instBuf.allocation, nullptr);

    {
        VkBufferDeviceAddressInfo bdai{VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO};
        bdai.buffer = instBuf.handle;
        instBuf.deviceAddress = vkGetBufferDeviceAddress(device, &bdai);
    }

    // Upload instance data via staging buffer
    {
        VkBufferCreateInfo staBCI{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        staBCI.size       = instBufSize;
        staBCI.usage      = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        staBCI.sharingMode= VK_SHARING_MODE_EXCLUSIVE;
        VmaAllocationCreateInfo staACI{};
        staACI.usage = VMA_MEMORY_USAGE_AUTO;
        staACI.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;

        VkBuffer stageBuf = VK_NULL_HANDLE;
        VmaAllocation stageAlloc = VK_NULL_HANDLE;
        VmaAllocationInfo stageInfo{};
        vmaCreateBuffer(vma, &staBCI, &staACI, &stageBuf, &stageAlloc, &stageInfo);
        std::memcpy(stageInfo.pMappedData, vkInsts.data(), instBufSize);
        vmaFlushAllocation(vma, stageAlloc, 0, VK_WHOLE_SIZE);

        VkCommandBuffer cmd = beginSingleUse(device, cmdPool);
        VkBufferCopy cp{ 0, 0, instBufSize };
        vkCmdCopyBuffer(cmd, stageBuf, instBuf.handle, 1, &cp);
        endAndSubmitSingleUse(device, cmdPool, computeQueue, cmd);

        vkDestroyBuffer(device, stageBuf, nullptr);
        vmaFreeMemory(vma, stageAlloc);
    }

    // ── Geometry ──────────────────────────────────────────────────────────────
    VkAccelerationStructureGeometryInstancesDataKHR instData{
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR};
    instData.arrayOfPointers   = VK_FALSE;
    instData.data.deviceAddress= instBuf.deviceAddress;

    VkAccelerationStructureGeometryKHR geom{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR};
    geom.geometryType        = VK_GEOMETRY_TYPE_INSTANCES_KHR;
    geom.geometry.instances  = instData;

    // ── Build sizes ───────────────────────────────────────────────────────────
    VkAccelerationStructureBuildGeometryInfoKHR buildInfo{
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR};
    buildInfo.type          = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
    buildInfo.flags         = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
    buildInfo.geometryCount = 1;
    buildInfo.pGeometries   = &geom;

    const uint32_t instCount = static_cast<uint32_t>(instances.size());
    VkAccelerationStructureBuildSizesInfoKHR sizeInfo{
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR};
    pfn.buildSizes(device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
                   &buildInfo, &instCount, &sizeInfo);

    // ── AS storage buffer ─────────────────────────────────────────────────────
    VkBufferCreateInfo asBufferCI{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    asBufferCI.size    = sizeInfo.accelerationStructureSize;
    asBufferCI.usage   = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR
                       | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    asBufferCI.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo asAllocCI{.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE};
    VulkanAccelStruct result{};
    result.buffer.size = sizeInfo.accelerationStructureSize;
    vmaCreateBuffer(vma, &asBufferCI, &asAllocCI,
                    &result.buffer.handle, &result.buffer.allocation, nullptr);

    VkAccelerationStructureCreateInfoKHR asCI{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR};
    asCI.buffer = result.buffer.handle;
    asCI.size   = sizeInfo.accelerationStructureSize;
    asCI.type   = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
    pfn.createAS(device, &asCI, nullptr, &result.handle);

    // ── Build ─────────────────────────────────────────────────────────────────
    VulkanBuffer scratch = allocScratch(vma, device, sizeInfo.buildScratchSize);

    buildInfo.mode                    = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    buildInfo.dstAccelerationStructure= result.handle;
    buildInfo.scratchData.deviceAddress = scratch.deviceAddress;

    VkAccelerationStructureBuildRangeInfoKHR range{ instCount, 0, 0, 0 };
    const VkAccelerationStructureBuildRangeInfoKHR* pRange = &range;

    VkCommandBuffer cmd = beginSingleUse(device, cmdPool);
    pfn.cmdBuild(cmd, 1, &buildInfo, &pRange);
    endAndSubmitSingleUse(device, cmdPool, computeQueue, cmd);

    // ── Device address ────────────────────────────────────────────────────────
    VkAccelerationStructureDeviceAddressInfoKHR addrInfo{
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR};
    addrInfo.accelerationStructure = result.handle;
    result.deviceAddress = pfn.getAddr(device, &addrInfo);

    // Cleanup scratch + instance buffer
    vkDestroyBuffer(device, scratch.handle, nullptr);
    vmaFreeMemory(vma, scratch.allocation);
    vkDestroyBuffer(device, instBuf.handle, nullptr);
    vmaFreeMemory(vma, instBuf.allocation);

    return result;
}

// ── Destroy ───────────────────────────────────────────────────────────────────
void destroyAccelStruct(VmaAllocator vma, VkDevice device,
                         const RTPfnTable& pfn, VulkanAccelStruct& as)
{
    if (as.handle != VK_NULL_HANDLE && pfn.destroyAS)
        pfn.destroyAS(device, as.handle, nullptr);
    if (as.buffer.handle != VK_NULL_HANDLE && vma)
        vmaDestroyBuffer(vma, as.buffer.handle, as.buffer.allocation);
    as = {};
}

// ── Shader binding table ──────────────────────────────────────────────────────
VulkanShaderBindingTableGpu buildShaderBindingTable(
    VmaAllocator vma, VkDevice device, VkPipeline rtPipeline,
    uint32_t handleSize, uint32_t handleAlignment, uint32_t baseAlignment,
    uint32_t missCount, uint32_t hitCount)
{
    VulkanShaderBindingTableGpu sbt{};
    if (handleSize == 0 || rtPipeline == VK_NULL_HANDLE || vma == nullptr) {
        return sbt;
    }

    auto pfnGetHandles = reinterpret_cast<PFN_vkGetRayTracingShaderGroupHandlesKHR>(
        vkGetDeviceProcAddr(device, "vkGetRayTracingShaderGroupHandlesKHR"));
    if (!pfnGetHandles) {
        return sbt;
    }

    const uint32_t groupCount = 1u + missCount + hitCount;
    const SbtLayout layout = computeShaderBindingTableLayout(
        handleSize, handleAlignment, baseAlignment, missCount, hitCount);

    // Fetch all group handles, tightly packed at `handleSize` each.
    std::vector<uint8_t> handles(static_cast<size_t>(groupCount) * handleSize);
    if (pfnGetHandles(device, rtPipeline, 0, groupCount,
                      handles.size(), handles.data()) != VK_SUCCESS) {
        return sbt;
    }

    // Host-visible, device-address SBT buffer (small; persistently mapped).
    VkBufferCreateInfo bci{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bci.size        = layout.totalSize;
    bci.usage       = VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR
                    | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo aci{};
    aci.usage = VMA_MEMORY_USAGE_AUTO;
    aci.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT
              | VMA_ALLOCATION_CREATE_MAPPED_BIT;

    VmaAllocationInfo allocInfo{};
    if (vmaCreateBuffer(vma, &bci, &aci, &sbt.buffer, &sbt.allocation, &allocInfo) != VK_SUCCESS) {
        return VulkanShaderBindingTableGpu{};
    }

    // Copy each group's handle into its region slot.
    auto* dst = static_cast<uint8_t*>(allocInfo.pMappedData);
    auto handleAt = [&](uint32_t group) {
        return handles.data() + static_cast<size_t>(group) * handleSize;
    };
    std::memcpy(dst + layout.raygen.offset, handleAt(0), handleSize);
    for (uint32_t i = 0; i < missCount; ++i) {
        std::memcpy(dst + layout.miss.offset + i * layout.miss.stride, handleAt(1 + i), handleSize);
    }
    for (uint32_t i = 0; i < hitCount; ++i) {
        std::memcpy(dst + layout.hit.offset + i * layout.hit.stride,
                    handleAt(1 + missCount + i), handleSize);
    }

    // Resolve device-address regions for vkCmdTraceRaysKHR.
    VkBufferDeviceAddressInfo bdai{VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO};
    bdai.buffer = sbt.buffer;
    const VkDeviceAddress base = vkGetBufferDeviceAddress(device, &bdai);

    sbt.raygen.deviceAddress = base + layout.raygen.offset;
    sbt.raygen.stride        = layout.raygen.stride;
    sbt.raygen.size          = layout.raygen.size;
    if (missCount > 0) {
        sbt.miss.deviceAddress = base + layout.miss.offset;
        sbt.miss.stride        = layout.miss.stride;
        sbt.miss.size          = layout.miss.size;
    }
    if (hitCount > 0) {
        sbt.hit.deviceAddress = base + layout.hit.offset;
        sbt.hit.stride        = layout.hit.stride;
        sbt.hit.size          = layout.hit.size;
    }
    // Callable region left zeroed (unused).
    return sbt;
}

} // namespace nexus::gfx
