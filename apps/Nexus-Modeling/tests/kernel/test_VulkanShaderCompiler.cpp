// ─────────────────────────────────────────────────────────────────────────────
//  Tests: Vulkan shader compiler helpers
// ─────────────────────────────────────────────────────────────────────────────
#include <nexus/gfx/vulkan/ShaderCompiler.h>

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>

using namespace nexus::gfx;

TEST(VulkanShaderCompiler, CompileVertexAndFragmentGlsl)
{
    constexpr const char* kVert = R"GLSL(
#version 460
layout(location = 0) in vec3 inPos;
void main() {
    gl_Position = vec4(inPos, 1.0);
}
)GLSL";

    constexpr const char* kFrag = R"GLSL(
#version 460
layout(location = 0) out vec4 outColor;
void main() {
    outColor = vec4(1.0, 0.5, 0.2, 1.0);
}
)GLSL";

    vkshader::ShaderCompileOutput vertOut;
    vkshader::ShaderCompileOutput fragOut;

    EXPECT_TRUE(vkshader::compileGlslToSpirv(kVert, ShaderStage::Vertex, vertOut, "main", "test.vert"))
        << vertOut.errorLog;
    EXPECT_TRUE(vkshader::compileGlslToSpirv(kFrag, ShaderStage::Fragment, fragOut, "main", "test.frag"))
        << fragOut.errorLog;

    EXPECT_FALSE(vertOut.spirv.empty());
    EXPECT_FALSE(fragOut.spirv.empty());
}

TEST(VulkanShaderCompiler, LoadSpvFile)
{
    constexpr const char* kComp = R"GLSL(
#version 460
layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;
void main() {}
)GLSL";

    vkshader::ShaderCompileOutput compiled;
    ASSERT_TRUE(vkshader::compileGlslToSpirv(kComp, ShaderStage::Compute, compiled, "main", "tmp.comp"))
        << compiled.errorLog;
    ASSERT_FALSE(compiled.spirv.empty());

    const std::filesystem::path tmpPath =
        std::filesystem::temp_directory_path() / "nexus_shader_compiler_test.spv";

    {
        std::ofstream out(tmpPath, std::ios::binary | std::ios::trunc);
        ASSERT_TRUE(out.good());
        out.write(reinterpret_cast<const char*>(compiled.spirv.data()),
                  static_cast<std::streamsize>(compiled.spirv.size() * sizeof(uint32_t)));
        ASSERT_TRUE(out.good());
    }

    vkshader::ShaderCompileOutput loaded;
    EXPECT_TRUE(vkshader::loadShaderFileToSpirv(tmpPath.string(), ShaderStage::Compute, loaded, "main"))
        << loaded.errorLog;
    EXPECT_EQ(loaded.spirv, compiled.spirv);

    std::error_code ec;
    std::filesystem::remove(tmpPath, ec);
}
