// ─────────────────────────────────────────────────────────────────────────────
//  VulkanShader — GLSL/SPIR-V compiler and module creation helpers
// ─────────────────────────────────────────────────────────────────────────────
#include <nexus/gfx/vulkan/ShaderCompiler.h>

#include <SPIRV/GlslangToSpv.h>
#include <glslang/Public/ResourceLimits.h>
#include <glslang/Public/ShaderLang.h>

#include <array>
#include <cctype>
#include <fstream>
#include <mutex>
#include <sstream>

namespace nexus::gfx::vkshader {

namespace {

EShLanguage toGlslangStage(ShaderStage stage)
{
	switch (stage) {
	case ShaderStage::Vertex:     return EShLangVertex;
	case ShaderStage::Fragment:   return EShLangFragment;
	case ShaderStage::Compute:    return EShLangCompute;
	case ShaderStage::Task:       return EShLangTask;
	case ShaderStage::Mesh:       return EShLangMesh;
	case ShaderStage::RayGen:     return EShLangRayGen;
	case ShaderStage::ClosestHit: return EShLangClosestHit;
	case ShaderStage::AnyHit:     return EShLangAnyHit;
	case ShaderStage::Miss:       return EShLangMiss;
	case ShaderStage::Intersect:  return EShLangIntersect;
	default:                      return EShLangVertex;
	}
}

bool isSingleStage(ShaderStage stage)
{
	const auto v = static_cast<uint32_t>(stage);
	return v != 0u && (v & (v - 1u)) == 0u;
}

std::string toLower(std::string_view in)
{
	std::string out(in.begin(), in.end());
	for (char& c : out)
		c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
	return out;
}

std::string fileExtLower(std::string_view path)
{
	const auto pos = path.find_last_of('.');
	if (pos == std::string_view::npos)
		return {};
	return toLower(path.substr(pos));
}

bool inferStageFromPath(std::string_view path, ShaderStage& stageOut)
{
	const std::string ext = fileExtLower(path);
	if (ext == ".vert") { stageOut = ShaderStage::Vertex; return true; }
	if (ext == ".frag") { stageOut = ShaderStage::Fragment; return true; }
	if (ext == ".comp") { stageOut = ShaderStage::Compute; return true; }
	if (ext == ".mesh") { stageOut = ShaderStage::Mesh; return true; }
	if (ext == ".task") { stageOut = ShaderStage::Task; return true; }
	if (ext == ".rgen") { stageOut = ShaderStage::RayGen; return true; }
	if (ext == ".rchit") { stageOut = ShaderStage::ClosestHit; return true; }
	if (ext == ".rahit") { stageOut = ShaderStage::AnyHit; return true; }
	if (ext == ".rmiss") { stageOut = ShaderStage::Miss; return true; }
	if (ext == ".rint") { stageOut = ShaderStage::Intersect; return true; }
	return false;
}

void ensureGlslangInit()
{
	static std::once_flag initOnce;
	std::call_once(initOnce, []() {
		glslang::InitializeProcess();
	});
}

bool readTextFile(std::string_view path, std::string& out, std::string& err)
{
	std::ifstream in(std::string(path), std::ios::in);
	if (!in) {
		err = "Failed to open text shader file: " + std::string(path);
		return false;
	}
	std::ostringstream ss;
	ss << in.rdbuf();
	out = ss.str();
	if (out.empty()) {
		err = "Shader file is empty: " + std::string(path);
		return false;
	}
	return true;
}

bool readSpvFile(std::string_view path, std::vector<uint32_t>& out, std::string& err)
{
	std::ifstream in(std::string(path), std::ios::binary | std::ios::ate);
	if (!in) {
		err = "Failed to open SPIR-V file: " + std::string(path);
		return false;
	}
	const auto bytes = in.tellg();
	if (bytes <= 0) {
		err = "SPIR-V file is empty: " + std::string(path);
		return false;
	}
	if ((bytes % 4) != 0) {
		err = "SPIR-V byte size must be multiple of 4: " + std::string(path);
		return false;
	}
	out.resize(static_cast<size_t>(bytes) / 4u);
	in.seekg(0, std::ios::beg);
	in.read(reinterpret_cast<char*>(out.data()), bytes);
	if (!in) {
		err = "Failed to read SPIR-V file: " + std::string(path);
		out.clear();
		return false;
	}
	return true;
}

} // namespace

bool compileGlslToSpirv(
	std::string_view glslSource,
	ShaderStage stage,
	ShaderCompileOutput& out,
	std::string_view entryPoint,
	std::string_view debugName)
{
	out.spirv.clear();
	out.errorLog.clear();

	if (glslSource.empty()) {
		out.errorLog = "GLSL source is empty";
		return false;
	}
	if (!isSingleStage(stage)) {
		out.errorLog = "ShaderStage for GLSL compile must be a single stage bit";
		return false;
	}

	ensureGlslangInit();
	const auto lang = toGlslangStage(stage);

	glslang::TShader shader(lang);
	const char* src = glslSource.data();
	const int len   = static_cast<int>(glslSource.size());
	shader.setStringsWithLengths(&src, &len, 1);

	std::string entry(entryPoint.empty() ? "main" : std::string(entryPoint));
	shader.setEntryPoint(entry.c_str());
	shader.setSourceEntryPoint(entry.c_str());

	std::string name(debugName.empty() ? "shader" : std::string(debugName));
	constexpr const char* preamble = "#line 1\n";
	shader.setPreamble(preamble);

	shader.setEnvInput(glslang::EShSourceGlsl, lang, glslang::EShClientVulkan, 460);
	shader.setEnvClient(glslang::EShClientVulkan, glslang::EShTargetVulkan_1_3);
	shader.setEnvTarget(glslang::EShTargetSpv, glslang::EShTargetSpv_1_6);

	const EShMessages messages = static_cast<EShMessages>(EShMsgSpvRules | EShMsgVulkanRules);
	if (!shader.parse(GetDefaultResources(), 460, false, messages)) {
		out.errorLog = "GLSL parse failed for '" + name + "':\n";
		out.errorLog += shader.getInfoLog();
		const char* dbg = shader.getInfoDebugLog();
		if (dbg && dbg[0] != '\0') {
			out.errorLog += "\n";
			out.errorLog += dbg;
		}
		return false;
	}

	glslang::TProgram program;
	program.addShader(&shader);
	if (!program.link(messages)) {
		out.errorLog = "GLSL link failed for '" + name + "':\n";
		out.errorLog += program.getInfoLog();
		const char* dbg = program.getInfoDebugLog();
		if (dbg && dbg[0] != '\0') {
			out.errorLog += "\n";
			out.errorLog += dbg;
		}
		return false;
	}

	auto* intermediate = program.getIntermediate(lang);
	if (!intermediate) {
		out.errorLog = "GLSL compilation produced no intermediate for '" + name + "'";
		return false;
	}

	glslang::SpvOptions spvOptions{};
	spv::SpvBuildLogger logger;
	glslang::GlslangToSpv(*intermediate, out.spirv, &logger, &spvOptions);
	if (out.spirv.empty()) {
		out.errorLog = "SPIR-V generation failed for '" + name + "'";
		const std::string msgs = logger.getAllMessages();
		if (!msgs.empty()) {
			out.errorLog += "\n";
			out.errorLog += msgs;
		}
		return false;
	}

	return true;
}

bool loadShaderFileToSpirv(
	std::string_view path,
	ShaderStage glslStage,
	ShaderCompileOutput& out,
	std::string_view entryPoint)
{
	out.spirv.clear();
	out.errorLog.clear();
	if (path.empty()) {
		out.errorLog = "Shader file path is empty";
		return false;
	}

	const std::string ext = fileExtLower(path);
	if (ext == ".spv") {
		return readSpvFile(path, out.spirv, out.errorLog);
	}

	if (ext == ".glsl" || ext == ".vert" || ext == ".frag" || ext == ".comp" ||
		ext == ".mesh" || ext == ".task" || ext == ".rgen" || ext == ".rchit" ||
		ext == ".rahit" || ext == ".rmiss" || ext == ".rint") {
		ShaderStage stage = glslStage;
		if (!isSingleStage(stage) && !inferStageFromPath(path, stage)) {
			out.errorLog = "Cannot infer GLSL shader stage from file extension for: " + std::string(path);
			return false;
		}

		std::string source;
		if (!readTextFile(path, source, out.errorLog))
			return false;

		return compileGlslToSpirv(source, stage, out, entryPoint, path);
	}

	out.errorLog = "Unsupported shader extension: " + ext;
	return false;
}

VkShaderModule vkCompileShader(
	VkDevice device,
	const char* glslSource,
	ShaderStage stage,
	const char* entryPoint,
	std::string* errorOut)
{
	ShaderCompileOutput out;
	if (!compileGlslToSpirv(glslSource ? std::string_view(glslSource) : std::string_view(),
							stage, out, entryPoint ? entryPoint : "main", "inline-glsl")) {
		if (errorOut) *errorOut = out.errorLog;
		return VK_NULL_HANDLE;
	}

	VkShaderModuleCreateInfo ci{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
	ci.codeSize = out.spirv.size() * sizeof(uint32_t);
	ci.pCode    = out.spirv.data();

	VkShaderModule mod = VK_NULL_HANDLE;
	const VkResult vr = vkCreateShaderModule(device, &ci, nullptr, &mod);
	if (vr != VK_SUCCESS) {
		if (errorOut) {
			*errorOut = "vkCreateShaderModule failed with VkResult=" + std::to_string(static_cast<int>(vr));
		}
		return VK_NULL_HANDLE;
	}
	if (errorOut) errorOut->clear();
	return mod;
}

VkShaderModule vkCreateShaderModuleFromFile(
	VkDevice device,
	std::string_view path,
	ShaderStage glslStage,
	std::string_view entryPoint,
	std::string* errorOut)
{
	ShaderCompileOutput out;
	if (!loadShaderFileToSpirv(path, glslStage, out, entryPoint)) {
		if (errorOut) *errorOut = out.errorLog;
		return VK_NULL_HANDLE;
	}

	VkShaderModuleCreateInfo ci{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
	ci.codeSize = out.spirv.size() * sizeof(uint32_t);
	ci.pCode    = out.spirv.data();

	VkShaderModule mod = VK_NULL_HANDLE;
	const VkResult vr = vkCreateShaderModule(device, &ci, nullptr, &mod);
	if (vr != VK_SUCCESS) {
		if (errorOut) {
			*errorOut = "vkCreateShaderModule failed for file '" + std::string(path)
					  + "' with VkResult=" + std::to_string(static_cast<int>(vr));
		}
		return VK_NULL_HANDLE;
	}
	if (errorOut) errorOut->clear();
	return mod;
}

} // namespace nexus::gfx::vkshader
