#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  Vulkan Shader Compiler helpers
//  - Compile GLSL source to SPIR-V (glslang)
//  - Load .spv/.glsl files and create VkShaderModule
// ─────────────────────────────────────────────────────────────────────────────
#include <nexus/gfx/Types.h>
#include <vulkan/vulkan.h>

#include <string>
#include <string_view>
#include <vector>

namespace nexus::gfx::vkshader {

struct ShaderCompileOutput {
    std::vector<uint32_t> spirv;
    std::string           errorLog;
};

[[nodiscard]] bool compileGlslToSpirv(
    std::string_view glslSource,
    ShaderStage stage,
    ShaderCompileOutput& out,
    std::string_view entryPoint = "main",
    std::string_view debugName  = {}
);

[[nodiscard]] bool loadShaderFileToSpirv(
    std::string_view path,
    ShaderStage glslStage,
    ShaderCompileOutput& out,
    std::string_view entryPoint = "main"
);

// Convenience function requested by runtime paths: compiles GLSL and creates module.
[[nodiscard]] VkShaderModule vkCompileShader(
    VkDevice device,
    const char* glslSource,
    ShaderStage stage,
    const char* entryPoint,
    std::string* errorOut = nullptr
);

[[nodiscard]] VkShaderModule vkCreateShaderModuleFromFile(
    VkDevice device,
    std::string_view path,
    ShaderStage glslStage,
    std::string_view entryPoint,
    std::string* errorOut = nullptr
);

} // namespace nexus::gfx::vkshader
