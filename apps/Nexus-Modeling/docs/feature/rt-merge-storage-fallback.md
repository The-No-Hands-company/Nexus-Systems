# RT Merge Storage Fallback

## Purpose

This document explains the runtime fallback implemented for Vulkan swapchains that do not support `VK_IMAGE_USAGE_STORAGE_BIT` on swapchain images.

The renderer uses a compute-based RT-merge pass to blend ray-traced output into the lit composite image without re-opening the composite render pass. That design requires the merge target to be in a storage-writable layout (`TextureLayout::General` or `TextureLayout::ShaderReadWrite`).

## Design summary

- `Renderer::render` records the usual deferred frame passes.
- After the composite pass, if ray tracing merge is active, the pipeline transitions the color output to a storage-writable layout and dispatches the merge compute pass.
- This avoids a second render-pass reopen on the swapchain image, which would clear the composite result because the composite pass uses a clear load op.

## Vulkan swapchain capability issue

Some Vulkan drivers reject swapchain creation when `VK_IMAGE_USAGE_STORAGE_BIT` is requested in `VkSwapchainCreateInfoKHR::imageUsage`.

To preserve RT-merge behavior on these platforms, the Vulkan backend now falls back:

1. Attempt swapchain creation with:
   - `VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT`
   - `VK_IMAGE_USAGE_TRANSFER_DST_BIT`
   - `VK_IMAGE_USAGE_STORAGE_BIT`
2. If `vkCreateSwapchainKHR` fails, retry without `VK_IMAGE_USAGE_STORAGE_BIT`.
3. Record the actual `VkImageUsageFlags` used by the swapchain so upper layers can inspect support.
4. If the swapchain was created without `VK_IMAGE_USAGE_STORAGE_BIT`, allocate an intermediate storage-backed texture for each image.
5. Render and merge into that intermediate texture.
6. At `VulkanFrameScheduler::endFrame`, copy the intermediate texture into the real swapchain image using transfer commands and then present.

## Code references

- `src/kernel/src/render/Renderer.cpp`
  - RT-merge compute dispatch and final color target layout transition
  - `TextureLayout::General` / `TextureLayout::TransferSrc` handling

- `src/kernel/src/backend/vulkan/VulkanSwapchain.cpp`
  - swapchain create fallback when `VK_IMAGE_USAGE_STORAGE_BIT` is unsupported
  - `vkImageUsageFlags()` accessor

- `src/kernel/src/backend/vulkan/VulkanFrameScheduler.cpp`
  - `registerSwapchainImages()` creates intermediate storage textures when needed
  - `endFrame()` copies intermediate results into the external swapchain image

- `tests/kernel/test_VulkanSwapchainStorageUsage.cpp`
  - runtime assertion and skip behavior for swapchain storage support

- `tests/kernel/test_RenderPipelineV1.cpp`
  - validator coverage for RT merge ordering and storage-layout requirements

## Validation

The fallback is guarded by the swapchain usage flags and only engages when `VK_IMAGE_USAGE_STORAGE_BIT` was unavailable.

The `RenderGraphValidator` ensures:

- `RayTracingMerge` does not appear before `RayTracing`
- `RayTracingMerge` target is in a storage-writable layout

## Notes

This fallback maintains the renderer's compute-merge design while preserving compatibility on stricter Vulkan implementations. It is intended for driver/platform compatibility rather than a preferred fast path.
