#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  Nexus GFX — GPU Memory Allocator interface
//
//  Backed by VMA (Vulkan Memory Allocator) on Vulkan.
//  Supports tiled/reserved resources for "infinite" scene streaming.
// ─────────────────────────────────────────────────────────────────────────────
#include <nexus/gfx/Types.h>
#include <cstdint>

namespace nexus::gfx {

struct AllocationInfo {
    uint64_t   sizeBytes   = 0;
    MemoryHint hint        = MemoryHint::GpuOnly;
    uint64_t   alignment   = 0;
    bool       isTiled     = false;   // sparse/reserved resource
};

class IGPUAllocator {
public:
    virtual ~IGPUAllocator() = default;

    // ── Raw sub-allocation (used internally by IDevice) ────────────────────
    virtual void* alloc  (const AllocationInfo& info) = 0;
    virtual void  free   (void* allocation)           = 0;

    // ── Tiled resource pages (maps to VkSparseMemoryBind / D3D12 tile mapping)
    virtual void  commitTile  (TextureHandle texture, uint32_t tileX, uint32_t tileY, uint32_t mipLevel) = 0;
    virtual void  evictTile   (TextureHandle texture, uint32_t tileX, uint32_t tileY, uint32_t mipLevel) = 0;

    // ── Budget queries ─────────────────────────────────────────────────────
    [[nodiscard]] virtual uint64_t budgetBytes()    const noexcept = 0;
    [[nodiscard]] virtual uint64_t allocatedBytes() const noexcept = 0;
    [[nodiscard]] virtual uint64_t usedBytes()      const noexcept = 0;
};

} // namespace nexus::gfx
