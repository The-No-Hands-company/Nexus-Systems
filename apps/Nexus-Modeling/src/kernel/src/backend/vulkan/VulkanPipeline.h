#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  VulkanPipeline — pipeline ownership module
// ─────────────────────────────────────────────────────────────────────────────

#include <nexus/gfx/Device.h>

namespace nexus::gfx {

class VulkanDevice;

[[nodiscard]] PipelineHandle vkCreateGraphicsPipeline(VulkanDevice& device,
                                                      const GraphicsPipelineDesc& desc);
[[nodiscard]] PipelineHandle vkCreateMeshShaderPipeline(VulkanDevice& device,
                                                        const MeshShaderPipelineDesc& desc);
[[nodiscard]] PipelineHandle vkCreateComputePipeline(VulkanDevice& device,
                                                     const ComputePipelineDesc& desc);
[[nodiscard]] PipelineHandle vkCreateRayTracingPipeline(VulkanDevice& device,
                                                        const RayTracingPipelineDesc& desc);
void vkDestroyPipeline(VulkanDevice& device, PipelineHandle handle);

} // namespace nexus::gfx
