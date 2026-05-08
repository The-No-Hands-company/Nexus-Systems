#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  VulkanSync — timeline semaphore helpers
// ─────────────────────────────────────────────────────────────────────────────

#include <vulkan/vulkan.h>

#include <cstdint>
#include <span>

namespace nexus::gfx::vksync {

[[nodiscard]] bool supportsTimelineSemaphores(VkPhysicalDevice physicalDevice) noexcept;

[[nodiscard]] VkResult createBinarySemaphore(VkDevice device, VkSemaphore& outSemaphore) noexcept;
[[nodiscard]] VkResult createTimelineSemaphore(VkDevice device,
                                               uint64_t initialValue,
                                               VkSemaphore& outSemaphore) noexcept;
[[nodiscard]] VkResult queryTimelineSemaphoreValue(VkDevice device,
                                                   VkSemaphore semaphore,
                                                   uint64_t& outValue) noexcept;
[[nodiscard]] VkResult signalTimelineSemaphore(VkDevice device,
                                               VkSemaphore semaphore,
                                               uint64_t value) noexcept;
[[nodiscard]] VkResult waitTimelineSemaphore(VkDevice device,
                                             VkSemaphore semaphore,
                                             uint64_t value,
                                             uint64_t timeoutNs) noexcept;
[[nodiscard]] VkResult queueSubmitWithTimeline(VkQueue queue,
                                               std::span<const VkCommandBuffer> commandBuffers,
                                               std::span<const VkSemaphore> waitSemaphores,
                                               std::span<const uint64_t> waitValues,
                                               std::span<const VkPipelineStageFlags> waitStages,
                                               std::span<const VkSemaphore> signalSemaphores,
                                               std::span<const uint64_t> signalValues,
                                               VkFence signalFence) noexcept;

} // namespace nexus::gfx::vksync
