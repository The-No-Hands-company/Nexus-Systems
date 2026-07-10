#pragma once
#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include <nexus/automation/AutomationScript.h>

namespace nexus {

namespace geometry { class Mesh; }
namespace asset { class SceneAsset; }
namespace gfx { struct GaussianSplatCloud; }
namespace parametric { class ConstraintGraph; }

} // namespace nexus

namespace nexus::automation {

bool isFiniteFloat(float v) noexcept;
bool isFiniteDouble(double v) noexcept;

// Stable scalar formatting for automation messages. Produces the shortest
// round-trippable form but always keeps a decimal point (e.g. 4 -> "4.0",
// 4.5 -> "4.5") so message output does not drift with the standard library's
// std::to_string behaviour (which in C++26 drops trailing zeros).
std::string formatScalar(double v);
std::string formatScalar(float v);
nexus::render::Mat4 makeTranslationMatrix(float tx, float ty, float tz) noexcept;
nexus::render::Mat4 makeScaleMatrix(float sx, float sy, float sz) noexcept;

std::string trimCopy(std::string_view s);
std::vector<std::string> tokenizeLine(std::string_view line);
bool parseKeyValueToken(const std::string& token, std::string& key, std::string& value);
std::optional<std::string> getArg(const ScriptCommand& command, std::string_view key);
std::optional<float> parseFloatArg(const ScriptCommand& command, std::string_view key);
std::optional<int32_t> parseIntArg(const ScriptCommand& command, std::string_view key);
std::optional<double> parseDoubleArg(const ScriptCommand& command, std::string_view key);

std::string makePathString(const std::filesystem::path& path);
bool writeBytesToFile(const std::filesystem::path& path, const std::vector<uint8_t>& bytes,
                      std::vector<std::string>& messages);
bool readBytesFromFile(const std::filesystem::path& path, std::vector<uint8_t>& outBytes,
                       std::vector<std::string>& messages);

uint64_t hashBytesFnv1a64(const std::vector<uint8_t>& bytes) noexcept;
std::string hashHex(uint64_t value);
std::optional<std::string> extractJsonStringField(std::string_view text, std::string_view key);
std::optional<uint64_t> parseExpectedHashArg(const ScriptCommand& command);

size_t byteDiffCount(const std::vector<uint8_t>& a, const std::vector<uint8_t>& b) noexcept;
size_t firstByteDiff(const std::vector<uint8_t>& a, const std::vector<uint8_t>& b) noexcept;

std::string jsonEscape(std::string_view text);
std::string joinCommandsJson(const std::vector<std::string>& commands);
std::string serializeCommandSurfaceJson(const std::vector<std::string>& commands);
std::string serializeCommandSurfaceCompactJson(const std::vector<std::string>& commands);

std::string makeDiagnosticsReportJson(const ScriptContext& context,
                                      const std::vector<std::string>& commands);
std::string makeReplaySummaryBundleJson(const ScriptContext& context,
                                        const std::vector<std::string>& commands);

std::vector<uint8_t> serializeMeshState(const nexus::geometry::Mesh& mesh);
std::vector<uint8_t> serializeGaussianState(const nexus::gfx::GaussianSplatCloud& cloud);
std::vector<uint8_t> serializeSceneState(const nexus::asset::SceneAsset& scene);
std::vector<uint8_t> serializeParametricState(const nexus::parametric::ConstraintGraph& graph);
std::vector<uint8_t> serializeAnimationState(const nexus::animation::Skeleton& skeleton,
                                             const nexus::animation::Pose& pose);
std::vector<uint8_t> serializeCrossSolverState(const ScriptContext& context);
std::string makeCrossSolverBundleJson(const ScriptContext& context);

} // namespace nexus::automation
