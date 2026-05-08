// ─────────────────────────────────────────────────────────────────────────────
//  VulkanPipeline — graphics / compute / RT pipeline creation
// ─────────────────────────────────────────────────────────────────────────────
#include "VulkanPipeline.h"

#include "VulkanCommandBuffer.h"
#include "VulkanDevice.h"
#include "VulkanMeshShader.h"
#include "VulkanUtils.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <vector>

namespace nexus::gfx {

namespace {

inline uint64_t fnv1a_hash(const void* data, size_t size)
{
	constexpr uint64_t FNV_offset_basis = 14695981039346656037ULL;
	constexpr uint64_t FNV_prime = 1099511628211ULL;
	uint64_t hash = FNV_offset_basis;
	const auto* bytes = static_cast<const uint8_t*>(data);
	for (size_t i = 0; i < size; ++i) {
		hash ^= bytes[i];
		hash *= FNV_prime;
	}
	return hash;
}

uint64_t hashGraphicsPipelineDesc(const GraphicsPipelineDesc& desc)
{
	uint64_t hash = 0;
	hash ^= fnv1a_hash(&desc.vertexShader, sizeof(desc.vertexShader));
	hash ^= fnv1a_hash(&desc.fragmentShader, sizeof(desc.fragmentShader));
	hash ^= fnv1a_hash(&desc.topology, sizeof(desc.topology));
	hash ^= fnv1a_hash(&desc.polygonMode, sizeof(desc.polygonMode));
	hash ^= fnv1a_hash(&desc.cullMode, sizeof(desc.cullMode));
	hash ^= fnv1a_hash(&desc.frontFace, sizeof(desc.frontFace));
	hash ^= fnv1a_hash(&desc.depthTest, sizeof(desc.depthTest));
	hash ^= fnv1a_hash(&desc.depthWrite, sizeof(desc.depthWrite));
	hash ^= fnv1a_hash(&desc.depthCompare, sizeof(desc.depthCompare));
	hash ^= fnv1a_hash(&desc.stencilEnable, sizeof(desc.stencilEnable));
	hash ^= fnv1a_hash(&desc.blendEnable, sizeof(desc.blendEnable));
	hash ^= fnv1a_hash(&desc.colorWriteMask, sizeof(desc.colorWriteMask));
	hash ^= fnv1a_hash(&desc.srcColorBlend, sizeof(desc.srcColorBlend));
	hash ^= fnv1a_hash(&desc.dstColorBlend, sizeof(desc.dstColorBlend));
	hash ^= fnv1a_hash(&desc.colorBlendOp, sizeof(desc.colorBlendOp));
	hash ^= fnv1a_hash(&desc.srcAlphaBlend, sizeof(desc.srcAlphaBlend));
	hash ^= fnv1a_hash(&desc.dstAlphaBlend, sizeof(desc.dstAlphaBlend));
	hash ^= fnv1a_hash(&desc.alphaBlendOp, sizeof(desc.alphaBlendOp));
	hash ^= fnv1a_hash(&desc.lineWidth, sizeof(desc.lineWidth));
	hash ^= fnv1a_hash(&desc.colorAttachmentFormat, sizeof(desc.colorAttachmentFormat));
	hash ^= fnv1a_hash(&desc.depthAttachmentFormat, sizeof(desc.depthAttachmentFormat));
	return hash;
}

uint64_t hashMeshShaderPipelineDesc(const MeshShaderPipelineDesc& desc)
{
	uint64_t hash = 0;
	hash ^= fnv1a_hash(&desc.taskShader, sizeof(desc.taskShader));
	hash ^= fnv1a_hash(&desc.meshShader, sizeof(desc.meshShader));
	hash ^= fnv1a_hash(&desc.fragmentShader, sizeof(desc.fragmentShader));
	hash ^= fnv1a_hash(&desc.depthTest, sizeof(desc.depthTest));
	hash ^= fnv1a_hash(&desc.depthWrite, sizeof(desc.depthWrite));
	return hash;
}

uint64_t hashComputePipelineDesc(const ComputePipelineDesc& desc)
{
	return fnv1a_hash(&desc.computeShader, sizeof(desc.computeShader));
}

uint64_t hashRayTracingPipelineDesc(const RayTracingPipelineDesc& desc)
{
	uint64_t hash = 0;
	hash ^= fnv1a_hash(&desc.rayGenShader, sizeof(desc.rayGenShader));
	if (desc.missShaders.data())
		hash ^= fnv1a_hash(desc.missShaders.data(), desc.missShaders.size_bytes());
	if (desc.closestHitShaders.data())
		hash ^= fnv1a_hash(desc.closestHitShaders.data(), desc.closestHitShaders.size_bytes());
	if (desc.anyHitShaders.data())
		hash ^= fnv1a_hash(desc.anyHitShaders.data(), desc.anyHitShaders.size_bytes());
	hash ^= fnv1a_hash(&desc.maxRecursionDepth, sizeof(desc.maxRecursionDepth));
	return hash;
}

} // namespace

PipelineHandle vkCreateGraphicsPipeline(VulkanDevice& dev, const GraphicsPipelineDesc& desc)
{
	auto* pool = dev.resourcePool();
	const VkDevice device = dev.logical();
	if (!pool || device == VK_NULL_HANDLE) return {};

	if (!desc.vertexShader.valid() || !desc.fragmentShader.valid()) {
		std::fprintf(stderr, "createGraphicsPipeline: vertex/fragment shader handles are required\n");
		return {};
	}
	if (desc.vertexShader.id >= pool->shaders.size() ||
		desc.fragmentShader.id >= pool->shaders.size()) {
		std::fprintf(stderr, "createGraphicsPipeline: shader handle out of range\n");
		return {};
	}

	const uint64_t descHash = hashGraphicsPipelineDesc(desc);
	auto it = pool->psoCacheGraphics.find(descHash);
	if (it != pool->psoCacheGraphics.end()) {
		PipelineHandle h{};
		h.id = it->second;
		return h;
	}

	const VkShaderModule vs = pool->shaders[desc.vertexShader.id];
	const VkShaderModule fs = pool->shaders[desc.fragmentShader.id];
	if (vs == VK_NULL_HANDLE || fs == VK_NULL_HANDLE) {
		std::fprintf(stderr, "createGraphicsPipeline: shader module is null\n");
		return {};
	}

	VkPushConstantRange pcRange{};
	pcRange.stageFlags = VK_SHADER_STAGE_ALL;
	pcRange.offset = 0;
	pcRange.size = 128;

	VkPipelineLayoutCreateInfo lci{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
	lci.setLayoutCount = 0;
	lci.pushConstantRangeCount = 1;
	lci.pPushConstantRanges = &pcRange;
	VkPipelineLayout layout = VK_NULL_HANDLE;
	if (vkCreatePipelineLayout(device, &lci, nullptr, &layout) != VK_SUCCESS) {
		std::fprintf(stderr, "createGraphicsPipeline: vkCreatePipelineLayout failed\n");
		return {};
	}

	auto toVkTopology = [](Topology t) -> VkPrimitiveTopology {
		switch (t) {
		case Topology::TriangleStrip: return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
		case Topology::LineList: return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
		case Topology::LineStrip: return VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;
		case Topology::PointList: return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
		case Topology::TriangleList:
		default: return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		}
	};

	VkPipelineShaderStageCreateInfo stages[2]{};
	stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
	stages[0].module = vs;
	stages[0].pName = "main";

	stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	stages[1].module = fs;
	stages[1].pName = "main";

	VkPipelineVertexInputStateCreateInfo vi{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};

	VkPipelineInputAssemblyStateCreateInfo ia{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
	ia.topology = toVkTopology(desc.topology);
	ia.primitiveRestartEnable = VK_FALSE;

	VkPipelineViewportStateCreateInfo vp{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
	vp.viewportCount = 1;
	vp.scissorCount = 1;

	VkPipelineRasterizationStateCreateInfo rs{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
	rs.depthClampEnable = VK_FALSE;
	rs.rasterizerDiscardEnable = VK_FALSE;
	rs.polygonMode = vkutil::toVkPolygonMode(desc.polygonMode);
	rs.cullMode = vkutil::toVkCullMode(desc.cullMode);
	rs.frontFace = vkutil::toVkFrontFace(desc.frontFace);
	rs.depthBiasEnable = VK_FALSE;
	rs.lineWidth = desc.lineWidth;

	VkPipelineMultisampleStateCreateInfo ms{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
	ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

	VkPipelineDepthStencilStateCreateInfo ds{VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
	ds.depthTestEnable = desc.depthTest ? VK_TRUE : VK_FALSE;
	ds.depthWriteEnable = desc.depthWrite ? VK_TRUE : VK_FALSE;
	ds.depthCompareOp = vkutil::toVkCompareOp(desc.depthCompare);
	ds.depthBoundsTestEnable = VK_FALSE;
	ds.stencilTestEnable = desc.stencilEnable ? VK_TRUE : VK_FALSE;

	VkPipelineColorBlendAttachmentState cbAttach{};
	cbAttach.blendEnable = desc.blendEnable ? VK_TRUE : VK_FALSE;
	cbAttach.colorWriteMask = static_cast<VkColorComponentFlags>(desc.colorWriteMask & 0xF);
	if (desc.blendEnable) {
		cbAttach.srcColorBlendFactor = vkutil::toVkBlendFactor(desc.srcColorBlend);
		cbAttach.dstColorBlendFactor = vkutil::toVkBlendFactor(desc.dstColorBlend);
		cbAttach.colorBlendOp = vkutil::toVkBlendOp(desc.colorBlendOp);
		cbAttach.srcAlphaBlendFactor = vkutil::toVkBlendFactor(desc.srcAlphaBlend);
		cbAttach.dstAlphaBlendFactor = vkutil::toVkBlendFactor(desc.dstAlphaBlend);
		cbAttach.alphaBlendOp = vkutil::toVkBlendOp(desc.alphaBlendOp);
	}

	VkPipelineColorBlendStateCreateInfo cb{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
	cb.logicOpEnable = VK_FALSE;
	cb.attachmentCount = 1;
	cb.pAttachments = &cbAttach;

	const VkDynamicState dynStates[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
	VkPipelineDynamicStateCreateInfo dyn{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
	dyn.dynamicStateCount = static_cast<uint32_t>(std::size(dynStates));
	dyn.pDynamicStates = dynStates;

	const bool hasColorAttachment = desc.colorAttachmentFormat != Format::Undefined;
	VkFormat colorFormat = hasColorAttachment
						   ? vkutil::toVkFormat(desc.colorAttachmentFormat)
						   : VK_FORMAT_UNDEFINED;
	VkFormat depthFormat = VK_FORMAT_UNDEFINED;
	if (desc.depthTest || desc.depthWrite) {
		depthFormat = desc.depthAttachmentFormat != Format::Undefined
						  ? vkutil::toVkFormat(desc.depthAttachmentFormat)
						  : VK_FORMAT_D32_SFLOAT;
	}

	VkPipelineRenderingCreateInfo rendering{VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO};
	rendering.colorAttachmentCount = hasColorAttachment ? 1u : 0u;
	rendering.pColorAttachmentFormats = hasColorAttachment ? &colorFormat : nullptr;
	rendering.depthAttachmentFormat = depthFormat;
	rendering.stencilAttachmentFormat = VK_FORMAT_UNDEFINED;

	VkGraphicsPipelineCreateInfo pci{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
	pci.pNext = &rendering;
	pci.stageCount = 2;
	pci.pStages = stages;
	pci.pVertexInputState = &vi;
	pci.pInputAssemblyState = &ia;
	pci.pViewportState = &vp;
	pci.pRasterizationState = &rs;
	pci.pMultisampleState = &ms;
	pci.pDepthStencilState = &ds;
	pci.pColorBlendState = &cb;
	pci.pDynamicState = &dyn;
	pci.layout = layout;
	pci.renderPass = VK_NULL_HANDLE;
	pci.subpass = 0;

	VkPipeline pipeline = VK_NULL_HANDLE;
	const VkResult vr = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pci, nullptr, &pipeline);
	if (vr != VK_SUCCESS) {
		std::fprintf(stderr, "createGraphicsPipeline: vkCreateGraphicsPipelines failed (VkResult=%d)\n",
					 static_cast<int>(vr));
		vkDestroyPipelineLayout(device, layout, nullptr);
		return {};
	}

	VulkanResourcePool::PipelineEntry e{};
	e.pipeline = pipeline;
	e.layout = layout;

	PipelineHandle h{};
	h.id = pool->nextPipeId;
	if (pool->nextPipeId >= pool->pipelines.size())
		pool->pipelines.resize(pool->nextPipeId + 1);
	pool->pipelines[pool->nextPipeId++] = e;
	pool->psoCacheGraphics[descHash] = h.id;
	return h;
}

PipelineHandle vkCreateMeshShaderPipeline(VulkanDevice& dev, const MeshShaderPipelineDesc& desc)
{
	auto* pool = dev.resourcePool();
	const VkDevice device = dev.logical();
	if (!pool || device == VK_NULL_HANDLE) return {};

	const char* guardReason = nullptr;
	if (!vkmesh::canCreateMeshShaderPipeline(dev, desc, guardReason)) {
		std::fprintf(stderr, "createMeshShaderPipeline: %s\n",
					 guardReason ? guardReason : "mesh pipeline guard failed");
		return {};
	}
	if (desc.meshShader.id >= pool->shaders.size() ||
		desc.fragmentShader.id >= pool->shaders.size() ||
		(desc.taskShader.valid() && desc.taskShader.id >= pool->shaders.size())) {
		std::fprintf(stderr, "createMeshShaderPipeline: shader handle out of range\n");
		return {};
	}

	const uint64_t descHash = hashMeshShaderPipelineDesc(desc);
	auto it = pool->psoCacheMeshShader.find(descHash);
	if (it != pool->psoCacheMeshShader.end()) {
		PipelineHandle h{};
		h.id = it->second;
		return h;
	}

	const VkShaderModule task = desc.taskShader.valid() ? pool->shaders[desc.taskShader.id] : VK_NULL_HANDLE;
	const VkShaderModule mesh = pool->shaders[desc.meshShader.id];
	const VkShaderModule frag = pool->shaders[desc.fragmentShader.id];
	if ((desc.taskShader.valid() && task == VK_NULL_HANDLE) || mesh == VK_NULL_HANDLE || frag == VK_NULL_HANDLE) {
		std::fprintf(stderr, "createMeshShaderPipeline: shader module is null\n");
		return {};
	}

	VkPushConstantRange pcRange{};
	pcRange.stageFlags = VK_SHADER_STAGE_ALL;
	pcRange.offset = 0;
	pcRange.size = 128;

	VkPipelineLayoutCreateInfo lci{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
	lci.setLayoutCount = 0;
	lci.pushConstantRangeCount = 1;
	lci.pPushConstantRanges = &pcRange;

	VkPipelineLayout layout = VK_NULL_HANDLE;
	if (vkCreatePipelineLayout(device, &lci, nullptr, &layout) != VK_SUCCESS) {
		std::fprintf(stderr, "createMeshShaderPipeline: vkCreatePipelineLayout failed\n");
		return {};
	}

	std::array<VkPipelineShaderStageCreateInfo, 3> stages{};
	uint32_t stageCount = 0;
	if (desc.taskShader.valid()) {
		stages[stageCount].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		stages[stageCount].stage = VK_SHADER_STAGE_TASK_BIT_EXT;
		stages[stageCount].module = task;
		stages[stageCount].pName = "main";
		++stageCount;
	}
	stages[stageCount].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stages[stageCount].stage = VK_SHADER_STAGE_MESH_BIT_EXT;
	stages[stageCount].module = mesh;
	stages[stageCount].pName = "main";
	++stageCount;

	stages[stageCount].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stages[stageCount].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	stages[stageCount].module = frag;
	stages[stageCount].pName = "main";
	++stageCount;

	VkPipelineViewportStateCreateInfo vp{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
	vp.viewportCount = 1;
	vp.scissorCount = 1;

	VkPipelineRasterizationStateCreateInfo rs{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
	rs.depthClampEnable = VK_FALSE;
	rs.rasterizerDiscardEnable = VK_FALSE;
	rs.polygonMode = VK_POLYGON_MODE_FILL;
	rs.cullMode = VK_CULL_MODE_BACK_BIT;
	rs.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	rs.depthBiasEnable = VK_FALSE;
	rs.lineWidth = 1.f;

	VkPipelineMultisampleStateCreateInfo ms{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
	ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

	VkPipelineDepthStencilStateCreateInfo ds{VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
	ds.depthTestEnable = desc.depthTest ? VK_TRUE : VK_FALSE;
	ds.depthWriteEnable = desc.depthWrite ? VK_TRUE : VK_FALSE;
	ds.depthCompareOp = VK_COMPARE_OP_GREATER_OR_EQUAL;
	ds.depthBoundsTestEnable = VK_FALSE;
	ds.stencilTestEnable = VK_FALSE;

	VkPipelineColorBlendAttachmentState cbAttach{};
	cbAttach.blendEnable = VK_FALSE;
	cbAttach.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT |
							  VK_COLOR_COMPONENT_A_BIT;

	VkPipelineColorBlendStateCreateInfo cb{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
	cb.logicOpEnable = VK_FALSE;
	cb.attachmentCount = 1;
	cb.pAttachments = &cbAttach;

	const VkDynamicState dynStates[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
	VkPipelineDynamicStateCreateInfo dyn{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
	dyn.dynamicStateCount = static_cast<uint32_t>(std::size(dynStates));
	dyn.pDynamicStates = dynStates;

	VkFormat colorFormat = VK_FORMAT_B8G8R8A8_SRGB;
	VkFormat depthFormat = (desc.depthTest || desc.depthWrite) ? VK_FORMAT_D32_SFLOAT : VK_FORMAT_UNDEFINED;
	VkPipelineRenderingCreateInfo rendering{VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO};
	rendering.colorAttachmentCount = 1;
	rendering.pColorAttachmentFormats = &colorFormat;
	rendering.depthAttachmentFormat = depthFormat;
	rendering.stencilAttachmentFormat = VK_FORMAT_UNDEFINED;

	VkGraphicsPipelineCreateInfo pci{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
	pci.pNext = &rendering;
	pci.stageCount = stageCount;
	pci.pStages = stages.data();
	pci.pViewportState = &vp;
	pci.pRasterizationState = &rs;
	pci.pMultisampleState = &ms;
	pci.pDepthStencilState = &ds;
	pci.pColorBlendState = &cb;
	pci.pDynamicState = &dyn;
	pci.layout = layout;
	pci.renderPass = VK_NULL_HANDLE;
	pci.subpass = 0;

	VkPipeline pipeline = VK_NULL_HANDLE;
	const VkResult vr = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pci, nullptr, &pipeline);
	if (vr != VK_SUCCESS) {
		std::fprintf(stderr, "createMeshShaderPipeline: vkCreateGraphicsPipelines failed (VkResult=%d)\n",
					 static_cast<int>(vr));
		vkDestroyPipelineLayout(device, layout, nullptr);
		return {};
	}

	VulkanResourcePool::PipelineEntry e{};
	e.pipeline = pipeline;
	e.layout = layout;
	e.bindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;

	PipelineHandle h{};
	h.id = pool->nextPipeId;
	if (pool->nextPipeId >= pool->pipelines.size())
		pool->pipelines.resize(pool->nextPipeId + 1);
	pool->pipelines[pool->nextPipeId++] = e;
	pool->psoCacheMeshShader[descHash] = h.id;
	return h;
}

PipelineHandle vkCreateComputePipeline(VulkanDevice& dev, const ComputePipelineDesc& desc)
{
	auto* pool = dev.resourcePool();
	const VkDevice device = dev.logical();
	if (!pool || device == VK_NULL_HANDLE) return {};

	if (!desc.computeShader.valid() || desc.computeShader.id >= pool->shaders.size()) {
		std::fprintf(stderr, "createComputePipeline: invalid compute shader handle\n");
		return {};
	}

	const uint64_t descHash = hashComputePipelineDesc(desc);
	auto it = pool->psoCacheCompute.find(descHash);
	if (it != pool->psoCacheCompute.end()) {
		PipelineHandle h{};
		h.id = it->second;
		return h;
	}

	VkShaderModule cs = pool->shaders[desc.computeShader.id];
	if (!cs) {
		std::fprintf(stderr, "createComputePipeline: shader module is null\n");
		return {};
	}

	VkPushConstantRange pcRange{};
	pcRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	pcRange.offset = 0;
	pcRange.size = 128;

	VkPipelineLayoutCreateInfo lci{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
	lci.pushConstantRangeCount = 1;
	lci.pPushConstantRanges = &pcRange;
	VkPipelineLayout layout = VK_NULL_HANDLE;
	if (vkCreatePipelineLayout(device, &lci, nullptr, &layout) != VK_SUCCESS)
		return {};

	VkPipelineShaderStageCreateInfo stage{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
	stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	stage.module = cs;
	stage.pName = "main";

	VkComputePipelineCreateInfo ci{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
	ci.stage = stage;
	ci.layout = layout;

	VkPipeline pipeline = VK_NULL_HANDLE;
	if (vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &ci, nullptr, &pipeline) != VK_SUCCESS) {
		std::fprintf(stderr, "createComputePipeline: vkCreateComputePipelines failed\n");
		vkDestroyPipelineLayout(device, layout, nullptr);
		return {};
	}

	VulkanResourcePool::PipelineEntry e{};
	e.pipeline = pipeline;
	e.layout = layout;
	e.bindPoint = VK_PIPELINE_BIND_POINT_COMPUTE;
	PipelineHandle h{};
	h.id = pool->nextPipeId;
	if (pool->nextPipeId >= pool->pipelines.size())
		pool->pipelines.resize(pool->nextPipeId + 1);
	pool->pipelines[pool->nextPipeId++] = e;
	pool->psoCacheCompute[descHash] = h.id;
	return h;
}

PipelineHandle vkCreateRayTracingPipeline(VulkanDevice& dev, const RayTracingPipelineDesc& desc)
{
	auto* pool = dev.resourcePool();
	const VkDevice device = dev.logical();
	if (!pool || device == VK_NULL_HANDLE) return {};

	if (!dev.caps().rayTracingPipeline) {
		std::fprintf(stderr, "createRayTracingPipeline: ray tracing pipeline is not supported on this device\n");
		return {};
	}
	if (!desc.rayGenShader.valid() || desc.missShaders.empty() || desc.closestHitShaders.empty()) {
		std::fprintf(stderr, "createRayTracingPipeline: raygen, miss, and closest-hit shaders are required\n");
		return {};
	}

	const uint64_t descHash = hashRayTracingPipelineDesc(desc);
	auto it = pool->psoCacheRayTracing.find(descHash);
	if (it != pool->psoCacheRayTracing.end()) {
		PipelineHandle h{};
		h.id = it->second;
		return h;
	}

	auto shaderModuleFromHandle = [&](ShaderHandle h) -> VkShaderModule {
		if (!h.valid() || h.id >= pool->shaders.size()) return VK_NULL_HANDLE;
		return pool->shaders[h.id];
	};

	VkShaderModule raygen = shaderModuleFromHandle(desc.rayGenShader);
	if (raygen == VK_NULL_HANDLE) {
		std::fprintf(stderr, "createRayTracingPipeline: invalid raygen shader module\n");
		return {};
	}

	VkPushConstantRange pcRange{};
	pcRange.stageFlags = VK_SHADER_STAGE_ALL;
	pcRange.offset = 0;
	pcRange.size = 128;

	VkPipelineLayoutCreateInfo lci{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
	lci.pushConstantRangeCount = 1;
	lci.pPushConstantRanges = &pcRange;
	VkPipelineLayout layout = VK_NULL_HANDLE;
	if (vkCreatePipelineLayout(device, &lci, nullptr, &layout) != VK_SUCCESS) {
		std::fprintf(stderr, "createRayTracingPipeline: vkCreatePipelineLayout failed\n");
		return {};
	}

	std::vector<VkPipelineShaderStageCreateInfo> stages;
	stages.reserve(1 + desc.missShaders.size() + desc.closestHitShaders.size() + desc.anyHitShaders.size());
	std::vector<VkRayTracingShaderGroupCreateInfoKHR> groups;
	groups.reserve(1 + desc.missShaders.size() + desc.closestHitShaders.size());

	auto pushStage = [&](VkShaderStageFlagBits stageFlag, VkShaderModule module) -> uint32_t {
		VkPipelineShaderStageCreateInfo s{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
		s.stage = stageFlag;
		s.module = module;
		s.pName = "main";
		stages.push_back(s);
		return static_cast<uint32_t>(stages.size() - 1);
	};

	{
		const uint32_t rgIndex = pushStage(VK_SHADER_STAGE_RAYGEN_BIT_KHR, raygen);
		VkRayTracingShaderGroupCreateInfoKHR g{VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR};
		g.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
		g.generalShader = rgIndex;
		g.closestHitShader = VK_SHADER_UNUSED_KHR;
		g.anyHitShader = VK_SHADER_UNUSED_KHR;
		g.intersectionShader = VK_SHADER_UNUSED_KHR;
		groups.push_back(g);
	}

	for (const ShaderHandle h : desc.missShaders) {
		VkShaderModule miss = shaderModuleFromHandle(h);
		if (miss == VK_NULL_HANDLE) {
			std::fprintf(stderr, "createRayTracingPipeline: invalid miss shader module\n");
			vkDestroyPipelineLayout(device, layout, nullptr);
			return {};
		}
		const uint32_t missIndex = pushStage(VK_SHADER_STAGE_MISS_BIT_KHR, miss);
		VkRayTracingShaderGroupCreateInfoKHR g{VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR};
		g.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
		g.generalShader = missIndex;
		g.closestHitShader = VK_SHADER_UNUSED_KHR;
		g.anyHitShader = VK_SHADER_UNUSED_KHR;
		g.intersectionShader = VK_SHADER_UNUSED_KHR;
		groups.push_back(g);
	}

	for (size_t i = 0; i < desc.closestHitShaders.size(); ++i) {
		const ShaderHandle chHandle = desc.closestHitShaders[i];
		VkShaderModule closestHit = shaderModuleFromHandle(chHandle);
		if (closestHit == VK_NULL_HANDLE) {
			std::fprintf(stderr, "createRayTracingPipeline: invalid closest-hit shader module\n");
			vkDestroyPipelineLayout(device, layout, nullptr);
			return {};
		}

		const uint32_t chIndex = pushStage(VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, closestHit);
		uint32_t ahIndex = VK_SHADER_UNUSED_KHR;

		if (i < desc.anyHitShaders.size() && desc.anyHitShaders[i].valid()) {
			VkShaderModule anyHit = shaderModuleFromHandle(desc.anyHitShaders[i]);
			if (anyHit == VK_NULL_HANDLE) {
				std::fprintf(stderr, "createRayTracingPipeline: invalid any-hit shader module\n");
				vkDestroyPipelineLayout(device, layout, nullptr);
				return {};
			}
			ahIndex = pushStage(VK_SHADER_STAGE_ANY_HIT_BIT_KHR, anyHit);
		}

		VkRayTracingShaderGroupCreateInfoKHR g{VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR};
		g.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
		g.generalShader = VK_SHADER_UNUSED_KHR;
		g.closestHitShader = chIndex;
		g.anyHitShader = ahIndex;
		g.intersectionShader = VK_SHADER_UNUSED_KHR;
		groups.push_back(g);
	}

	auto pfnCreateRTPipeline =
		reinterpret_cast<PFN_vkCreateRayTracingPipelinesKHR>(vkGetDeviceProcAddr(device, "vkCreateRayTracingPipelinesKHR"));
	if (!pfnCreateRTPipeline) {
		std::fprintf(stderr, "createRayTracingPipeline: vkCreateRayTracingPipelinesKHR unavailable\n");
		vkDestroyPipelineLayout(device, layout, nullptr);
		return {};
	}

	VkRayTracingPipelineCreateInfoKHR ci{VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR};
	ci.stageCount = static_cast<uint32_t>(stages.size());
	ci.pStages = stages.data();
	ci.groupCount = static_cast<uint32_t>(groups.size());
	ci.pGroups = groups.data();
	ci.maxPipelineRayRecursionDepth = desc.maxRecursionDepth;
	ci.layout = layout;

	VkPipeline pipeline = VK_NULL_HANDLE;
	const VkResult vr = pfnCreateRTPipeline(device, VK_NULL_HANDLE, VK_NULL_HANDLE, 1, &ci, nullptr, &pipeline);
	if (vr != VK_SUCCESS) {
		std::fprintf(stderr, "createRayTracingPipeline: vkCreateRayTracingPipelinesKHR failed (VkResult=%d)\n",
					 static_cast<int>(vr));
		vkDestroyPipelineLayout(device, layout, nullptr);
		return {};
	}

	VulkanResourcePool::PipelineEntry e{};
	e.pipeline = pipeline;
	e.layout = layout;
	e.bindPoint = VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR;

	PipelineHandle h{};
	h.id = pool->nextPipeId;
	if (pool->nextPipeId >= pool->pipelines.size())
		pool->pipelines.resize(pool->nextPipeId + 1);
	pool->pipelines[pool->nextPipeId++] = e;
	pool->psoCacheRayTracing[descHash] = h.id;
	return h;
}

void vkDestroyPipeline(VulkanDevice& dev, PipelineHandle h)
{
	auto* pool = dev.resourcePool();
	const VkDevice device = dev.logical();
	if (!pool || device == VK_NULL_HANDLE) return;
	if (!h.valid() || h.id >= pool->pipelines.size()) return;

	auto& entry = pool->pipelines[h.id];
	if (entry.pipeline)
		vkDestroyPipeline(device, entry.pipeline, nullptr);
	if (entry.layout)
		vkDestroyPipelineLayout(device, entry.layout, nullptr);
	entry = {};

	auto eraseByPipelineId = [pipelineId = h.id](auto& cache) {
		for (auto it = cache.begin(); it != cache.end();) {
			if (it->second == pipelineId)
				it = cache.erase(it);
			else
				++it;
		}
	};

	eraseByPipelineId(pool->psoCacheGraphics);
	eraseByPipelineId(pool->psoCacheMeshShader);
	eraseByPipelineId(pool->psoCacheCompute);
	eraseByPipelineId(pool->psoCacheRayTracing);
}

} // namespace nexus::gfx
