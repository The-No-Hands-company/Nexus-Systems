#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  VulkanShaderBindingTable — SBT layout computation
//
//  Pure arithmetic for laying out a ray-tracing shader binding table, separated
//  from any Vulkan calls so the alignment rules can be unit-tested without a GPU.
//
//  Vulkan layout rules (VK_KHR_ray_tracing_pipeline):
//   - Each record's stride within a region is align(handleSize, handleAlignment).
//   - Each region (raygen / miss / hit / callable) must start at an address that
//     is a multiple of shaderGroupBaseAlignment.
//   - The raygen region is special: its `size` must equal its `stride`.
// ─────────────────────────────────────────────────────────────────────────────

#include <cstdint>

namespace nexus::gfx {

struct SbtRegionLayout {
    uint64_t offset = 0; // byte offset from the start of the SBT buffer
    uint64_t stride = 0; // bytes between consecutive records
    uint64_t size   = 0; // total bytes of the region (stride * count, raygen: == stride)
};

struct SbtLayout {
    SbtRegionLayout raygen;
    SbtRegionLayout miss;
    SbtRegionLayout hit;
    SbtRegionLayout callable; // unused for now (size 0)
    uint64_t        totalSize = 0;
};

/// Round `value` up to the next multiple of `alignment` (alignment may be any
/// positive integer; Vulkan reports powers of two). A zero alignment is treated
/// as 1 (no alignment).
[[nodiscard]] constexpr uint64_t alignUp(uint64_t value, uint64_t alignment) noexcept
{
    if (alignment <= 1) {
        return value;
    }
    return ((value + alignment - 1) / alignment) * alignment;
}

/// Compute the SBT layout for a pipeline with one raygen group, `missCount` miss
/// groups, and `hitCount` hit groups (no callable groups).
///
/// `handleSize`      = VkPhysicalDeviceRayTracingPipelinePropertiesKHR::shaderGroupHandleSize
/// `handleAlignment` = ...::shaderGroupHandleAlignment
/// `baseAlignment`   = ...::shaderGroupBaseAlignment
[[nodiscard]] constexpr SbtLayout computeShaderBindingTableLayout(
    uint32_t handleSize,
    uint32_t handleAlignment,
    uint32_t baseAlignment,
    uint32_t missCount,
    uint32_t hitCount) noexcept
{
    const uint64_t handleSizeAligned = alignUp(handleSize, handleAlignment);

    SbtLayout layout{};

    // Raygen: exactly one record; size must equal stride, region base-aligned.
    layout.raygen.offset = 0;
    layout.raygen.stride = alignUp(handleSizeAligned, baseAlignment);
    layout.raygen.size   = layout.raygen.stride;

    // Miss: `missCount` records, region base aligned.
    layout.miss.offset = alignUp(layout.raygen.offset + layout.raygen.size, baseAlignment);
    layout.miss.stride = (missCount == 0) ? 0 : handleSizeAligned;
    layout.miss.size   = handleSizeAligned * missCount;

    // Hit: `hitCount` records, region base aligned.
    layout.hit.offset = alignUp(layout.miss.offset + layout.miss.size, baseAlignment);
    layout.hit.stride = (hitCount == 0) ? 0 : handleSizeAligned;
    layout.hit.size   = handleSizeAligned * hitCount;

    // Callable: unused — points just past the hit region.
    layout.callable.offset = layout.hit.offset + layout.hit.size;
    layout.callable.stride = 0;
    layout.callable.size   = 0;

    // Buffer size = end of the last used region (each region already starts
    // base-aligned; no need to pad the tail).
    layout.totalSize = layout.hit.offset + layout.hit.size;
    return layout;
}

} // namespace nexus::gfx
