#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  VulkanQueue — queue-family ownership transfer helpers
// ─────────────────────────────────────────────────────────────────────────────

#include <vulkan/vulkan.h>

#include <cstdint>

namespace nexus::gfx::vkqueue {

struct QueueFamilyTransferInfo {
    uint32_t srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    uint32_t dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    bool requiresOwnershipTransfer = false;
};

[[nodiscard]] QueueFamilyTransferInfo resolveQueueFamilyTransfer(uint32_t srcQueueFamily,
                                                                 uint32_t dstQueueFamily) noexcept;

void applyQueueFamilyTransfer(VkImageMemoryBarrier2& barrier,
                              const QueueFamilyTransferInfo& transfer) noexcept;

void applyQueueFamilyTransfer(VkBufferMemoryBarrier2& barrier,
                              const QueueFamilyTransferInfo& transfer) noexcept;

} // namespace nexus::gfx::vkqueue