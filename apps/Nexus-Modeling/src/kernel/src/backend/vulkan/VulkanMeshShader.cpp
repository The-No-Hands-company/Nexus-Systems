// ─────────────────────────────────────────────────────────────────────────────
//  VulkanMeshShader — mesh shader runtime capability helpers
// ─────────────────────────────────────────────────────────────────────────────
#include "VulkanMeshShader.h"

#include "VulkanDevice.h"

namespace nexus::gfx::vkmesh {

bool isMeshShaderRuntimeEnabled(const VulkanDevice& device) noexcept
{
	return device.caps().meshShaders;
}

bool canCreateMeshShaderPipeline(const VulkanDevice& device,
								 const MeshShaderPipelineDesc& desc,
								 const char*& outReason) noexcept
{
	outReason = nullptr;

	if (!isMeshShaderRuntimeEnabled(device)) {
		outReason = "mesh shaders are not enabled on this runtime device";
		return false;
	}

	if (!desc.meshShader.valid() || !desc.fragmentShader.valid()) {
		outReason = "mesh and fragment shader handles are required";
		return false;
	}

	return true;
}

} // namespace nexus::gfx::vkmesh
