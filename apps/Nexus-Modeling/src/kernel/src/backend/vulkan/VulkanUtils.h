#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  VulkanUtils — shared conversion helpers used across all Vulkan source files
// ─────────────────────────────────────────────────────────────────────────────
#include <nexus/gfx/Types.h>
#include <nexus/gfx/Swapchain.h>
#include <vulkan/vulkan.h>

namespace nexus::gfx::vkutil {

// ── Format ───────────────────────────────────────────────────────────────────
inline VkFormat toVkFormat(Format f) noexcept
{
    switch (f) {
    case Format::R8_Unorm:              return VK_FORMAT_R8_UNORM;
    case Format::R8_Uint:               return VK_FORMAT_R8_UINT;
    case Format::R8_Sint:               return VK_FORMAT_R8_SINT;
    case Format::R8G8_Unorm:            return VK_FORMAT_R8G8_UNORM;
    case Format::R8G8B8A8_Unorm:        return VK_FORMAT_R8G8B8A8_UNORM;
    case Format::R8G8B8A8_Srgb:         return VK_FORMAT_R8G8B8A8_SRGB;
    case Format::B8G8R8A8_Unorm:        return VK_FORMAT_B8G8R8A8_UNORM;
    case Format::B8G8R8A8_Srgb:         return VK_FORMAT_B8G8R8A8_SRGB;
    case Format::R16_Float:             return VK_FORMAT_R16_SFLOAT;
    case Format::R16G16_Float:          return VK_FORMAT_R16G16_SFLOAT;
    case Format::R16G16B16A16_Float:    return VK_FORMAT_R16G16B16A16_SFLOAT;
    case Format::R32_Float:             return VK_FORMAT_R32_SFLOAT;
    case Format::R32G32_Float:          return VK_FORMAT_R32G32_SFLOAT;
    case Format::R32G32B32_Float:       return VK_FORMAT_R32G32B32_SFLOAT;
    case Format::R32G32B32A32_Float:    return VK_FORMAT_R32G32B32A32_SFLOAT;
    case Format::A2R10G10B10_Unorm:     return VK_FORMAT_A2R10G10B10_UNORM_PACK32;
    case Format::D16_Unorm:             return VK_FORMAT_D16_UNORM;
    case Format::D32_Float:             return VK_FORMAT_D32_SFLOAT;
    case Format::D24_Unorm_S8_Uint:     return VK_FORMAT_D24_UNORM_S8_UINT;
    case Format::D32_Float_S8_Uint:     return VK_FORMAT_D32_SFLOAT_S8_UINT;
    case Format::BC1_Unorm:             return VK_FORMAT_BC1_RGB_UNORM_BLOCK;
    case Format::BC3_Unorm:             return VK_FORMAT_BC3_UNORM_BLOCK;
    case Format::BC5_Unorm:             return VK_FORMAT_BC5_UNORM_BLOCK;
    case Format::BC7_Unorm:             return VK_FORMAT_BC7_UNORM_BLOCK;
    case Format::A2R10G10B10_Unorm_Pack32:  return VK_FORMAT_A2R10G10B10_UNORM_PACK32;
    case Format::R16G16B16A16_Sfloat:       return VK_FORMAT_R16G16B16A16_SFLOAT;
    default:                            return VK_FORMAT_UNDEFINED;
    }
}

inline Format fromVkFormat(VkFormat f) noexcept
{
    switch (f) {
    case VK_FORMAT_R8G8B8A8_UNORM:         return Format::R8G8B8A8_Unorm;
    case VK_FORMAT_R8G8B8A8_SRGB:          return Format::R8G8B8A8_Srgb;
    case VK_FORMAT_B8G8R8A8_UNORM:         return Format::B8G8R8A8_Unorm;
    case VK_FORMAT_B8G8R8A8_SRGB:          return Format::B8G8R8A8_Srgb;
    case VK_FORMAT_R16G16B16A16_SFLOAT:    return Format::R16G16B16A16_Float;
    case VK_FORMAT_R32_SFLOAT:             return Format::R32_Float;
    case VK_FORMAT_D32_SFLOAT:             return Format::D32_Float;
    case VK_FORMAT_D24_UNORM_S8_UINT:      return Format::D24_Unorm_S8_Uint;
    case VK_FORMAT_D32_SFLOAT_S8_UINT:     return Format::D32_Float_S8_Uint;
    default:                               return Format::Undefined;
    }
}

// ── Buffer usage ──────────────────────────────────────────────────────────────
inline VkBufferUsageFlags toVkBufferUsage(BufferUsage u) noexcept
{
    VkBufferUsageFlags flags = 0;
    if (hasFlag(u, BufferUsage::VertexBuffer))  flags |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    if (hasFlag(u, BufferUsage::IndexBuffer))   flags |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    if (hasFlag(u, BufferUsage::UniformBuffer)) flags |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    if (hasFlag(u, BufferUsage::StorageBuffer)) flags |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    if (hasFlag(u, BufferUsage::IndirectArgs))  flags |= VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
    if (hasFlag(u, BufferUsage::TransferSrc))   flags |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    if (hasFlag(u, BufferUsage::TransferDst))   flags |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    if (hasFlag(u, BufferUsage::RayTracingAS))  flags |= VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR
                                                       | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    if (hasFlag(u, BufferUsage::MeshletBuffer)) flags |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    // Always enable device address for bindless / ray tracing convenience
    flags |= VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    return flags;
}

// ── Texture usage ─────────────────────────────────────────────────────────────
inline VkImageUsageFlags toVkImageUsage(TextureUsage u) noexcept
{
    VkImageUsageFlags flags = 0;
    if (static_cast<uint32_t>(u) & static_cast<uint32_t>(TextureUsage::Sampled))          flags |= VK_IMAGE_USAGE_SAMPLED_BIT;
    if (static_cast<uint32_t>(u) & static_cast<uint32_t>(TextureUsage::Storage))          flags |= VK_IMAGE_USAGE_STORAGE_BIT;
    if (static_cast<uint32_t>(u) & static_cast<uint32_t>(TextureUsage::ColorAttachment))  flags |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    if (static_cast<uint32_t>(u) & static_cast<uint32_t>(TextureUsage::DepthAttachment))  flags |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    if (static_cast<uint32_t>(u) & static_cast<uint32_t>(TextureUsage::TransferSrc))      flags |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    if (static_cast<uint32_t>(u) & static_cast<uint32_t>(TextureUsage::TransferDst))      flags |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    return flags;
}

// ── Sample count ──────────────────────────────────────────────────────────────
inline VkSampleCountFlagBits toVkSampleCount(SampleCount s) noexcept
{
    switch (s) {
    case SampleCount::X2:  return VK_SAMPLE_COUNT_2_BIT;
    case SampleCount::X4:  return VK_SAMPLE_COUNT_4_BIT;
    case SampleCount::X8:  return VK_SAMPLE_COUNT_8_BIT;
    case SampleCount::X16: return VK_SAMPLE_COUNT_16_BIT;
    default:               return VK_SAMPLE_COUNT_1_BIT;
    }
}

// ── Depth format helper ───────────────────────────────────────────────────────
inline bool isDepthFormat(Format f) noexcept
{
    return f == Format::D16_Unorm
        || f == Format::D32_Float
        || f == Format::D24_Unorm_S8_Uint
        || f == Format::D32_Float_S8_Uint;
}

inline bool hasStencil(Format f) noexcept
{
    return f == Format::D24_Unorm_S8_Uint || f == Format::D32_Float_S8_Uint;
}

// ── Present mode ──────────────────────────────────────────────────────────────
inline VkPresentModeKHR toVkPresentMode(PresentMode m) noexcept
{
    switch (m) {
    case PresentMode::Immediate:    return VK_PRESENT_MODE_IMMEDIATE_KHR;
    case PresentMode::FifoRelaxed:  return VK_PRESENT_MODE_FIFO_RELAXED_KHR;
    case PresentMode::Mailbox:      return VK_PRESENT_MODE_MAILBOX_KHR;
    default:                        return VK_PRESENT_MODE_FIFO_KHR;
    }
}

// ── Shader stage ──────────────────────────────────────────────────────────────
inline VkShaderStageFlags toVkShaderStage(ShaderStage s) noexcept
{
    VkShaderStageFlags flags = 0;
    if (static_cast<uint32_t>(s) & static_cast<uint32_t>(ShaderStage::Vertex))     flags |= VK_SHADER_STAGE_VERTEX_BIT;
    if (static_cast<uint32_t>(s) & static_cast<uint32_t>(ShaderStage::Fragment))   flags |= VK_SHADER_STAGE_FRAGMENT_BIT;
    if (static_cast<uint32_t>(s) & static_cast<uint32_t>(ShaderStage::Compute))    flags |= VK_SHADER_STAGE_COMPUTE_BIT;
    if (static_cast<uint32_t>(s) & static_cast<uint32_t>(ShaderStage::Task))       flags |= VK_SHADER_STAGE_TASK_BIT_EXT;
    if (static_cast<uint32_t>(s) & static_cast<uint32_t>(ShaderStage::Mesh))       flags |= VK_SHADER_STAGE_MESH_BIT_EXT;
    if (static_cast<uint32_t>(s) & static_cast<uint32_t>(ShaderStage::RayGen))     flags |= VK_SHADER_STAGE_RAYGEN_BIT_KHR;
    if (static_cast<uint32_t>(s) & static_cast<uint32_t>(ShaderStage::ClosestHit)) flags |= VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
    if (static_cast<uint32_t>(s) & static_cast<uint32_t>(ShaderStage::AnyHit))     flags |= VK_SHADER_STAGE_ANY_HIT_BIT_KHR;
    if (static_cast<uint32_t>(s) & static_cast<uint32_t>(ShaderStage::Miss))       flags |= VK_SHADER_STAGE_MISS_BIT_KHR;
    if (static_cast<uint32_t>(s) & static_cast<uint32_t>(ShaderStage::Intersect))  flags |= VK_SHADER_STAGE_INTERSECTION_BIT_KHR;
    return flags;
}

// ── Rasterizer state ──────────────────────────────────────────────────────────────────
inline VkCullModeFlags toVkCullMode(CullMode c) noexcept {
    switch (c) {
    case CullMode::None:  return VK_CULL_MODE_NONE;
    case CullMode::Front: return VK_CULL_MODE_FRONT_BIT;
    default:              return VK_CULL_MODE_BACK_BIT;
    }
}
inline VkFrontFace toVkFrontFace(FrontFace f) noexcept {
    return f == FrontFace::CW ? VK_FRONT_FACE_CLOCKWISE : VK_FRONT_FACE_COUNTER_CLOCKWISE;
}
inline VkPolygonMode toVkPolygonMode(PolygonMode p) noexcept {
    switch (p) {
    case PolygonMode::Line:  return VK_POLYGON_MODE_LINE;
    case PolygonMode::Point: return VK_POLYGON_MODE_POINT;
    default:                 return VK_POLYGON_MODE_FILL;
    }
}

// ── Depth / stencil ──────────────────────────────────────────────────────────────────────
inline VkCompareOp toVkCompareOp(CompareOp op) noexcept {
    switch (op) {
    case CompareOp::Never:          return VK_COMPARE_OP_NEVER;
    case CompareOp::Less:           return VK_COMPARE_OP_LESS;
    case CompareOp::Equal:          return VK_COMPARE_OP_EQUAL;
    case CompareOp::LessOrEqual:    return VK_COMPARE_OP_LESS_OR_EQUAL;
    case CompareOp::Greater:        return VK_COMPARE_OP_GREATER;
    case CompareOp::NotEqual:       return VK_COMPARE_OP_NOT_EQUAL;
    case CompareOp::GreaterOrEqual: return VK_COMPARE_OP_GREATER_OR_EQUAL;
    default:                        return VK_COMPARE_OP_ALWAYS;
    }
}

// ── Blend ──────────────────────────────────────────────────────────────────────────────────
inline VkBlendFactor toVkBlendFactor(BlendFactor f) noexcept {
    switch (f) {
    case BlendFactor::Zero:                  return VK_BLEND_FACTOR_ZERO;
    case BlendFactor::One:                   return VK_BLEND_FACTOR_ONE;
    case BlendFactor::SrcColor:              return VK_BLEND_FACTOR_SRC_COLOR;
    case BlendFactor::OneMinusSrcColor:      return VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
    case BlendFactor::DstColor:              return VK_BLEND_FACTOR_DST_COLOR;
    case BlendFactor::OneMinusDstColor:      return VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
    case BlendFactor::SrcAlpha:              return VK_BLEND_FACTOR_SRC_ALPHA;
    case BlendFactor::OneMinusSrcAlpha:      return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    case BlendFactor::DstAlpha:              return VK_BLEND_FACTOR_DST_ALPHA;
    case BlendFactor::OneMinusDstAlpha:      return VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
    case BlendFactor::ConstantColor:         return VK_BLEND_FACTOR_CONSTANT_COLOR;
    case BlendFactor::OneMinusConstantColor: return VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR;
    default:                                 return VK_BLEND_FACTOR_SRC_ALPHA_SATURATE;
    }
}
inline VkBlendOp toVkBlendOp(BlendOp op) noexcept {
    switch (op) {
    case BlendOp::Subtract:        return VK_BLEND_OP_SUBTRACT;
    case BlendOp::ReverseSubtract: return VK_BLEND_OP_REVERSE_SUBTRACT;
    case BlendOp::Min:             return VK_BLEND_OP_MIN;
    case BlendOp::Max:             return VK_BLEND_OP_MAX;
    default:                       return VK_BLEND_OP_ADD;
    }
}

// ── Sampler ───────────────────────────────────────────────────────────────────────────────────
inline VkFilter toVkFilter(SamplerFilter f) noexcept {
    return f == SamplerFilter::Linear ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;
}
inline VkSamplerAddressMode toVkAddressMode(SamplerAddressMode m) noexcept {
    switch (m) {
    case SamplerAddressMode::MirroredRepeat:    return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
    case SamplerAddressMode::ClampToEdge:       return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    case SamplerAddressMode::ClampToBorder:     return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    case SamplerAddressMode::MirrorClampToEdge: return VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE;
    default:                                    return VK_SAMPLER_ADDRESS_MODE_REPEAT;
    }
}
inline VkSamplerMipmapMode toVkMipmapMode(SamplerMipmapMode m) noexcept {
    return m == SamplerMipmapMode::Linear ? VK_SAMPLER_MIPMAP_MODE_LINEAR : VK_SAMPLER_MIPMAP_MODE_NEAREST;
}

// ── Texture layout ───────────────────────────────────────────────────────────────────────────
inline VkImageLayout toVkImageLayout(TextureLayout l) noexcept {
    switch (l) {
    case TextureLayout::General:         return VK_IMAGE_LAYOUT_GENERAL;
    case TextureLayout::ColorAttachment: return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    case TextureLayout::DepthWrite:      return VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    case TextureLayout::DepthRead:       return VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
    case TextureLayout::ShaderRead:      return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    case TextureLayout::ShaderReadWrite: return VK_IMAGE_LAYOUT_GENERAL;
    case TextureLayout::TransferSrc:     return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    case TextureLayout::TransferDst:     return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    case TextureLayout::Present:         return VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    default:                             return VK_IMAGE_LAYOUT_UNDEFINED;
    }
}

// Maps BufferAccess to (srcStageMask, srcAccessMask) pair for sync2
struct BarrierStageMask { VkPipelineStageFlags2 stage; VkAccessFlags2 access; };
inline BarrierStageMask toVkBufferBarrierMask(BufferAccess a) noexcept {
    VkPipelineStageFlags2 stage  = VK_PIPELINE_STAGE_2_NONE;
    VkAccessFlags2        access = VK_ACCESS_2_NONE;
    if (static_cast<uint32_t>(a) & static_cast<uint32_t>(BufferAccess::IndirectCommand))
        { stage |= VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT;  access |= VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT; }
    if (static_cast<uint32_t>(a) & static_cast<uint32_t>(BufferAccess::IndexRead))
        { stage |= VK_PIPELINE_STAGE_2_INDEX_INPUT_BIT;    access |= VK_ACCESS_2_INDEX_READ_BIT; }
    if (static_cast<uint32_t>(a) & static_cast<uint32_t>(BufferAccess::VertexRead))
        { stage |= VK_PIPELINE_STAGE_2_VERTEX_ATTRIBUTE_INPUT_BIT; access |= VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT; }
    if (static_cast<uint32_t>(a) & static_cast<uint32_t>(BufferAccess::UniformRead))
        { stage |= VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
          access |= VK_ACCESS_2_UNIFORM_READ_BIT; }
    if (static_cast<uint32_t>(a) & static_cast<uint32_t>(BufferAccess::ShaderRead))
        { stage |= VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
          access |= VK_ACCESS_2_SHADER_READ_BIT; }
    if (static_cast<uint32_t>(a) & static_cast<uint32_t>(BufferAccess::ShaderWrite))
        { stage |= VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
          access |= VK_ACCESS_2_SHADER_WRITE_BIT; }
    if (static_cast<uint32_t>(a) & static_cast<uint32_t>(BufferAccess::TransferRead))
        { stage |= VK_PIPELINE_STAGE_2_COPY_BIT;  access |= VK_ACCESS_2_TRANSFER_READ_BIT; }
    if (static_cast<uint32_t>(a) & static_cast<uint32_t>(BufferAccess::TransferWrite))
        { stage |= VK_PIPELINE_STAGE_2_COPY_BIT;  access |= VK_ACCESS_2_TRANSFER_WRITE_BIT; }
    return {stage, access};
}

} // namespace nexus::gfx::vkutil
