// ─────────────────────────────────────────────────────────────────────────────
//  VulkanQueue — queue utilities
// ─────────────────────────────────────────────────────────────────────────────
#include "VulkanQueue.h"

namespace nexus::gfx::vkqueue {

QueueFamilyTransferInfo resolveQueueFamilyTransfer(uint32_t srcQueueFamily,
												   uint32_t dstQueueFamily) noexcept
{
	QueueFamilyTransferInfo transfer{};
	if (srcQueueFamily == VK_QUEUE_FAMILY_IGNORED ||
		dstQueueFamily == VK_QUEUE_FAMILY_IGNORED ||
		srcQueueFamily == dstQueueFamily) {
		return transfer;
	}

	transfer.srcQueueFamilyIndex = srcQueueFamily;
	transfer.dstQueueFamilyIndex = dstQueueFamily;
	transfer.requiresOwnershipTransfer = true;
	return transfer;
}

void applyQueueFamilyTransfer(VkImageMemoryBarrier2& barrier,
							  const QueueFamilyTransferInfo& transfer) noexcept
{
	barrier.srcQueueFamilyIndex = transfer.srcQueueFamilyIndex;
	barrier.dstQueueFamilyIndex = transfer.dstQueueFamilyIndex;
}

void applyQueueFamilyTransfer(VkBufferMemoryBarrier2& barrier,
							  const QueueFamilyTransferInfo& transfer) noexcept
{
	barrier.srcQueueFamilyIndex = transfer.srcQueueFamilyIndex;
	barrier.dstQueueFamilyIndex = transfer.dstQueueFamilyIndex;
}

} // namespace nexus::gfx::vkqueue
