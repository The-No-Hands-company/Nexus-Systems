#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  VulkanMeshShader — mesh-shader runtime capability helpers
// ─────────────────────────────────────────────────────────────────────────────

#include <nexus/gfx/Device.h>

namespace nexus::gfx {

class VulkanDevice;

namespace vkmesh {

[[nodiscard]] bool isMeshShaderRuntimeEnabled(const VulkanDevice& device) noexcept;

[[nodiscard]] bool canCreateMeshShaderPipeline(const VulkanDevice& device,
                                               const MeshShaderPipelineDesc& desc,
                                               const char*& outReason) noexcept;

} // namespace vkmesh

} // namespace nexus::gfx