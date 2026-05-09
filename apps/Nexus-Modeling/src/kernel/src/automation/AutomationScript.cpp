// ─────────────────────────────────────────────────────────────────────────────
//  Nexus Automation — Scripting and Batch Pipeline v0
// ─────────────────────────────────────────────────────────────────────────────
#include <nexus/automation/AutomationScript.h>

#include <algorithm>
#include <charconv>
#include <cctype>
#include <cmath>
#include <fstream>
#include <sstream>
#include <utility>

namespace nexus::automation {
namespace {

[[nodiscard]] nexus::render::Mat4 makeTranslationMatrix(float x, float y, float z) noexcept
{
    nexus::render::Mat4 m = nexus::render::Mat4::identity();
    m.m[0][3] = x;
    m.m[1][3] = y;
    m.m[2][3] = z;
    return m;
}

[[nodiscard]] nexus::render::Mat4 makeScaleMatrix(float x, float y, float z) noexcept
{
    nexus::render::Mat4 m = nexus::render::Mat4::identity();
    m.m[0][0] = x;
    m.m[1][1] = y;
    m.m[2][2] = z;
    return m;
}

[[nodiscard]] std::string trimCopy(std::string_view text)
{
    size_t begin = 0;
    size_t end = text.size();
    while (begin < end && std::isspace(static_cast<unsigned char>(text[begin]))) {
        ++begin;
    }
    while (end > begin && std::isspace(static_cast<unsigned char>(text[end - 1]))) {
        --end;
    }
    return std::string(text.substr(begin, end - begin));
}

[[nodiscard]] std::vector<std::string> tokenizeLine(std::string_view line)
{
    std::vector<std::string> tokens;
    size_t i = 0;
    while (i < line.size()) {
        while (i < line.size() && std::isspace(static_cast<unsigned char>(line[i]))) {
            ++i;
        }
        if (i >= line.size()) {
            break;
        }
        if (line[i] == '#') {
            break;
        }

        std::string token;
        if (line[i] == '"') {
            ++i;
            while (i < line.size()) {
                const char c = line[i++];
                if (c == '"') {
                    break;
                }
                if (c == '\\' && i < line.size()) {
                    token.push_back(line[i++]);
                } else {
                    token.push_back(c);
                }
            }
        } else {
            while (i < line.size() && !std::isspace(static_cast<unsigned char>(line[i]))) {
                if (line[i] == '#') {
                    break;
                }
                token.push_back(line[i++]);
            }
        }

        if (!token.empty()) {
            tokens.push_back(std::move(token));
        }

        while (i < line.size() && !std::isspace(static_cast<unsigned char>(line[i]))) {
            if (line[i] == '#') {
                i = line.size();
                break;
            }
            ++i;
        }
    }
    return tokens;
}

[[nodiscard]] bool parseKeyValueToken(const std::string& token,
                                      std::string& key,
                                      std::string& value)
{
    const size_t eq = token.find('=');
    if (eq == std::string::npos || eq == 0 || eq + 1 >= token.size()) {
        return false;
    }
    key = token.substr(0, eq);
    value = token.substr(eq + 1);
    return true;
}

[[nodiscard]] std::optional<std::string> getArg(const ScriptCommand& command, std::string_view key)
{
    auto it = command.args.find(std::string(key));
    if (it == command.args.end()) {
        return std::nullopt;
    }
    return it->second;
}

[[nodiscard]] std::optional<float> parseFloatArg(const ScriptCommand& command, std::string_view key)
{
    const auto text = getArg(command, key);
    if (!text) {
        return std::nullopt;
    }
    char* end = nullptr;
    const float value = std::strtof(text->c_str(), &end);
    if (end == text->c_str() || *end != '\0') {
        return std::nullopt;
    }
    return value;
}

[[nodiscard]] std::optional<int32_t> parseIntArg(const ScriptCommand& command, std::string_view key)
{
    const auto text = getArg(command, key);
    if (!text) {
        return std::nullopt;
    }
    int32_t value = 0;
    const auto first = text->data();
    const auto last = text->data() + text->size();
    const auto result = std::from_chars(first, last, value);
    if (result.ec != std::errc{} || result.ptr != last) {
        return std::nullopt;
    }
    return value;
}

[[nodiscard]] std::string makePathString(const std::filesystem::path& path)
{
    return path.lexically_normal().string();
}

} // namespace

bool ScriptRegistry::registerCommand(std::string name, CommandHandler handler)
{
    if (name.empty() || !handler) {
        return false;
    }
    m_handlers.insert_or_assign(std::move(name), std::move(handler));
    return true;
}

bool ScriptRegistry::hasCommand(std::string_view name) const
{
    return m_handlers.find(std::string(name)) != m_handlers.end();
}

bool ScriptRegistry::execute(ScriptContext& context,
                             const ScriptCommand& command,
                             std::vector<std::string>& messages) const
{
    const auto it = m_handlers.find(command.name);
    if (it == m_handlers.end()) {
        messages.push_back("unknown command: " + command.name);
        return false;
    }
    return it->second(context, command, messages);
}

ScriptBatchHarness::ScriptBatchHarness()
{
    registerBuiltinCommands();
}

std::string ScriptBatchHarness::normalizePath(const std::filesystem::path& base,
                                              const std::string& pathText)
{
    const std::filesystem::path path(pathText);
    if (path.is_absolute()) {
        return makePathString(path);
    }
    return makePathString(base / path);
}

ScriptRunReport ScriptBatchHarness::runScript(std::string_view scriptText,
                                              ScriptContext& context) const
{
    ScriptRunReport report{};
    const auto lines = splitScriptLines(scriptText);
    for (size_t lineIndex = 0; lineIndex < lines.size(); ++lineIndex) {
        const std::string& line = lines[lineIndex];
        std::vector<std::string> parseErrors;
        ScriptCommand command = parseCommandLine(line, parseErrors);
        if (!parseErrors.empty()) {
            report.valid = false;
            report.messages.insert(report.messages.end(), parseErrors.begin(), parseErrors.end());
            report.steps.push_back(ScriptStepReport{lineIndex + 1, {}, false, parseErrors});
            continue;
        }
        if (command.name.empty()) {
            continue;
        }

        std::vector<std::string> messages;
        const bool success = m_registry.execute(context, command, messages);
        report.steps.push_back(ScriptStepReport{lineIndex + 1, command.name, success, messages});
        if (!success) {
            report.valid = false;
            report.messages.insert(report.messages.end(), messages.begin(), messages.end());
        }
    }
    return report;
}

ScriptRunReport ScriptBatchHarness::runScriptFile(const std::filesystem::path& scriptPath,
                                                 ScriptContext& context) const
{
    std::ifstream in(scriptPath);
    if (!in) {
        ScriptRunReport report{};
        report.valid = false;
        report.messages.push_back("failed to open script file: " + makePathString(scriptPath));
        return report;
    }

    std::ostringstream buffer;
    buffer << in.rdbuf();
    context.workingDirectory = scriptPath.parent_path();
    return runScript(buffer.str(), context);
}

ScriptCommand ScriptBatchHarness::parseCommandLine(std::string_view line,
                                                   std::vector<std::string>& errors)
{
    ScriptCommand command{};
    const auto tokens = tokenizeLine(line);
    if (tokens.empty()) {
        return command;
    }

    command.name = tokens.front();
    for (size_t i = 1; i < tokens.size(); ++i) {
        std::string key;
        std::string value;
        if (!parseKeyValueToken(tokens[i], key, value)) {
            errors.push_back("invalid argument token: " + tokens[i]);
            continue;
        }
        command.args.emplace(std::move(key), std::move(value));
    }
    return command;
}

std::vector<std::string> ScriptBatchHarness::splitScriptLines(std::string_view scriptText)
{
    std::vector<std::string> lines;
    std::string current;
    for (char c : scriptText) {
        if (c == '\n') {
            lines.push_back(trimCopy(current));
            current.clear();
        } else if (c != '\r') {
            current.push_back(c);
        }
    }
    if (!current.empty() || (!scriptText.empty() && scriptText.back() == '\n')) {
        lines.push_back(trimCopy(current));
    }
    return lines;
}

std::optional<std::string> ScriptBatchHarness::stripQuotes(std::string_view text)
{
    if (text.size() >= 2 && text.front() == '"' && text.back() == '"') {
        return std::string(text.substr(1, text.size() - 2));
    }
    return std::nullopt;
}

std::optional<float> ScriptBatchHarness::parseFloat(std::string_view text)
{
    char* end = nullptr;
    const std::string tmp(text);
    const float value = std::strtof(tmp.c_str(), &end);
    if (end == tmp.c_str() || *end != '\0') {
        return std::nullopt;
    }
    return value;
}

std::optional<int32_t> ScriptBatchHarness::parseInt(std::string_view text)
{
    int32_t value = 0;
    const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
    if (result.ec != std::errc{} || result.ptr != text.data() + text.size()) {
        return std::nullopt;
    }
    return value;
}

std::optional<bool> ScriptBatchHarness::parseBool(std::string_view text)
{
    if (text == "1" || text == "true" || text == "True" || text == "yes") {
        return true;
    }
    if (text == "0" || text == "false" || text == "False" || text == "no") {
        return false;
    }
    return std::nullopt;
}

void ScriptBatchHarness::registerBuiltinCommands()
{
    using namespace nexus::geometry;
    using namespace nexus::asset;
    using namespace nexus::animation;
    using namespace nexus::gfx;

    m_registry.registerCommand("mesh.make_triangle",
        [](ScriptContext& context, const ScriptCommand& command, std::vector<std::string>& messages) {
            const auto size = parseFloatArg(command, "size").value_or(1.f);
            context.mesh = nexus::geometry::primitives::makeTriangle(size);
            context.hasMesh = true;
            context.meshName = command.args.contains("name") ? command.args.at("name") : "triangle";
            messages.push_back("mesh created: " + context.meshName);
            return true;
        });

    m_registry.registerCommand("mesh.compute_normals",
        [](ScriptContext& context, const ScriptCommand&, std::vector<std::string>& messages) {
            if (!context.hasMesh) {
                messages.push_back("mesh.compute_normals requires mesh.make_triangle or mesh.load first");
                return false;
            }
            if (!context.mesh.computeVertexNormals()) {
                messages.push_back("mesh.compute_normals failed");
                return false;
            }
            messages.push_back("mesh normals computed");
            return true;
        });

    m_registry.registerCommand("mesh.transform",
        [](ScriptContext& context, const ScriptCommand& command, std::vector<std::string>& messages) {
            if (!context.hasMesh) {
                messages.push_back("mesh.transform requires a mesh");
                return false;
            }

            const float tx = parseFloatArg(command, "tx").value_or(0.f);
            const float ty = parseFloatArg(command, "ty").value_or(0.f);
            const float tz = parseFloatArg(command, "tz").value_or(0.f);
            const float sx = parseFloatArg(command, "sx").value_or(1.f);
            const float sy = parseFloatArg(command, "sy").value_or(1.f);
            const float sz = parseFloatArg(command, "sz").value_or(1.f);

            nexus::render::Mat4 transform = makeTranslationMatrix(tx, ty, tz);
            const nexus::render::Mat4 scale = makeScaleMatrix(sx, sy, sz);
            transform = transform * scale;

            const GeometryCommandDesc desc{GeometryCommandType::Transform, transform};
            const GeometryCommandReport report = GeometryCommandSurface::execute(context.mesh, desc);
            if (!report.valid) {
                messages.insert(messages.end(), report.errors.begin(), report.errors.end());
                return false;
            }
            messages.push_back("mesh transformed");
            return true;
        });

    m_registry.registerCommand("mesh.export",
        [](ScriptContext& context, const ScriptCommand& command, std::vector<std::string>& messages) {
            if (!context.hasMesh) {
                messages.push_back("mesh.export requires a mesh");
                return false;
            }
            const auto pathArg = getArg(command, "path");
            if (!pathArg) {
                messages.push_back("mesh.export requires path=");
                return false;
            }
            const std::string formatText = command.args.contains("format") ? command.args.at("format") : "obj";
            MeshExportOptions options{};
            if (formatText == "ply") {
                options.format = MeshExportFormat::PLY;
            } else {
                options.format = MeshExportFormat::OBJ;
            }

            const std::filesystem::path target = normalizePath(context.workingDirectory, *pathArg);
            const MeshExportReport report = MeshIO::exportMesh(context.mesh, target.string(), options);
            if (!report.valid) {
                messages.insert(messages.end(), report.messages.begin(), report.messages.end());
                return false;
            }
            messages.push_back("mesh exported: " + target.string());
            return true;
        });

    m_registry.registerCommand("scene.new",
        [](ScriptContext& context, const ScriptCommand& command, std::vector<std::string>& messages) {
            context.scene = SceneAsset{};
            context.hasScene = true;
            if (command.args.contains("name")) {
                context.scene.setSceneName(command.args.at("name"));
            }
            context.sceneName = context.scene.sceneName();
            messages.push_back("scene created: " + context.sceneName);
            return true;
        });

    m_registry.registerCommand("scene.add_mesh_entry",
        [](ScriptContext& context, const ScriptCommand& command, std::vector<std::string>& messages) {
            if (!context.hasMesh) {
                messages.push_back("scene.add_mesh_entry requires a current mesh");
                return false;
            }
            if (!context.hasScene) {
                context.scene = SceneAsset{};
                context.hasScene = true;
            }

            SceneMeshEntry entry;
            entry.name = command.args.contains("name") ? command.args.at("name") : context.meshName;
            if (entry.name.empty()) {
                entry.name = "mesh_entry";
            }
            entry.sourcePath = command.args.contains("source") ? command.args.at("source") : std::string{};
            entry.mesh = context.mesh;
            context.scene.addEntry(std::move(entry));
            messages.push_back("scene entry added: " + context.scene.entry(context.scene.entryCount() - 1).name);
            return true;
        });

    m_registry.registerCommand("scene.load",
        [](ScriptContext& context, const ScriptCommand& command, std::vector<std::string>& messages) {
            const auto pathArg = getArg(command, "path");
            if (!pathArg) {
                messages.push_back("scene.load requires path=");
                return false;
            }
            const std::filesystem::path target = normalizePath(context.workingDirectory, *pathArg);
            if (!context.hasScene) {
                context.scene = SceneAsset{};
                context.hasScene = true;
            }
            const SceneAssetIOReport report = context.scene.load(target.string());
            if (!report.valid) {
                messages.insert(messages.end(), report.messages.begin(), report.messages.end());
                return false;
            }
            context.sceneName = context.scene.sceneName();
            messages.push_back("scene loaded: " + target.string());
            return true;
        });

    m_registry.registerCommand("scene.save",
        [](ScriptContext& context, const ScriptCommand& command, std::vector<std::string>& messages) {
            if (!context.hasScene) {
                messages.push_back("scene.save requires a scene");
                return false;
            }
            const auto pathArg = getArg(command, "path");
            if (!pathArg) {
                messages.push_back("scene.save requires path=");
                return false;
            }
            const std::filesystem::path target = normalizePath(context.workingDirectory, *pathArg);
            const SceneAssetIOReport report = context.scene.save(target.string());
            if (!report.valid) {
                messages.insert(messages.end(), report.messages.begin(), report.messages.end());
                return false;
            }
            messages.push_back("scene saved: " + target.string());
            return true;
        });

    m_registry.registerCommand("scene.export_text",
        [](ScriptContext& context, const ScriptCommand& command, std::vector<std::string>& messages) {
            if (!context.hasScene) {
                messages.push_back("scene.export_text requires a scene");
                return false;
            }
            const auto pathArg = getArg(command, "path");
            if (!pathArg) {
                messages.push_back("scene.export_text requires path=");
                return false;
            }
            const std::filesystem::path target = normalizePath(context.workingDirectory, *pathArg);
            const SceneAssetIOReport report = context.scene.exportText(target.string());
            if (!report.valid) {
                messages.insert(messages.end(), report.messages.begin(), report.messages.end());
                return false;
            }
            messages.push_back("scene text exported: " + target.string());
            return true;
        });

    m_registry.registerCommand("render.create_null_context",
        [](ScriptContext& context, const ScriptCommand& command, std::vector<std::string>& messages) {
            RenderContextDesc desc{};
            desc.preferredBackend = Backend::Null;
            desc.validation = ValidationLevel::Off;
            auto renderContext = RenderContext::create(desc);
            if (!renderContext) {
                messages.push_back("render context creation failed");
                return false;
            }
            context.renderContext = std::move(renderContext);
            const auto width = parseIntArg(command, "width").value_or(1280);
            const auto height = parseIntArg(command, "height").value_or(720);
            messages.push_back("render context created: null " + std::to_string(width) + "x" + std::to_string(height));
            return true;
        });

    m_registry.registerCommand("render.describe",
        [](ScriptContext& context, const ScriptCommand&, std::vector<std::string>& messages) {
            if (!context.renderContext) {
                messages.push_back("render.describe requires render.create_null_context first");
                return false;
            }
            messages.push_back("backend=" + std::string(context.renderContext->activeBackend() == Backend::Null ? "null" : "other"));
            messages.push_back("tier=" + std::to_string(static_cast<int>(context.renderContext->hardwareTier())));
            return true;
        });

    m_registry.registerCommand("render.plan_frame",
        [](ScriptContext& context, const ScriptCommand& command, std::vector<std::string>& messages) {
            (void)command;
            if (!context.renderContext) {
                messages.push_back("render.plan_frame requires render.create_null_context first");
                return false;
            }
            auto& device = context.renderContext->device();
            const CmdBufHandle cmd = device.allocateCommandBuffer(QueueType::Graphics);
            if (!cmd.valid()) {
                messages.push_back("render.plan_frame failed to allocate command buffer");
                return false;
            }
            RenderPassGraphPlanner planner;
            const auto pass = planner.addPass(RenderPassGraphPlanner::PassDesc{QueueType::Graphics, std::span<const CmdBufHandle>(&cmd, 1)});
            const auto plan = planner.buildSubmissionPlan({});
            messages.push_back("render plan pass_count=" + std::to_string(pass + 1));
            messages.push_back("render plan submit_count=" + std::to_string(plan.submits.size()));
            device.freeCommandBuffer(cmd);
            return true;
        });

    m_registry.registerCommand("animation.add_bone",
        [](ScriptContext& context, const ScriptCommand& command, std::vector<std::string>& messages) {
            const auto nameArg = getArg(command, "name");
            if (!nameArg || nameArg->empty()) {
                messages.push_back("animation.add_bone requires name=");
                return false;
            }
            const int32_t parent = parseIntArg(command, "parent").value_or(-1);
            BoneDesc bone{};
            bone.name = *nameArg;
            bone.parentIndex = parent;
            bone.bindLocal.translation.x = parseFloatArg(command, "tx").value_or(0.f);
            bone.bindLocal.translation.y = parseFloatArg(command, "ty").value_or(0.f);
            bone.bindLocal.translation.z = parseFloatArg(command, "tz").value_or(0.f);
            bone.bindLocal.scale.x = parseFloatArg(command, "sx").value_or(1.f);
            bone.bindLocal.scale.y = parseFloatArg(command, "sy").value_or(1.f);
            bone.bindLocal.scale.z = parseFloatArg(command, "sz").value_or(1.f);

            const int32_t index = context.skeleton.addBone(std::move(bone));
            if (index < 0) {
                messages.push_back("animation.add_bone failed");
                return false;
            }
            context.hasSkeleton = true;
            context.pose.resize(context.skeleton.boneCount());
            messages.push_back("bone added: " + *nameArg);
            return true;
        });

    m_registry.registerCommand("animation.reset_pose",
        [](ScriptContext& context, const ScriptCommand&, std::vector<std::string>& messages) {
            if (!context.hasSkeleton) {
                messages.push_back("animation.reset_pose requires a skeleton");
                return false;
            }
            context.pose.resize(context.skeleton.boneCount());
            for (size_t i = 0; i < context.skeleton.boneCount(); ++i) {
                context.pose.setLocalTransform(i, context.skeleton.bindLocal(i));
            }
            messages.push_back("pose reset to bind local");
            return true;
        });

    m_registry.registerCommand("animation.sample_bind_pose",
        [](ScriptContext& context, const ScriptCommand&, std::vector<std::string>& messages) {
            if (!context.hasSkeleton) {
                messages.push_back("animation.sample_bind_pose requires a skeleton");
                return false;
            }
            context.pose.resize(context.skeleton.boneCount());
            for (size_t i = 0; i < context.skeleton.boneCount(); ++i) {
                context.pose.setLocalTransform(i, context.skeleton.bindLocal(i));
            }
            context.pose.computeModelMatrices(context.skeleton);
            messages.push_back("bind pose sampled");
            return true;
        });
}

} // namespace nexus::automation
