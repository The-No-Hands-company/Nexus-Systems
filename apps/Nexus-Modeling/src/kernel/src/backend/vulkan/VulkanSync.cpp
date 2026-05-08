// ─────────────────────────────────────────────────────────────────────────────
//  VulkanSync — fence / semaphore / event utilities
// ─────────────────────────────────────────────────────────────────────────────
#include "VulkanSync.h"

namespace nexus::gfx::vksync {

bool supportsTimelineSemaphores(VkPhysicalDevice physicalDevice) noexcept
{
	if (physicalDevice == VK_NULL_HANDLE) {
		return false;
	}

	VkPhysicalDeviceVulkan12Features features12{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};
	VkPhysicalDeviceFeatures2 features2{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
	features2.pNext = &features12;
	vkGetPhysicalDeviceFeatures2(physicalDevice, &features2);
	return features12.timelineSemaphore == VK_TRUE;
}

VkResult createBinarySemaphore(VkDevice device, VkSemaphore& outSemaphore) noexcept
{
	outSemaphore = VK_NULL_HANDLE;
	if (device == VK_NULL_HANDLE) {
		return VK_ERROR_INITIALIZATION_FAILED;
	}

	VkSemaphoreCreateInfo createInfo{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
	return vkCreateSemaphore(device, &createInfo, nullptr, &outSemaphore);
}

VkResult createTimelineSemaphore(VkDevice device,
								 uint64_t initialValue,
								 VkSemaphore& outSemaphore) noexcept
{
	outSemaphore = VK_NULL_HANDLE;
	if (device == VK_NULL_HANDLE) {
		return VK_ERROR_INITIALIZATION_FAILED;
	}

	VkSemaphoreTypeCreateInfo typeInfo{VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO};
	typeInfo.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
	typeInfo.initialValue = initialValue;

	VkSemaphoreCreateInfo createInfo{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
	createInfo.pNext = &typeInfo;
	return vkCreateSemaphore(device, &createInfo, nullptr, &outSemaphore);
}

VkResult queryTimelineSemaphoreValue(VkDevice device,
									 VkSemaphore semaphore,
									 uint64_t& outValue) noexcept
{
	outValue = 0;
	if (device == VK_NULL_HANDLE || semaphore == VK_NULL_HANDLE) {
		return VK_ERROR_INITIALIZATION_FAILED;
	}
	return vkGetSemaphoreCounterValue(device, semaphore, &outValue);
}

VkResult signalTimelineSemaphore(VkDevice device,
								 VkSemaphore semaphore,
								 uint64_t value) noexcept
{
	if (device == VK_NULL_HANDLE || semaphore == VK_NULL_HANDLE) {
		return VK_ERROR_INITIALIZATION_FAILED;
	}

	VkSemaphoreSignalInfo info{VK_STRUCTURE_TYPE_SEMAPHORE_SIGNAL_INFO};
	info.semaphore = semaphore;
	info.value = value;
	return vkSignalSemaphore(device, &info);
}

VkResult waitTimelineSemaphore(VkDevice device,
							   VkSemaphore semaphore,
							   uint64_t value,
							   uint64_t timeoutNs) noexcept
{
	if (device == VK_NULL_HANDLE || semaphore == VK_NULL_HANDLE) {
		return VK_ERROR_INITIALIZATION_FAILED;
	}

	VkSemaphoreWaitInfo info{VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO};
	info.semaphoreCount = 1;
	info.pSemaphores = &semaphore;
	info.pValues = &value;
	return vkWaitSemaphores(device, &info, timeoutNs);
}

VkResult queueSubmitWithTimeline(VkQueue queue,
								 std::span<const VkCommandBuffer> commandBuffers,
								 std::span<const VkSemaphore> waitSemaphores,
								 std::span<const uint64_t> waitValues,
								 std::span<const VkPipelineStageFlags> waitStages,
								 std::span<const VkSemaphore> signalSemaphores,
								 std::span<const uint64_t> signalValues,
								 VkFence signalFence) noexcept
{
	if (queue == VK_NULL_HANDLE) {
		return VK_ERROR_INITIALIZATION_FAILED;
	}

	VkTimelineSemaphoreSubmitInfo timelineInfo{VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO};
	timelineInfo.waitSemaphoreValueCount = static_cast<uint32_t>(waitValues.size());
	timelineInfo.pWaitSemaphoreValues = waitValues.empty() ? nullptr : waitValues.data();
	timelineInfo.signalSemaphoreValueCount = static_cast<uint32_t>(signalValues.size());
	timelineInfo.pSignalSemaphoreValues = signalValues.empty() ? nullptr : signalValues.data();

	VkSubmitInfo submitInfo{VK_STRUCTURE_TYPE_SUBMIT_INFO};
	submitInfo.pNext = &timelineInfo;
	submitInfo.waitSemaphoreCount = static_cast<uint32_t>(waitSemaphores.size());
	submitInfo.pWaitSemaphores = waitSemaphores.empty() ? nullptr : waitSemaphores.data();
	submitInfo.pWaitDstStageMask = waitStages.empty() ? nullptr : waitStages.data();
	submitInfo.commandBufferCount = static_cast<uint32_t>(commandBuffers.size());
	submitInfo.pCommandBuffers = commandBuffers.empty() ? nullptr : commandBuffers.data();
	submitInfo.signalSemaphoreCount = static_cast<uint32_t>(signalSemaphores.size());
	submitInfo.pSignalSemaphores = signalSemaphores.empty() ? nullptr : signalSemaphores.data();

	return vkQueueSubmit(queue, 1, &submitInfo, signalFence);
}

} // namespace nexus::gfx::vksync
