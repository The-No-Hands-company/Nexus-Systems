#include "AutomationScriptCommands.h"
#include "AutomationScriptUtilities.h"
#include <nexus/automation/AutomationScript.h>

#include <nexus/geometry/GeometryKernel.h>
#include <nexus/geometry/MeshIO.h>
#include <nexus/gfx/RenderContext.h>
#include <nexus/parametric/ParametricSolver.h>
#include <nexus/render/SceneGraph.h>

#include <algorithm>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

namespace nexus::automation {

void registerMetaCommands(ScriptRegistry& registry)
{
    using namespace nexus::geometry;
    using namespace nexus::asset;
    using namespace nexus::animation;
    using namespace nexus::gfx;

    registry.registerCommand("script.list_commands",
        [&registry](ScriptContext&, const ScriptCommand&, std::vector<std::string>& messages) {
            const auto commands = registry.listCommands();
            messages.push_back("command_count=" + std::to_string(commands.size()));
            for (const auto& name : commands) {
                messages.push_back("command=" + name);
            }
            return true;
        });

    registry.registerCommand("script.commands_hash",
        [&registry](ScriptContext&, const ScriptCommand&, std::vector<std::string>& messages) {
            const auto commands = registry.listCommands();
            std::vector<uint8_t> bytes;
            for (const auto& name : commands) {
                bytes.insert(bytes.end(), name.begin(), name.end());
                bytes.push_back(static_cast<uint8_t>('\n'));
            }
            messages.push_back("commands hash=" + hashHex(hashBytesFnv1a64(bytes))
                + " count=" + std::to_string(commands.size()));
            return true;
        });

    registry.registerCommand("script.assert_command",
        [&registry](ScriptContext&, const ScriptCommand& command, std::vector<std::string>& messages) {
            const auto nameArg = getArg(command, "name");
            if (!nameArg || nameArg->empty()) {
                messages.push_back("script.assert_command requires name=");
                return false;
            }
            if (!registry.hasCommand(*nameArg)) {
                messages.push_back("script.assert_command missing: " + *nameArg);
                return false;
            }
            messages.push_back("script.assert_command present: " + *nameArg);
            return true;
        });

    registry.registerCommand("script.export_report",
        [&registry](ScriptContext& context, const ScriptCommand& command, std::vector<std::string>& messages) {
            const auto pathArg = getArg(command, "path");
            if (!pathArg) {
                messages.push_back("script.export_report requires path=");
                return false;
            }
            const std::filesystem::path target = ScriptBatchHarness::normalizePath(context.workingDirectory, *pathArg);
            const std::string report = makeDiagnosticsReportJson(context, registry.listCommands());
            std::vector<uint8_t> bytes(report.begin(), report.end());
            if (!writeBytesToFile(target, bytes, messages)) {
                return false;
            }
            messages.push_back("report exported bytes=" + std::to_string(bytes.size()));
            return true;
        });

    registry.registerCommand("script.export_replay_bundle",
        [&registry](ScriptContext& context, const ScriptCommand& command, std::vector<std::string>& messages) {
            const auto pathArg = getArg(command, "path");
            if (!pathArg) {
                messages.push_back("script.export_replay_bundle requires path=");
                return false;
            }
            const std::filesystem::path target = ScriptBatchHarness::normalizePath(context.workingDirectory, *pathArg);
            const std::string report = makeReplaySummaryBundleJson(context, registry.listCommands());
            std::vector<uint8_t> bytes(report.begin(), report.end());
            if (!writeBytesToFile(target, bytes, messages)) {
                return false;
            }
            messages.push_back("replay bundle exported bytes=" + std::to_string(bytes.size()));
            return true;
        });

    registry.registerCommand("script.verify_replay_bundle",
        [&registry](ScriptContext& context, const ScriptCommand& command, std::vector<std::string>& messages) {
            const auto pathArg = getArg(command, "path");
            if (!pathArg) {
                messages.push_back("script.verify_replay_bundle requires path=");
                return false;
            }

            const std::filesystem::path source = ScriptBatchHarness::normalizePath(context.workingDirectory, *pathArg);
            std::vector<uint8_t> input;
            if (!readBytesFromFile(source, input, messages)) {
                return false;
            }

            const std::string expectedText(input.begin(), input.end());
            const auto expectedHash = extractJsonStringField(expectedText, "replay_hash");
            if (!expectedHash) {
                messages.push_back("script.verify_replay_bundle missing replay_hash in: " + makePathString(source));
                return false;
            }

            const std::string current = makeReplaySummaryBundleJson(context, registry.listCommands());
            const auto actualHash = extractJsonStringField(current, "replay_hash");
            if (!actualHash) {
                messages.push_back("script.verify_replay_bundle failed to compute replay_hash");
                return false;
            }

            if (*expectedHash != *actualHash) {
                messages.push_back("script.verify_replay_bundle mismatch expected=" + *expectedHash
                    + " actual=" + *actualHash);
                return false;
            }

            messages.push_back("script.verify_replay_bundle match: " + *actualHash);
            return true;
        });
}

void registerMeshCommands(ScriptRegistry& registry)
{
    using namespace nexus::geometry;
    using namespace nexus::asset;

    registry.registerCommand("mesh.make_triangle",
        [](ScriptContext& context, const ScriptCommand& command, std::vector<std::string>& messages) {
            const auto size = parseFloatArg(command, "size").value_or(1.f);
            context.mesh = nexus::geometry::primitives::makeTriangle(size);
            context.hasMesh = true;
            context.meshName = command.args.contains("name") ? command.args.at("name") : "triangle";
            messages.push_back("mesh created: " + context.meshName);
            return true;
        });

    registry.registerCommand("mesh.make_sphere",
        [](ScriptContext& context, const ScriptCommand& command, std::vector<std::string>& messages) {
            const float radius  = parseFloatArg(command, "radius").value_or(1.f);
            const auto latSegs  = static_cast<uint32_t>(std::max(2, parseIntArg(command, "lat_segs").value_or(16)));
            const auto lonSegs  = static_cast<uint32_t>(std::max(3, parseIntArg(command, "lon_segs").value_or(16)));
            context.mesh = nexus::geometry::primitives::makeSphere(radius, latSegs, lonSegs);
            context.hasMesh = true;
            context.meshName = command.args.contains("name") ? command.args.at("name") : "sphere";
            messages.push_back("mesh created: " + context.meshName
                + " vertices=" + std::to_string(context.mesh.attributes().vertexCount())
                + " faces=" + std::to_string(context.mesh.topology().faceCount()));
            return true;
        });

    registry.registerCommand("mesh.make_cylinder",
        [](ScriptContext& context, const ScriptCommand& command, std::vector<std::string>& messages) {
            const float radius    = parseFloatArg(command, "radius").value_or(1.f);
            const float height    = parseFloatArg(command, "height").value_or(2.f);
            const auto radialSegs = static_cast<uint32_t>(std::max(3, parseIntArg(command, "segs").value_or(16)));
            context.mesh = nexus::geometry::primitives::makeCylinder(radius, height, radialSegs);
            context.hasMesh = true;
            context.meshName = command.args.contains("name") ? command.args.at("name") : "cylinder";
            messages.push_back("mesh created: " + context.meshName
                + " vertices=" + std::to_string(context.mesh.attributes().vertexCount())
                + " faces=" + std::to_string(context.mesh.topology().faceCount()));
            return true;
        });

    registry.registerCommand("mesh.make_cone",
        [](ScriptContext& context, const ScriptCommand& command, std::vector<std::string>& messages) {
            const float radius    = parseFloatArg(command, "radius").value_or(1.f);
            const float height    = parseFloatArg(command, "height").value_or(2.f);
            const auto radialSegs = static_cast<uint32_t>(std::max(3, parseIntArg(command, "segs").value_or(16)));
            context.mesh = nexus::geometry::primitives::makeCone(radius, height, radialSegs);
            context.hasMesh = true;
            context.meshName = command.args.contains("name") ? command.args.at("name") : "cone";
            messages.push_back("mesh created: " + context.meshName
                + " vertices=" + std::to_string(context.mesh.attributes().vertexCount())
                + " faces=" + std::to_string(context.mesh.topology().faceCount()));
            return true;
        });

    registry.registerCommand("mesh.make_capsule",
        [](ScriptContext& context, const ScriptCommand& command, std::vector<std::string>& messages) {
            const float radius         = parseFloatArg(command, "radius").value_or(0.5f);
            const float cylinderHeight = parseFloatArg(command, "height").value_or(1.f);
            const auto radialSegs      = static_cast<uint32_t>(std::max(3, parseIntArg(command, "segs").value_or(16)));
            const auto ringSegs        = static_cast<uint32_t>(std::max(2, parseIntArg(command, "ring_segs").value_or(8)));
            context.mesh = nexus::geometry::primitives::makeCapsule(radius, cylinderHeight, radialSegs, ringSegs);
            context.hasMesh = true;
            context.meshName = command.args.contains("name") ? command.args.at("name") : "capsule";
            messages.push_back("mesh created: " + context.meshName
                + " vertices=" + std::to_string(context.mesh.attributes().vertexCount())
                + " faces=" + std::to_string(context.mesh.topology().faceCount()));
            return true;
        });

    registry.registerCommand("mesh.compute_normals",
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

    registry.registerCommand("mesh.transform",
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

    registry.registerCommand("mesh.export",
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

            const std::filesystem::path target = ScriptBatchHarness::normalizePath(context.workingDirectory, *pathArg);
            const MeshExportReport report = MeshIO::exportMesh(context.mesh, target.string(), options);
            if (!report.valid) {
                messages.insert(messages.end(), report.messages.begin(), report.messages.end());
                return false;
            }
            messages.push_back("mesh exported: " + target.string());
            return true;
        });

    registry.registerCommand("mesh.hash",
        [](ScriptContext& context, const ScriptCommand&, std::vector<std::string>& messages) {
            if (!context.hasMesh) {
                messages.push_back("mesh.hash requires a mesh");
                return false;
            }
            const auto bytes = serializeMeshState(context.mesh);
            messages.push_back("mesh hash=" + hashHex(hashBytesFnv1a64(bytes))
                + " vertices=" + std::to_string(context.mesh.attributes().vertexCount())
                + " faces=" + std::to_string(context.mesh.topology().faceCount())
                + " bytes=" + std::to_string(bytes.size()));
            return true;
        });

    registry.registerCommand("mesh.set_baseline",
        [](ScriptContext& context, const ScriptCommand&, std::vector<std::string>& messages) {
            if (!context.hasMesh) {
                messages.push_back("mesh.set_baseline requires a mesh");
                return false;
            }
            context.meshBaseline = context.mesh;
            context.hasMeshBaseline = true;
            messages.push_back("mesh baseline set vertices="
                + std::to_string(context.mesh.attributes().vertexCount())
                + " faces=" + std::to_string(context.mesh.topology().faceCount()));
            return true;
        });

    registry.registerCommand("mesh.diff",
        [](ScriptContext& context, const ScriptCommand&, std::vector<std::string>& messages) {
            if (!context.hasMesh) {
                messages.push_back("mesh.diff requires a mesh");
                return false;
            }
            if (!context.hasMeshBaseline) {
                messages.push_back("mesh.diff requires mesh.set_baseline first");
                return false;
            }
            const auto currBytes = serializeMeshState(context.mesh);
            const auto baseBytes = serializeMeshState(context.meshBaseline);
            const size_t changed = byteDiffCount(baseBytes, currBytes);
            const size_t firstDiff = firstByteDiff(baseBytes, currBytes);
            messages.push_back("mesh diff equal=" + std::string(changed == 0 ? "1" : "0")
                + " changed=" + std::to_string(changed)
                + " first_diff=" + std::string(firstDiff == static_cast<size_t>(-1) ? "-1" : std::to_string(firstDiff))
                + " base_hash=" + hashHex(hashBytesFnv1a64(baseBytes))
                + " curr_hash=" + hashHex(hashBytesFnv1a64(currBytes)));
            return true;
        });

    registry.registerCommand("mesh.expect_hash",
        [](ScriptContext& context, const ScriptCommand& command, std::vector<std::string>& messages) {
            if (!context.hasMesh) {
                messages.push_back("mesh.expect_hash requires a mesh");
                return false;
            }
            const auto expected = parseExpectedHashArg(command);
            if (!expected) {
                messages.push_back("mesh.expect_hash requires hash=<uint64|0xHEX>");
                return false;
            }
            const auto bytes = serializeMeshState(context.mesh);
            const uint64_t actual = hashBytesFnv1a64(bytes);
            if (actual != *expected) {
                messages.push_back("mesh.expect_hash mismatch expected="
                    + hashHex(*expected) + " actual=" + hashHex(actual));
                return false;
            }
            messages.push_back("mesh.expect_hash match " + hashHex(actual));
            return true;
        });

    registry.registerCommand("mesh.export_bundle",
        [](ScriptContext& context, const ScriptCommand& command, std::vector<std::string>& messages) {
            if (!context.hasMesh) {
                messages.push_back("mesh.export_bundle requires a mesh");
                return false;
            }
            const auto pathArg = getArg(command, "path");
            if (!pathArg) {
                messages.push_back("mesh.export_bundle requires path=");
                return false;
            }
            const std::filesystem::path target = ScriptBatchHarness::normalizePath(context.workingDirectory, *pathArg);
            const auto bytes = serializeMeshState(context.mesh);
            const std::string meshHash = hashHex(hashBytesFnv1a64(bytes));
            std::ostringstream oss;
            oss << "{\"mesh_hash\":\"" << meshHash << "\""
                << ",\"vertex_count\":" << context.mesh.attributes().vertexCount()
                << ",\"face_count\":" << context.mesh.topology().faceCount()
                << "}";
            const std::string json = oss.str();
            std::vector<uint8_t> outBytes(json.begin(), json.end());
            if (!writeBytesToFile(target, outBytes, messages)) {
                return false;
            }
            messages.push_back("mesh bundle exported hash=" + meshHash
                + " bytes=" + std::to_string(outBytes.size()));
            return true;
        });

    registry.registerCommand("mesh.verify_bundle",
        [](ScriptContext& context, const ScriptCommand& command, std::vector<std::string>& messages) {
            if (!context.hasMesh) {
                messages.push_back("mesh.verify_bundle requires a mesh");
                return false;
            }
            const auto pathArg = getArg(command, "path");
            if (!pathArg) {
                messages.push_back("mesh.verify_bundle requires path=");
                return false;
            }
            const std::filesystem::path source = ScriptBatchHarness::normalizePath(context.workingDirectory, *pathArg);
            std::vector<uint8_t> input;
            if (!readBytesFromFile(source, input, messages)) {
                return false;
            }
            const std::string text(input.begin(), input.end());
            const auto expectedHash = extractJsonStringField(text, "mesh_hash");
            if (!expectedHash) {
                messages.push_back("mesh.verify_bundle missing mesh_hash in: " + makePathString(source));
                return false;
            }
            const auto currBytes = serializeMeshState(context.mesh);
            const std::string actualHash = hashHex(hashBytesFnv1a64(currBytes));
            if (*expectedHash != actualHash) {
                messages.push_back("mesh.verify_bundle mismatch expected=" + *expectedHash
                    + " actual=" + actualHash);
                return false;
            }
            messages.push_back("mesh.verify_bundle match: " + actualHash);
            return true;
        });
}

void registerSceneCommands(ScriptRegistry& registry)
{
    using namespace nexus::geometry;
    using namespace nexus::asset;

    registry.registerCommand("scene.new",
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

    registry.registerCommand("scene.add_mesh_entry",
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

    registry.registerCommand("scene.rename_entry",
        [](ScriptContext& context, const ScriptCommand& command, std::vector<std::string>& messages) {
            if (!context.hasScene) {
                messages.push_back("scene.rename_entry requires a scene");
                return false;
            }
            const auto toArg = getArg(command, "to");
            if (!toArg || toArg->empty()) {
                messages.push_back("scene.rename_entry requires to=");
                return false;
            }

            const bool hasIndexArg = command.args.contains("index");
            const auto fromArg    = getArg(command, "from");

            SceneMeshEntry* entry = nullptr;
            std::string     oldName;

            if (hasIndexArg) {
                const auto indexArg = parseIntArg(command, "index");
                if (!indexArg || *indexArg < 0) {
                    messages.push_back("scene.rename_entry invalid index");
                    return false;
                }
                const size_t idx = static_cast<size_t>(*indexArg);
                if (idx >= context.scene.entryCount()) {
                    messages.push_back("scene.rename_entry index out of range");
                    return false;
                }
                entry   = &context.scene.entry(idx);
                oldName = entry->name;
            } else {
                if (!fromArg || fromArg->empty()) {
                    messages.push_back("scene.rename_entry requires from= or index=");
                    return false;
                }
                entry = context.scene.findByName(*fromArg);
                if (!entry) {
                    messages.push_back("scene.rename_entry missing entry: " + *fromArg);
                    return false;
                }
                oldName = *fromArg;
            }

            if (oldName != *toArg && context.scene.findByName(*toArg)) {
                messages.push_back("scene.rename_entry target already exists: " + *toArg);
                return false;
            }

            entry->name = *toArg;
            messages.push_back("scene entry renamed: " + oldName + " -> " + *toArg);
            return true;
        });

    registry.registerCommand("scene.remove_entry",
        [](ScriptContext& context, const ScriptCommand& command, std::vector<std::string>& messages) {
            if (!context.hasScene) {
                messages.push_back("scene.remove_entry requires a scene");
                return false;
            }
            const bool hasIndexArg = command.args.contains("index");
            const auto nameArg = getArg(command, "name");
            if (!hasIndexArg && (!nameArg || nameArg->empty())) {
                messages.push_back("scene.remove_entry requires name= or index=");
                return false;
            }

            if (hasIndexArg) {
                const auto indexArg = parseIntArg(command, "index");
                if (!indexArg || *indexArg < 0) {
                    messages.push_back("scene.remove_entry invalid index");
                    return false;
                }
                const size_t removeIndex = static_cast<size_t>(*indexArg);
                if (removeIndex >= context.scene.entryCount()) {
                    messages.push_back("scene.remove_entry index out of range");
                    return false;
                }

                const std::string removedName = context.scene.entry(removeIndex).name;
                context.scene.removeEntry(removeIndex);
                messages.push_back("scene entry removed: " + removedName + " index=" + std::to_string(removeIndex));
                return true;
            }

            size_t removeIndex = context.scene.entryCount();
            for (size_t i = 0; i < context.scene.entryCount(); ++i) {
                if (context.scene.entry(i).name == *nameArg) {
                    removeIndex = i;
                    break;
                }
            }
            if (removeIndex == context.scene.entryCount()) {
                messages.push_back("scene.remove_entry missing entry: " + *nameArg);
                return false;
            }

            context.scene.removeEntry(removeIndex);
            messages.push_back("scene entry removed: " + *nameArg);
            return true;
        });

    registry.registerCommand("scene.load",
        [](ScriptContext& context, const ScriptCommand& command, std::vector<std::string>& messages) {
            const auto pathArg = getArg(command, "path");
            if (!pathArg) {
                messages.push_back("scene.load requires path=");
                return false;
            }
            const std::filesystem::path target = ScriptBatchHarness::normalizePath(context.workingDirectory, *pathArg);
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

    registry.registerCommand("scene.save",
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
            const std::filesystem::path target = ScriptBatchHarness::normalizePath(context.workingDirectory, *pathArg);
            const SceneAssetIOReport report = context.scene.save(target.string());
            if (!report.valid) {
                messages.insert(messages.end(), report.messages.begin(), report.messages.end());
                return false;
            }
            messages.push_back("scene saved: " + target.string());
            return true;
        });

    registry.registerCommand("scene.export_text",
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
            const std::filesystem::path target = ScriptBatchHarness::normalizePath(context.workingDirectory, *pathArg);
            const SceneAssetIOReport report = context.scene.exportText(target.string());
            if (!report.valid) {
                messages.insert(messages.end(), report.messages.begin(), report.messages.end());
                return false;
            }
            messages.push_back("scene text exported: " + target.string());
            return true;
        });

    registry.registerCommand("scene.hash",
        [](ScriptContext& context, const ScriptCommand&, std::vector<std::string>& messages) {
            if (!context.hasScene) {
                messages.push_back("scene.hash requires a scene");
                return false;
            }
            const auto bytes = serializeSceneState(context.scene);
            messages.push_back("scene hash=" + hashHex(hashBytesFnv1a64(bytes))
                + " entries=" + std::to_string(context.scene.entryCount())
                + " bytes=" + std::to_string(bytes.size()));
            return true;
        });

    registry.registerCommand("scene.set_baseline",
        [](ScriptContext& context, const ScriptCommand&, std::vector<std::string>& messages) {
            if (!context.hasScene) {
                messages.push_back("scene.set_baseline requires a scene");
                return false;
            }
            context.sceneBaselineBytes = serializeSceneState(context.scene);
            context.hasSceneBaseline = true;
            messages.push_back("scene baseline set entries="
                + std::to_string(context.scene.entryCount()));
            return true;
        });

    registry.registerCommand("scene.diff",
        [](ScriptContext& context, const ScriptCommand&, std::vector<std::string>& messages) {
            if (!context.hasScene) {
                messages.push_back("scene.diff requires a scene");
                return false;
            }
            if (!context.hasSceneBaseline) {
                messages.push_back("scene.diff requires scene.set_baseline first");
                return false;
            }
            const auto currBytes = serializeSceneState(context.scene);
            const auto& baseBytes = context.sceneBaselineBytes;
            const size_t changed = byteDiffCount(baseBytes, currBytes);
            const size_t firstDiff = firstByteDiff(baseBytes, currBytes);
            messages.push_back("scene diff equal=" + std::string(changed == 0 ? "1" : "0")
                + " changed=" + std::to_string(changed)
                + " first_diff=" + std::string(firstDiff == static_cast<size_t>(-1) ? "-1" : std::to_string(firstDiff))
                + " base_hash=" + hashHex(hashBytesFnv1a64(baseBytes))
                + " curr_hash=" + hashHex(hashBytesFnv1a64(currBytes)));
            return true;
        });

    registry.registerCommand("scene.export_bundle",
        [](ScriptContext& context, const ScriptCommand& command, std::vector<std::string>& messages) {
            if (!context.hasScene) {
                messages.push_back("scene.export_bundle requires a scene");
                return false;
            }
            const auto pathArg = getArg(command, "path");
            if (!pathArg) {
                messages.push_back("scene.export_bundle requires path=");
                return false;
            }
            const std::filesystem::path target = ScriptBatchHarness::normalizePath(context.workingDirectory, *pathArg);
            const auto bytes = serializeSceneState(context.scene);
            const std::string sceneHash = hashHex(hashBytesFnv1a64(bytes));
            std::ostringstream oss;
            oss << "{\"scene_hash\":\"" << sceneHash << "\""
                << ",\"entry_count\":" << context.scene.entryCount()
                << ",\"scene_name\":\"" << jsonEscape(context.scene.sceneName()) << "\""
                << "}";
            const std::string json = oss.str();
            std::vector<uint8_t> outBytes(json.begin(), json.end());
            if (!writeBytesToFile(target, outBytes, messages)) {
                return false;
            }
            messages.push_back("scene bundle exported hash=" + sceneHash
                + " bytes=" + std::to_string(outBytes.size()));
            return true;
        });

    registry.registerCommand("scene.verify_bundle",
        [](ScriptContext& context, const ScriptCommand& command, std::vector<std::string>& messages) {
            if (!context.hasScene) {
                messages.push_back("scene.verify_bundle requires a scene");
                return false;
            }
            const auto pathArg = getArg(command, "path");
            if (!pathArg) {
                messages.push_back("scene.verify_bundle requires path=");
                return false;
            }
            const std::filesystem::path source = ScriptBatchHarness::normalizePath(context.workingDirectory, *pathArg);
            std::vector<uint8_t> input;
            if (!readBytesFromFile(source, input, messages)) {
                return false;
            }
            const std::string text(input.begin(), input.end());
            const auto expectedHash = extractJsonStringField(text, "scene_hash");
            if (!expectedHash) {
                messages.push_back("scene.verify_bundle missing scene_hash in: " + makePathString(source));
                return false;
            }
            const auto currBytes = serializeSceneState(context.scene);
            const std::string actualHash = hashHex(hashBytesFnv1a64(currBytes));
            if (*expectedHash != actualHash) {
                messages.push_back("scene.verify_bundle mismatch expected=" + *expectedHash
                    + " actual=" + actualHash);
                return false;
            }
            messages.push_back("scene.verify_bundle match: " + actualHash);
            return true;
        });

    registry.registerCommand("scene.expect_hash",
        [](ScriptContext& context, const ScriptCommand& command, std::vector<std::string>& messages) {
            if (!context.hasScene) {
                messages.push_back("scene.expect_hash requires a scene");
                return false;
            }
            const auto expected = parseExpectedHashArg(command);
            if (!expected) {
                messages.push_back("scene.expect_hash requires hash=<uint64|0xHEX>");
                return false;
            }
            const auto bytes = serializeSceneState(context.scene);
            const uint64_t actual = hashBytesFnv1a64(bytes);
            if (actual != *expected) {
                messages.push_back("scene.expect_hash mismatch expected="
                    + hashHex(*expected) + " actual=" + hashHex(actual));
                return false;
            }
            messages.push_back("scene.expect_hash match " + hashHex(actual));
            return true;
        });

    registry.registerCommand("scene.describe",
        [](ScriptContext& context, const ScriptCommand&, std::vector<std::string>& messages) {
            if (!context.hasScene) {
                messages.push_back("scene.describe requires a scene");
                return false;
            }
            messages.push_back("scene name=" + context.scene.sceneName()
                + " entries=" + std::to_string(context.scene.entryCount()));
            return true;
        });

    registry.registerCommand("scene.list_entries",
        [](ScriptContext& context, const ScriptCommand&, std::vector<std::string>& messages) {
            if (!context.hasScene) {
                messages.push_back("scene.list_entries requires a scene");
                return false;
            }
            for (size_t i = 0; i < context.scene.entryCount(); ++i) {
                const auto& e = context.scene.entry(i);
                messages.push_back("scene entry index=" + std::to_string(i)
                    + " name=" + e.name
                    + " vertices=" + std::to_string(e.mesh.attributes().vertexCount())
                    + " faces=" + std::to_string(e.mesh.topology().faceCount())
                    + " visible=" + (e.visible ? "1" : "0"));
            }
            return true;
        });

    registry.registerCommand("scene.get_entry",
        [](ScriptContext& context, const ScriptCommand& command, std::vector<std::string>& messages) {
            if (!context.hasScene) {
                messages.push_back("scene.get_entry requires a scene");
                return false;
            }
            const bool hasIndex = command.args.contains("index");
            const auto nameArg = getArg(command, "name");
            const nexus::asset::SceneMeshEntry* entry = nullptr;
            size_t foundIdx = 0;
            if (hasIndex) {
                const auto indexArg = parseIntArg(command, "index");
                if (!indexArg || *indexArg < 0) {
                    messages.push_back("scene.get_entry requires valid index=");
                    return false;
                }
                foundIdx = static_cast<size_t>(*indexArg);
                if (foundIdx >= context.scene.entryCount()) {
                    messages.push_back("scene.get_entry index out of range");
                    return false;
                }
                entry = &context.scene.entry(foundIdx);
            } else {
                if (!nameArg || nameArg->empty()) {
                    messages.push_back("scene.get_entry requires name= or index=");
                    return false;
                }
                entry = context.scene.findByName(*nameArg);
                if (!entry) {
                    messages.push_back("scene.get_entry not found: " + *nameArg);
                    return false;
                }
                for (size_t i = 0; i < context.scene.entryCount(); ++i) {
                    if (&context.scene.entry(i) == entry) { foundIdx = i; break; }
                }
            }
            messages.push_back("scene entry index=" + std::to_string(foundIdx)
                + " name=" + entry->name
                + " vertices=" + std::to_string(entry->mesh.attributes().vertexCount())
                + " faces=" + std::to_string(entry->mesh.topology().faceCount())
                + " visible=" + (entry->visible ? "1" : "0"));
            return true;
        });
}

void registerRenderCommands(ScriptRegistry& registry)
{
    using namespace nexus::gfx;

    registry.registerCommand("render.create_null_context",
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

    registry.registerCommand("render.describe",
        [](ScriptContext& context, const ScriptCommand&, std::vector<std::string>& messages) {
            if (!context.renderContext) {
                messages.push_back("render.describe requires render.create_null_context first");
                return false;
            }
            messages.push_back("backend=" + std::string(context.renderContext->activeBackend() == Backend::Null ? "null" : "other"));
            messages.push_back("tier=" + std::to_string(static_cast<int>(context.renderContext->hardwareTier())));
            return true;
        });

    registry.registerCommand("render.plan_frame",
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
}

void registerAnimationCommands(ScriptRegistry& registry)
{
    using namespace nexus::animation;

    registry.registerCommand("animation.add_bone",
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

    registry.registerCommand("animation.reset_pose",
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

    registry.registerCommand("animation.sample_bind_pose",
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

    registry.registerCommand("animation.state_hash",
        [](ScriptContext& context, const ScriptCommand&, std::vector<std::string>& messages) {
            if (!context.hasSkeleton) {
                messages.push_back("animation.state_hash requires a skeleton");
                return false;
            }
            const auto bytes = serializeAnimationState(context.skeleton, context.pose);
            messages.push_back("animation hash=" + hashHex(hashBytesFnv1a64(bytes))
                + " bones=" + std::to_string(context.skeleton.boneCount())
                + " bytes=" + std::to_string(bytes.size()));
            return true;
        });

    registry.registerCommand("animation.set_baseline",
        [](ScriptContext& context, const ScriptCommand&, std::vector<std::string>& messages) {
            if (!context.hasSkeleton) {
                messages.push_back("animation.set_baseline requires a skeleton");
                return false;
            }
            context.skeletonBaseline = context.skeleton;
            context.poseBaseline = context.pose;
            context.hasAnimBaseline = true;
            messages.push_back("animation baseline set bones="
                + std::to_string(context.skeleton.boneCount()));
            return true;
        });

    registry.registerCommand("animation.diff",
        [](ScriptContext& context, const ScriptCommand&, std::vector<std::string>& messages) {
            if (!context.hasSkeleton) {
                messages.push_back("animation.diff requires a skeleton");
                return false;
            }
            if (!context.hasAnimBaseline) {
                messages.push_back("animation.diff requires animation.set_baseline first");
                return false;
            }
            const auto currBytes = serializeAnimationState(context.skeleton, context.pose);
            const auto baseBytes = serializeAnimationState(context.skeletonBaseline, context.poseBaseline);
            const size_t changed = byteDiffCount(baseBytes, currBytes);
            const size_t firstDiff = firstByteDiff(baseBytes, currBytes);
            messages.push_back("animation diff equal=" + std::string(changed == 0 ? "1" : "0")
                + " changed=" + std::to_string(changed)
                + " first_diff=" + std::string(firstDiff == static_cast<size_t>(-1) ? "-1" : std::to_string(firstDiff))
                + " base_hash=" + hashHex(hashBytesFnv1a64(baseBytes))
                + " curr_hash=" + hashHex(hashBytesFnv1a64(currBytes)));
            return true;
        });

    registry.registerCommand("animation.expect_hash",
        [](ScriptContext& context, const ScriptCommand& command, std::vector<std::string>& messages) {
            if (!context.hasSkeleton) {
                messages.push_back("animation.expect_hash requires a skeleton");
                return false;
            }
            const auto expected = parseExpectedHashArg(command);
            if (!expected) {
                messages.push_back("animation.expect_hash requires hash=<uint64|0xHEX>");
                return false;
            }
            const auto bytes = serializeAnimationState(context.skeleton, context.pose);
            const uint64_t actual = hashBytesFnv1a64(bytes);
            if (actual != *expected) {
                messages.push_back("animation.expect_hash mismatch expected="
                    + hashHex(*expected) + " actual=" + hashHex(actual));
                return false;
            }
            messages.push_back("animation.expect_hash match " + hashHex(actual));
            return true;
        });

    registry.registerCommand("animation.export_bundle",
        [](ScriptContext& context, const ScriptCommand& command, std::vector<std::string>& messages) {
            if (!context.hasSkeleton) {
                messages.push_back("animation.export_bundle requires a skeleton");
                return false;
            }
            const auto pathArg = getArg(command, "path");
            if (!pathArg) {
                messages.push_back("animation.export_bundle requires path=");
                return false;
            }
            const std::filesystem::path target = ScriptBatchHarness::normalizePath(context.workingDirectory, *pathArg);
            const auto bytes = serializeAnimationState(context.skeleton, context.pose);
            const std::string animationHash = hashHex(hashBytesFnv1a64(bytes));
            std::ostringstream oss;
            oss << "{\"animation_hash\":\"" << animationHash << "\""
                << ",\"bone_count\":" << context.skeleton.boneCount()
                << "}";
            const std::string json = oss.str();
            std::vector<uint8_t> outBytes(json.begin(), json.end());
            if (!writeBytesToFile(target, outBytes, messages)) {
                return false;
            }
            messages.push_back("animation bundle exported hash=" + animationHash
                + " bytes=" + std::to_string(outBytes.size()));
            return true;
        });

    registry.registerCommand("animation.verify_bundle",
        [](ScriptContext& context, const ScriptCommand& command, std::vector<std::string>& messages) {
            if (!context.hasSkeleton) {
                messages.push_back("animation.verify_bundle requires a skeleton");
                return false;
            }
            const auto pathArg = getArg(command, "path");
            if (!pathArg) {
                messages.push_back("animation.verify_bundle requires path=");
                return false;
            }
            const std::filesystem::path source = ScriptBatchHarness::normalizePath(context.workingDirectory, *pathArg);
            std::vector<uint8_t> input;
            if (!readBytesFromFile(source, input, messages)) {
                return false;
            }
            const std::string text(input.begin(), input.end());
            const auto expectedHash = extractJsonStringField(text, "animation_hash");
            if (!expectedHash) {
                messages.push_back("animation.verify_bundle missing animation_hash in: " + makePathString(source));
                return false;
            }
            const auto currBytes = serializeAnimationState(context.skeleton, context.pose);
            const std::string actualHash = hashHex(hashBytesFnv1a64(currBytes));
            if (*expectedHash != actualHash) {
                messages.push_back("animation.verify_bundle mismatch expected=" + *expectedHash
                    + " actual=" + actualHash);
                return false;
            }
            messages.push_back("animation.verify_bundle match: " + actualHash);
            return true;
        });

    registry.registerCommand("animation.describe",
        [](ScriptContext& context, const ScriptCommand&, std::vector<std::string>& messages) {
            if (!context.hasSkeleton) {
                messages.push_back("animation.describe requires a skeleton");
                return false;
            }
            messages.push_back("animation bones=" + std::to_string(context.skeleton.boneCount()));
            return true;
        });

    registry.registerCommand("animation.list_bones",
        [](ScriptContext& context, const ScriptCommand&, std::vector<std::string>& messages) {
            if (!context.hasSkeleton) {
                messages.push_back("animation.list_bones requires a skeleton");
                return false;
            }
            for (size_t i = 0; i < context.skeleton.boneCount(); ++i) {
                const auto& bl = context.skeleton.bindLocal(i);
                messages.push_back("bone index=" + std::to_string(i)
                    + " name=" + context.skeleton.boneName(i)
                    + " parent=" + std::to_string(context.skeleton.parentIndex(i))
                    + " tx=" + formatScalar(bl.translation.x)
                    + " ty=" + formatScalar(bl.translation.y)
                    + " tz=" + formatScalar(bl.translation.z));
            }
            return true;
        });

    registry.registerCommand("animation.has_bone",
        [](ScriptContext& context, const ScriptCommand& command, std::vector<std::string>& messages) {
            if (!context.hasSkeleton) {
                messages.push_back("animation.has_bone requires a skeleton");
                return false;
            }
            const auto nameArg = getArg(command, "name");
            if (!nameArg || nameArg->empty()) {
                messages.push_back("animation.has_bone requires name=");
                return false;
            }
            const int32_t idx = context.skeleton.findBoneIndexByName(*nameArg);
            if (idx < 0) {
                messages.push_back("animation.has_bone not found: " + *nameArg);
                return false;
            }
            messages.push_back("animation.has_bone exists: " + *nameArg + " index=" + std::to_string(idx));
            return true;
        });
}

void registerSimRigidCommands(ScriptRegistry& registry)
{
    registry.registerCommand("sim.rigid.create",
        [](ScriptContext& context, const ScriptCommand& command, std::vector<std::string>& messages) {
            context.rigidSolver  = std::make_unique<nexus::RigidBodySolver>();
            context.hasRigidSolver = true;
            context.hasRigidLastState = false;
            const float gx = parseFloatArg(command, "gx").value_or(0.f);
            const float gy = parseFloatArg(command, "gy").value_or(-9.81f);
            const float gz = parseFloatArg(command, "gz").value_or(0.f);
            context.rigidSolver->setGravity({gx, gy, gz});
            messages.push_back("rigid solver created");
            return true;
        });

    registry.registerCommand("sim.rigid.apply_force",
        [](ScriptContext& context, const ScriptCommand& command, std::vector<std::string>& messages) {
            if (!context.hasRigidSolver) {
                messages.push_back("sim.rigid.apply_force requires sim.rigid.create first");
                return false;
            }
            const auto id = parseIntArg(command, "id");
            if (!id || *id <= 0) {
                messages.push_back("sim.rigid.apply_force requires id=>0");
                return false;
            }
            const nexus::SimVec3 force{
                parseFloatArg(command, "fx").value_or(0.f),
                parseFloatArg(command, "fy").value_or(0.f),
                parseFloatArg(command, "fz").value_or(0.f),
            };
            if (!context.rigidSolver->applyForce(static_cast<nexus::BodyId>(*id), force)) {
                messages.push_back("sim.rigid.apply_force failed");
                return false;
            }
            messages.push_back("rigid force applied id=" + std::to_string(*id));
            return true;
        });

    registry.registerCommand("sim.rigid.add_body",
        [](ScriptContext& context, const ScriptCommand& command, std::vector<std::string>& messages) {
            if (!context.hasRigidSolver) {
                messages.push_back("sim.rigid.add_body requires sim.rigid.create first");
                return false;
            }
            nexus::SimBodyDesc desc{};
            desc.mass        = parseFloatArg(command, "mass").value_or(1.f);
            desc.position.x  = parseFloatArg(command, "tx").value_or(0.f);
            desc.position.y  = parseFloatArg(command, "ty").value_or(0.f);
            desc.position.z  = parseFloatArg(command, "tz").value_or(0.f);
            desc.velocity.x  = parseFloatArg(command, "vx").value_or(0.f);
            desc.velocity.y  = parseFloatArg(command, "vy").value_or(0.f);
            desc.velocity.z  = parseFloatArg(command, "vz").value_or(0.f);
            const nexus::BodyId id = context.rigidSolver->addBody(desc);
            messages.push_back("rigid body added id=" + std::to_string(id)
                + " mass=" + formatScalar(desc.mass));
            return true;
        });

    registry.registerCommand("sim.rigid.remove_body",
        [](ScriptContext& context, const ScriptCommand& command, std::vector<std::string>& messages) {
            if (!context.hasRigidSolver) {
                messages.push_back("sim.rigid.remove_body requires sim.rigid.create first");
                return false;
            }
            const auto idArg = parseIntArg(command, "id");
            if (!idArg || *idArg <= 0) {
                messages.push_back("sim.rigid.remove_body requires valid id=");
                return false;
            }
            const nexus::BodyId id = static_cast<nexus::BodyId>(*idArg);
            if (!context.rigidSolver->removeBody(id)) {
                messages.push_back("sim.rigid.remove_body not found id=" + std::to_string(id));
                return false;
            }
            messages.push_back("rigid body removed id=" + std::to_string(id));
            return true;
        });

    registry.registerCommand("sim.rigid.step",
        [](ScriptContext& context, const ScriptCommand& command, std::vector<std::string>& messages) {
            if (!context.hasRigidSolver) {
                messages.push_back("sim.rigid.step requires sim.rigid.create first");
                return false;
            }
            const double dt = static_cast<double>(parseFloatArg(command, "dt").value_or(0.016f));
            const nexus::StepReport report = context.rigidSolver->step(dt);
            if (!report.ok) {
                messages.push_back("sim.rigid.step failed (invalid dt)");
                return false;
            }
            messages.push_back("rigid step t=" + formatScalar(report.simulationTime)
                + " bodies=" + std::to_string(report.bodiesIntegrated));
            return true;
        });

    registry.registerCommand("sim.rigid.capture_state",
        [](ScriptContext& context, const ScriptCommand&, std::vector<std::string>& messages) {
            if (!context.hasRigidSolver) {
                messages.push_back("sim.rigid.capture_state requires sim.rigid.create first");
                return false;
            }
            context.rigidLastState = context.rigidSolver->captureState();
            context.hasRigidLastState = true;
            messages.push_back("rigid state captured bodies="
                + std::to_string(context.rigidLastState.bodies.size()));
            return true;
        });

    registry.registerCommand("sim.rigid.set_baseline",
        [](ScriptContext& context, const ScriptCommand&, std::vector<std::string>& messages) {
            if (!context.hasRigidSolver) {
                messages.push_back("sim.rigid.set_baseline requires sim.rigid.create first");
                return false;
            }
            context.rigidLastState = context.rigidSolver->captureState();
            context.hasRigidLastState = true;
            messages.push_back("rigid baseline set bodies="
                + std::to_string(context.rigidLastState.bodies.size()));
            return true;
        });

    registry.registerCommand("sim.rigid.state_hash",
        [](ScriptContext& context, const ScriptCommand&, std::vector<std::string>& messages) {
            if (!context.hasRigidSolver) {
                messages.push_back("sim.rigid.state_hash requires sim.rigid.create first");
                return false;
            }
            const nexus::SimState current = context.rigidSolver->captureState();
            const std::vector<uint8_t> bytes = serializeSimState(current);
            messages.push_back("rigid hash=" + hashHex(hashBytesFnv1a64(bytes))
                + " bytes=" + std::to_string(bytes.size()));
            return true;
        });

    registry.registerCommand("sim.rigid.state_summary",
        [](ScriptContext& context, const ScriptCommand&, std::vector<std::string>& messages) {
            if (!context.hasRigidSolver) {
                messages.push_back("sim.rigid.state_summary requires sim.rigid.create first");
                return false;
            }
            const nexus::SimState current = context.rigidSolver->captureState();
            const std::vector<uint8_t> bytes = serializeSimState(current);
            messages.push_back("rigid summary bodies=" + std::to_string(current.bodies.size())
                + " time=" + formatScalar(current.simulationTime)
                + " bytes=" + std::to_string(bytes.size())
                + " hash=" + hashHex(hashBytesFnv1a64(bytes)));
            return true;
        });

    registry.registerCommand("sim.rigid.describe",
        [](ScriptContext& context, const ScriptCommand&, std::vector<std::string>& messages) {
            if (!context.hasRigidSolver) {
                messages.push_back("sim.rigid.describe requires sim.rigid.create first");
                return false;
            }
            messages.push_back("rigid describe bodies=" + std::to_string(context.rigidSolver->bodyCount())
                + " gravity=" + formatScalar(context.rigidSolver->gravity().x)
                + "," + formatScalar(context.rigidSolver->gravity().y)
                + "," + formatScalar(context.rigidSolver->gravity().z)
                + " time=" + formatScalar(context.rigidSolver->simulationTime()));
            return true;
        });

    registry.registerCommand("sim.rigid.list_bodies",
        [](ScriptContext& context, const ScriptCommand&, std::vector<std::string>& messages) {
            if (!context.hasRigidSolver) {
                messages.push_back("sim.rigid.list_bodies requires sim.rigid.create first");
                return false;
            }
            const nexus::SimState state = context.rigidSolver->captureState();
            for (const auto& body : state.bodies) {
                messages.push_back("rigid body id=" + std::to_string(body.id)
                    + " px=" + formatScalar(body.position.x)
                    + " py=" + formatScalar(body.position.y)
                    + " pz=" + formatScalar(body.position.z)
                    + " vx=" + formatScalar(body.velocity.x)
                    + " vy=" + formatScalar(body.velocity.y)
                    + " vz=" + formatScalar(body.velocity.z));
            }
            return true;
        });

    registry.registerCommand("sim.rigid.has_body",
        [](ScriptContext& context, const ScriptCommand& command, std::vector<std::string>& messages) {
            if (!context.hasRigidSolver) {
                messages.push_back("sim.rigid.has_body requires sim.rigid.create first");
                return false;
            }
            const auto idArg = parseIntArg(command, "id");
            if (!idArg || *idArg <= 0) {
                messages.push_back("sim.rigid.has_body requires valid id=");
                return false;
            }
            const nexus::BodyId id = static_cast<nexus::BodyId>(*idArg);
            if (!context.rigidSolver->hasBody(id)) {
                messages.push_back("sim.rigid.has_body not found id=" + std::to_string(id));
                return false;
            }
            messages.push_back("sim.rigid.has_body exists id=" + std::to_string(id));
            return true;
        });

    registry.registerCommand("sim.rigid.get_body",
        [](ScriptContext& context, const ScriptCommand& command, std::vector<std::string>& messages) {
            if (!context.hasRigidSolver) {
                messages.push_back("sim.rigid.get_body requires sim.rigid.create first");
                return false;
            }
            const auto idArg = parseIntArg(command, "id");
            if (!idArg || *idArg <= 0) {
                messages.push_back("sim.rigid.get_body requires valid id=");
                return false;
            }
            const nexus::BodyId id = static_cast<nexus::BodyId>(*idArg);
            const nexus::SimState state = context.rigidSolver->captureState();
            for (const auto& body : state.bodies) {
                if (body.id == id) {
                    messages.push_back("rigid body id=" + std::to_string(body.id)
                        + " px=" + formatScalar(body.position.x)
                        + " py=" + formatScalar(body.position.y)
                        + " pz=" + formatScalar(body.position.z)
                        + " vx=" + formatScalar(body.velocity.x)
                        + " vy=" + formatScalar(body.velocity.y)
                        + " vz=" + formatScalar(body.velocity.z));
                    return true;
                }
            }
            messages.push_back("sim.rigid.get_body not found id=" + std::to_string(id));
            return false;
        });

    registry.registerCommand("sim.rigid.expect_hash",
        [](ScriptContext& context, const ScriptCommand& command, std::vector<std::string>& messages) {
            if (!context.hasRigidSolver) {
                messages.push_back("sim.rigid.expect_hash requires sim.rigid.create first");
                return false;
            }
            const auto expected = parseExpectedHashArg(command);
            if (!expected) {
                messages.push_back("sim.rigid.expect_hash requires hash=<uint64|0xHEX>");
                return false;
            }
            const nexus::SimState current = context.rigidSolver->captureState();
            const std::vector<uint8_t> bytes = serializeSimState(current);
            const uint64_t actual = hashBytesFnv1a64(bytes);
            if (actual != *expected) {
                messages.push_back("sim.rigid.expect_hash mismatch expected="
                    + hashHex(*expected) + " actual=" + hashHex(actual));
                return false;
            }
            messages.push_back("sim.rigid.expect_hash match " + hashHex(actual));
            return true;
        });

    registry.registerCommand("sim.rigid.diff_state",
        [](ScriptContext& context, const ScriptCommand&, std::vector<std::string>& messages) {
            if (!context.hasRigidSolver) {
                messages.push_back("sim.rigid.diff_state requires sim.rigid.create first");
                return false;
            }
            if (!context.hasRigidLastState) {
                messages.push_back("sim.rigid.diff_state requires sim.rigid.capture_state, sim.rigid.export_state, or sim.rigid.import_state first");
                return false;
            }
            const nexus::SimState current = context.rigidSolver->captureState();
            const std::vector<uint8_t> currBytes = serializeSimState(current);
            const std::vector<uint8_t> baseBytes = serializeSimState(context.rigidLastState);
            const size_t changed = byteDiffCount(baseBytes, currBytes);
            const size_t firstDiff = firstByteDiff(baseBytes, currBytes);
            messages.push_back("rigid diff equal=" + std::string(changed == 0 ? "1" : "0")
                + " changed=" + std::to_string(changed)
                + " first_diff=" + std::string(firstDiff == static_cast<size_t>(-1) ? "-1" : std::to_string(firstDiff))
                + " base_hash=" + hashHex(hashBytesFnv1a64(baseBytes))
                + " curr_hash=" + hashHex(hashBytesFnv1a64(currBytes)));
            return true;
        });

    registry.registerCommand("sim.rigid.restore_state",
        [](ScriptContext& context, const ScriptCommand&, std::vector<std::string>& messages) {
            if (!context.hasRigidSolver) {
                messages.push_back("sim.rigid.restore_state requires sim.rigid.create first");
                return false;
            }
            if (!context.hasRigidLastState) {
                messages.push_back("sim.rigid.restore_state requires sim.rigid.capture_state, sim.rigid.export_state, or sim.rigid.import_state first");
                return false;
            }
            if (!context.rigidSolver->restoreState(context.rigidLastState)) {
                messages.push_back("sim.rigid.restore_state failed");
                return false;
            }
            messages.push_back("rigid state restored");
            return true;
        });

    registry.registerCommand("sim.rigid.export_state",
        [](ScriptContext& context, const ScriptCommand& command, std::vector<std::string>& messages) {
            if (!context.hasRigidSolver) {
                messages.push_back("sim.rigid.export_state requires sim.rigid.create first");
                return false;
            }
            const auto pathArg = getArg(command, "path");
            if (!pathArg) {
                messages.push_back("sim.rigid.export_state requires path=");
                return false;
            }
            context.rigidLastState = context.rigidSolver->captureState();
            context.hasRigidLastState = true;
            const std::vector<uint8_t> bytes = serializeSimState(context.rigidLastState);
            const std::filesystem::path target = ScriptBatchHarness::normalizePath(context.workingDirectory, *pathArg);
            if (!writeBytesToFile(target, bytes, messages)) {
                return false;
            }
            messages.push_back("rigid state exported bytes=" + std::to_string(bytes.size()));
            return true;
        });

    registry.registerCommand("sim.rigid.import_state",
        [](ScriptContext& context, const ScriptCommand& command, std::vector<std::string>& messages) {
            if (!context.hasRigidSolver) {
                messages.push_back("sim.rigid.import_state requires sim.rigid.create first");
                return false;
            }
            const auto pathArg = getArg(command, "path");
            if (!pathArg) {
                messages.push_back("sim.rigid.import_state requires path=");
                return false;
            }
            std::vector<uint8_t> bytes;
            const std::filesystem::path target = ScriptBatchHarness::normalizePath(context.workingDirectory, *pathArg);
            if (!readBytesFromFile(target, bytes, messages)) {
                return false;
            }
            nexus::SimState state;
            if (!deserializeSimState(bytes, state)) {
                messages.push_back("sim.rigid.import_state deserialize failed");
                return false;
            }
            if (!context.rigidSolver->restoreState(state)) {
                messages.push_back("sim.rigid.import_state restore failed");
                return false;
            }
            context.rigidLastState = std::move(state);
            context.hasRigidLastState = true;
            messages.push_back("rigid state imported bodies="
                + std::to_string(context.rigidLastState.bodies.size()));
            return true;
        });
}

void registerSimClothCommands(ScriptRegistry& registry)
{
    registry.registerCommand("sim.cloth.create",
        [](ScriptContext& context, const ScriptCommand& command, std::vector<std::string>& messages) {
            context.clothSolver  = std::make_unique<nexus::ClothSolver>();
            context.hasClothSolver = true;
            context.hasClothLastState = false;
            const float gx = parseFloatArg(command, "gx").value_or(0.f);
            const float gy = parseFloatArg(command, "gy").value_or(-9.81f);
            const float gz = parseFloatArg(command, "gz").value_or(0.f);
            context.clothSolver->setGravity({gx, gy, gz});
            messages.push_back("cloth solver created");
            return true;
        });

    registry.registerCommand("sim.cloth.add_edge",
        [](ScriptContext& context, const ScriptCommand& command, std::vector<std::string>& messages) {
            if (!context.hasClothSolver) {
                messages.push_back("sim.cloth.add_edge requires sim.cloth.create first");
                return false;
            }
            const auto a = parseIntArg(command, "a");
            const auto b = parseIntArg(command, "b");
            if (!a || !b || *a <= 0 || *b <= 0) {
                messages.push_back("sim.cloth.add_edge requires a=>0 and b=>0");
                return false;
            }
            const float rest = parseFloatArg(command, "rest").value_or(1.0f);
            const float stiff = parseFloatArg(command, "stiff").value_or(100.0f);
            if (!context.clothSolver->addEdge(static_cast<nexus::ClothNodeId>(*a),
                                              static_cast<nexus::ClothNodeId>(*b),
                                              rest,
                                              stiff)) {
                messages.push_back("sim.cloth.add_edge failed");
                return false;
            }
            messages.push_back("cloth edge added a=" + std::to_string(*a) + " b=" + std::to_string(*b));
            return true;
        });

    registry.registerCommand("sim.cloth.add_node",
        [](ScriptContext& context, const ScriptCommand& command, std::vector<std::string>& messages) {
            if (!context.hasClothSolver) {
                messages.push_back("sim.cloth.add_node requires sim.cloth.create first");
                return false;
            }
            nexus::ClothNodeDesc desc{};
            desc.mass        = parseFloatArg(command, "mass").value_or(1.f);
            desc.position.x  = parseFloatArg(command, "tx").value_or(0.f);
            desc.position.y  = parseFloatArg(command, "ty").value_or(0.f);
            desc.position.z  = parseFloatArg(command, "tz").value_or(0.f);
            const nexus::ClothNodeId id = context.clothSolver->addNode(desc);
            messages.push_back("cloth node added id=" + std::to_string(id)
                + " mass=" + formatScalar(desc.mass));
            return true;
        });

    registry.registerCommand("sim.cloth.remove_node",
        [](ScriptContext& context, const ScriptCommand& command, std::vector<std::string>& messages) {
            if (!context.hasClothSolver) {
                messages.push_back("sim.cloth.remove_node requires sim.cloth.create first");
                return false;
            }
            const auto idArg = parseIntArg(command, "id");
            if (!idArg || *idArg <= 0) {
                messages.push_back("sim.cloth.remove_node requires valid id=");
                return false;
            }
            const nexus::ClothNodeId id = static_cast<nexus::ClothNodeId>(*idArg);
            if (!context.clothSolver->removeNode(id)) {
                messages.push_back("sim.cloth.remove_node not found id=" + std::to_string(id));
                return false;
            }
            messages.push_back("cloth node removed id=" + std::to_string(id));
            return true;
        });

    registry.registerCommand("sim.cloth.step",
        [](ScriptContext& context, const ScriptCommand& command, std::vector<std::string>& messages) {
            if (!context.hasClothSolver) {
                messages.push_back("sim.cloth.step requires sim.cloth.create first");
                return false;
            }
            const double dt = static_cast<double>(parseFloatArg(command, "dt").value_or(0.016f));
            const nexus::ClothStepReport report = context.clothSolver->step(dt);
            if (!report.ok) {
                messages.push_back("sim.cloth.step failed (invalid dt)");
                return false;
            }
            messages.push_back("cloth step t=" + formatScalar(report.simulationTime)
                + " nodes=" + std::to_string(report.nodesIntegrated));
            return true;
        });

    registry.registerCommand("sim.cloth.capture_state",
        [](ScriptContext& context, const ScriptCommand&, std::vector<std::string>& messages) {
            if (!context.hasClothSolver) {
                messages.push_back("sim.cloth.capture_state requires sim.cloth.create first");
                return false;
            }
            context.clothLastState = context.clothSolver->captureState();
            context.hasClothLastState = true;
            messages.push_back("cloth state captured nodes="
                + std::to_string(context.clothLastState.nodes.size()));
            return true;
        });

    registry.registerCommand("sim.cloth.set_baseline",
        [](ScriptContext& context, const ScriptCommand&, std::vector<std::string>& messages) {
            if (!context.hasClothSolver) {
                messages.push_back("sim.cloth.set_baseline requires sim.cloth.create first");
                return false;
            }
            context.clothLastState = context.clothSolver->captureState();
            context.hasClothLastState = true;
            messages.push_back("cloth baseline set nodes="
                + std::to_string(context.clothLastState.nodes.size()));
            return true;
        });

    registry.registerCommand("sim.cloth.state_hash",
        [](ScriptContext& context, const ScriptCommand&, std::vector<std::string>& messages) {
            if (!context.hasClothSolver) {
                messages.push_back("sim.cloth.state_hash requires sim.cloth.create first");
                return false;
            }
            const nexus::ClothState current = context.clothSolver->captureState();
            const std::vector<uint8_t> bytes = serializeClothState(current);
            messages.push_back("cloth hash=" + hashHex(hashBytesFnv1a64(bytes))
                + " bytes=" + std::to_string(bytes.size()));
            return true;
        });

    registry.registerCommand("sim.cloth.state_summary",
        [](ScriptContext& context, const ScriptCommand&, std::vector<std::string>& messages) {
            if (!context.hasClothSolver) {
                messages.push_back("sim.cloth.state_summary requires sim.cloth.create first");
                return false;
            }
            const nexus::ClothState current = context.clothSolver->captureState();
            const std::vector<uint8_t> bytes = serializeClothState(current);
            messages.push_back("cloth summary nodes=" + std::to_string(current.nodes.size())
                + " time=" + formatScalar(current.simulationTime)
                + " bytes=" + std::to_string(bytes.size())
                + " hash=" + hashHex(hashBytesFnv1a64(bytes)));
            return true;
        });

    registry.registerCommand("sim.cloth.describe",
        [](ScriptContext& context, const ScriptCommand&, std::vector<std::string>& messages) {
            if (!context.hasClothSolver) {
                messages.push_back("sim.cloth.describe requires sim.cloth.create first");
                return false;
            }
            const auto gravity = context.clothSolver->gravity();
            messages.push_back("cloth describe nodes=" + std::to_string(context.clothSolver->nodeCount())
                + " gravity=" + formatScalar(gravity.x)
                + "," + formatScalar(gravity.y)
                + "," + formatScalar(gravity.z)
                + " time=" + formatScalar(context.clothSolver->captureState().simulationTime));
            return true;
        });

    registry.registerCommand("sim.cloth.list_nodes",
        [](ScriptContext& context, const ScriptCommand&, std::vector<std::string>& messages) {
            if (!context.hasClothSolver) {
                messages.push_back("sim.cloth.list_nodes requires sim.cloth.create first");
                return false;
            }
            const nexus::ClothState current = context.clothSolver->captureState();
            for (const auto& node : current.nodes) {
                messages.push_back("cloth node id=" + std::to_string(node.id)
                    + " px=" + formatScalar(node.position.x)
                    + " py=" + formatScalar(node.position.y)
                    + " pz=" + formatScalar(node.position.z)
                    + " vx=" + formatScalar(node.velocity.x)
                    + " vy=" + formatScalar(node.velocity.y)
                    + " vz=" + formatScalar(node.velocity.z));
            }
            return true;
        });

    registry.registerCommand("sim.cloth.has_node",
        [](ScriptContext& context, const ScriptCommand& command, std::vector<std::string>& messages) {
            if (!context.hasClothSolver) {
                messages.push_back("sim.cloth.has_node requires sim.cloth.create first");
                return false;
            }
            const auto idArg = parseIntArg(command, "id");
            if (!idArg || *idArg <= 0) {
                messages.push_back("sim.cloth.has_node requires valid id=");
                return false;
            }
            const nexus::ClothNodeId id = static_cast<nexus::ClothNodeId>(*idArg);
            if (!context.clothSolver->hasNode(id)) {
                messages.push_back("sim.cloth.has_node not found id=" + std::to_string(id));
                return false;
            }
            messages.push_back("sim.cloth.has_node exists id=" + std::to_string(id));
            return true;
        });

    registry.registerCommand("sim.cloth.get_node",
        [](ScriptContext& context, const ScriptCommand& command, std::vector<std::string>& messages) {
            if (!context.hasClothSolver) {
                messages.push_back("sim.cloth.get_node requires sim.cloth.create first");
                return false;
            }
            const auto idArg = parseIntArg(command, "id");
            if (!idArg || *idArg <= 0) {
                messages.push_back("sim.cloth.get_node requires valid id=");
                return false;
            }
            const nexus::ClothNodeId id = static_cast<nexus::ClothNodeId>(*idArg);
            const nexus::ClothState current = context.clothSolver->captureState();
            for (const auto& node : current.nodes) {
                if (node.id == id) {
                    messages.push_back("cloth node id=" + std::to_string(node.id)
                        + " px=" + formatScalar(node.position.x)
                        + " py=" + formatScalar(node.position.y)
                        + " pz=" + formatScalar(node.position.z)
                        + " vx=" + formatScalar(node.velocity.x)
                        + " vy=" + formatScalar(node.velocity.y)
                        + " vz=" + formatScalar(node.velocity.z));
                    return true;
                }
            }
            messages.push_back("sim.cloth.get_node not found id=" + std::to_string(id));
            return false;
        });

    registry.registerCommand("sim.cloth.expect_hash",
        [](ScriptContext& context, const ScriptCommand& command, std::vector<std::string>& messages) {
            if (!context.hasClothSolver) {
                messages.push_back("sim.cloth.expect_hash requires sim.cloth.create first");
                return false;
            }
            const auto expected = parseExpectedHashArg(command);
            if (!expected) {
                messages.push_back("sim.cloth.expect_hash requires hash=<uint64|0xHEX>");
                return false;
            }
            const nexus::ClothState current = context.clothSolver->captureState();
            const std::vector<uint8_t> bytes = serializeClothState(current);
            const uint64_t actual = hashBytesFnv1a64(bytes);
            if (actual != *expected) {
                messages.push_back("sim.cloth.expect_hash mismatch expected="
                    + hashHex(*expected) + " actual=" + hashHex(actual));
                return false;
            }
            messages.push_back("sim.cloth.expect_hash match " + hashHex(actual));
            return true;
        });

    registry.registerCommand("sim.cloth.diff_state",
        [](ScriptContext& context, const ScriptCommand&, std::vector<std::string>& messages) {
            if (!context.hasClothSolver) {
                messages.push_back("sim.cloth.diff_state requires sim.cloth.create first");
                return false;
            }
            if (!context.hasClothLastState) {
                messages.push_back("sim.cloth.diff_state requires sim.cloth.capture_state, sim.cloth.export_state, or sim.cloth.import_state first");
                return false;
            }
            const nexus::ClothState current = context.clothSolver->captureState();
            const std::vector<uint8_t> currBytes = serializeClothState(current);
            const std::vector<uint8_t> baseBytes = serializeClothState(context.clothLastState);
            const size_t changed = byteDiffCount(baseBytes, currBytes);
            const size_t firstDiff = firstByteDiff(baseBytes, currBytes);
            messages.push_back("cloth diff equal=" + std::string(changed == 0 ? "1" : "0")
                + " changed=" + std::to_string(changed)
                + " first_diff=" + std::string(firstDiff == static_cast<size_t>(-1) ? "-1" : std::to_string(firstDiff))
                + " base_hash=" + hashHex(hashBytesFnv1a64(baseBytes))
                + " curr_hash=" + hashHex(hashBytesFnv1a64(currBytes)));
            return true;
        });

    registry.registerCommand("sim.cloth.restore_state",
        [](ScriptContext& context, const ScriptCommand&, std::vector<std::string>& messages) {
            if (!context.hasClothSolver) {
                messages.push_back("sim.cloth.restore_state requires sim.cloth.create first");
                return false;
            }
            if (!context.hasClothLastState) {
                messages.push_back("sim.cloth.restore_state requires sim.cloth.capture_state, sim.cloth.export_state, or sim.cloth.import_state first");
                return false;
            }
            if (!context.clothSolver->restoreState(context.clothLastState)) {
                messages.push_back("sim.cloth.restore_state failed");
                return false;
            }
            messages.push_back("cloth state restored");
            return true;
        });

    registry.registerCommand("sim.cloth.export_state",
        [](ScriptContext& context, const ScriptCommand& command, std::vector<std::string>& messages) {
            if (!context.hasClothSolver) {
                messages.push_back("sim.cloth.export_state requires sim.cloth.create first");
                return false;
            }
            const auto pathArg = getArg(command, "path");
            if (!pathArg) {
                messages.push_back("sim.cloth.export_state requires path=");
                return false;
            }
            context.clothLastState = context.clothSolver->captureState();
            context.hasClothLastState = true;
            const std::vector<uint8_t> bytes = serializeClothState(context.clothLastState);
            const std::filesystem::path target = ScriptBatchHarness::normalizePath(context.workingDirectory, *pathArg);
            if (!writeBytesToFile(target, bytes, messages)) {
                return false;
            }
            messages.push_back("cloth state exported bytes=" + std::to_string(bytes.size()));
            return true;
        });

    registry.registerCommand("sim.cloth.import_state",
        [](ScriptContext& context, const ScriptCommand& command, std::vector<std::string>& messages) {
            if (!context.hasClothSolver) {
                messages.push_back("sim.cloth.import_state requires sim.cloth.create first");
                return false;
            }
            const auto pathArg = getArg(command, "path");
            if (!pathArg) {
                messages.push_back("sim.cloth.import_state requires path=");
                return false;
            }
            std::vector<uint8_t> bytes;
            const std::filesystem::path target = ScriptBatchHarness::normalizePath(context.workingDirectory, *pathArg);
            if (!readBytesFromFile(target, bytes, messages)) {
                return false;
            }
            nexus::ClothState state;
            if (!deserializeClothState(bytes, state)) {
                messages.push_back("sim.cloth.import_state deserialize failed");
                return false;
            }
            if (!context.clothSolver->restoreState(state)) {
                messages.push_back("sim.cloth.import_state restore failed");
                return false;
            }
            context.clothLastState = std::move(state);
            context.hasClothLastState = true;
            messages.push_back("cloth state imported nodes="
                + std::to_string(context.clothLastState.nodes.size()));
            return true;
        });
}

void registerSimFluidCommands(ScriptRegistry& registry)
{
    registry.registerCommand("sim.fluid.create",
        [](ScriptContext& context, const ScriptCommand& command, std::vector<std::string>& messages) {
            context.fluidSolver  = std::make_unique<nexus::FluidSolver>();
            context.hasFluidSolver = true;
            context.hasFluidLastState = false;
            const float gx = parseFloatArg(command, "gx").value_or(0.f);
            const float gy = parseFloatArg(command, "gy").value_or(-9.81f);
            const float gz = parseFloatArg(command, "gz").value_or(0.f);
            const float h = parseFloatArg(command, "h").value_or(0.1f);
            const float k = parseFloatArg(command, "k").value_or(200.0f);
            context.fluidSolver->setGravity({gx, gy, gz});
            context.fluidSolver->setSmoothingRadius(h);
            context.fluidSolver->setPressureStiffness(k);
            messages.push_back("fluid solver created");
            return true;
        });

    registry.registerCommand("sim.fluid.add_particle",
        [](ScriptContext& context, const ScriptCommand& command, std::vector<std::string>& messages) {
            if (!context.hasFluidSolver) {
                messages.push_back("sim.fluid.add_particle requires sim.fluid.create first");
                return false;
            }
            nexus::FluidParticleDesc desc{};
            desc.mass        = parseFloatArg(command, "mass").value_or(1.f);
            desc.position.x  = parseFloatArg(command, "tx").value_or(0.f);
            desc.position.y  = parseFloatArg(command, "ty").value_or(0.f);
            desc.position.z  = parseFloatArg(command, "tz").value_or(0.f);
            desc.velocity.x  = parseFloatArg(command, "vx").value_or(0.f);
            desc.velocity.y  = parseFloatArg(command, "vy").value_or(0.f);
            desc.velocity.z  = parseFloatArg(command, "vz").value_or(0.f);
            desc.density     = parseFloatArg(command, "density").value_or(1000.0f);
            const nexus::FluidParticleId id = context.fluidSolver->addParticle(desc);
            messages.push_back("fluid particle added id=" + std::to_string(id)
                + " mass=" + formatScalar(desc.mass));
            return true;
        });

    registry.registerCommand("sim.fluid.remove_particle",
        [](ScriptContext& context, const ScriptCommand& command, std::vector<std::string>& messages) {
            if (!context.hasFluidSolver) {
                messages.push_back("sim.fluid.remove_particle requires sim.fluid.create first");
                return false;
            }
            const auto idArg = parseIntArg(command, "id");
            if (!idArg || *idArg <= 0) {
                messages.push_back("sim.fluid.remove_particle requires valid id=");
                return false;
            }
            const nexus::FluidParticleId id = static_cast<nexus::FluidParticleId>(*idArg);
            if (!context.fluidSolver->removeParticle(id)) {
                messages.push_back("sim.fluid.remove_particle not found id=" + std::to_string(id));
                return false;
            }
            messages.push_back("fluid particle removed id=" + std::to_string(id));
            return true;
        });

    registry.registerCommand("sim.fluid.step",
        [](ScriptContext& context, const ScriptCommand& command, std::vector<std::string>& messages) {
            if (!context.hasFluidSolver) {
                messages.push_back("sim.fluid.step requires sim.fluid.create first");
                return false;
            }
            const double dt = static_cast<double>(parseFloatArg(command, "dt").value_or(0.016f));
            const nexus::FluidStepReport report = context.fluidSolver->step(dt);
            if (!report.ok) {
                messages.push_back("sim.fluid.step failed (invalid dt)");
                return false;
            }
            messages.push_back("fluid step t=" + formatScalar(report.simulationTime)
                + " particles=" + std::to_string(report.particlesAdvanced));
            return true;
        });

    registry.registerCommand("sim.fluid.capture_state",
        [](ScriptContext& context, const ScriptCommand&, std::vector<std::string>& messages) {
            if (!context.hasFluidSolver) {
                messages.push_back("sim.fluid.capture_state requires sim.fluid.create first");
                return false;
            }
            context.fluidLastState = context.fluidSolver->captureState();
            context.hasFluidLastState = true;
            messages.push_back("fluid state captured particles="
                + std::to_string(context.fluidLastState.particles.size()));
            return true;
        });

    registry.registerCommand("sim.fluid.set_baseline",
        [](ScriptContext& context, const ScriptCommand&, std::vector<std::string>& messages) {
            if (!context.hasFluidSolver) {
                messages.push_back("sim.fluid.set_baseline requires sim.fluid.create first");
                return false;
            }
            context.fluidLastState = context.fluidSolver->captureState();
            context.hasFluidLastState = true;
            messages.push_back("fluid baseline set particles="
                + std::to_string(context.fluidLastState.particles.size()));
            return true;
        });

    registry.registerCommand("sim.fluid.state_hash",
        [](ScriptContext& context, const ScriptCommand&, std::vector<std::string>& messages) {
            if (!context.hasFluidSolver) {
                messages.push_back("sim.fluid.state_hash requires sim.fluid.create first");
                return false;
            }
            const nexus::FluidState current = context.fluidSolver->captureState();
            const std::vector<uint8_t> bytes = serializeFluidState(current);
            messages.push_back("fluid hash=" + hashHex(hashBytesFnv1a64(bytes))
                + " bytes=" + std::to_string(bytes.size()));
            return true;
        });

    registry.registerCommand("sim.fluid.state_summary",
        [](ScriptContext& context, const ScriptCommand&, std::vector<std::string>& messages) {
            if (!context.hasFluidSolver) {
                messages.push_back("sim.fluid.state_summary requires sim.fluid.create first");
                return false;
            }
            const nexus::FluidState current = context.fluidSolver->captureState();
            const std::vector<uint8_t> bytes = serializeFluidState(current);
            messages.push_back("fluid summary particles=" + std::to_string(current.particles.size())
                + " time=" + formatScalar(current.simulationTime)
                + " bytes=" + std::to_string(bytes.size())
                + " hash=" + hashHex(hashBytesFnv1a64(bytes)));
            return true;
        });

    registry.registerCommand("sim.fluid.describe",
        [](ScriptContext& context, const ScriptCommand&, std::vector<std::string>& messages) {
            if (!context.hasFluidSolver) {
                messages.push_back("sim.fluid.describe requires sim.fluid.create first");
                return false;
            }
            const auto gravity = context.fluidSolver->gravity();
            messages.push_back("fluid describe particles=" + std::to_string(context.fluidSolver->particleCount())
                + " gravity=" + formatScalar(gravity.x)
                + "," + formatScalar(gravity.y)
                + "," + formatScalar(gravity.z)
                + " h=" + formatScalar(context.fluidSolver->smoothingRadius())
                + " k=" + formatScalar(context.fluidSolver->pressureStiffness())
                + " time=" + formatScalar(context.fluidSolver->captureState().simulationTime));
            return true;
        });

    registry.registerCommand("sim.fluid.list_particles",
        [](ScriptContext& context, const ScriptCommand&, std::vector<std::string>& messages) {
            if (!context.hasFluidSolver) {
                messages.push_back("sim.fluid.list_particles requires sim.fluid.create first");
                return false;
            }
            const nexus::FluidState current = context.fluidSolver->captureState();
            for (const auto& p : current.particles) {
                messages.push_back("fluid particle id=" + std::to_string(p.id)
                    + " px=" + formatScalar(p.position.x)
                    + " py=" + formatScalar(p.position.y)
                    + " pz=" + formatScalar(p.position.z)
                    + " vx=" + formatScalar(p.velocity.x)
                    + " vy=" + formatScalar(p.velocity.y)
                    + " vz=" + formatScalar(p.velocity.z)
                    + " density=" + formatScalar(p.density));
            }
            return true;
        });

    registry.registerCommand("sim.fluid.has_particle",
        [](ScriptContext& context, const ScriptCommand& command, std::vector<std::string>& messages) {
            if (!context.hasFluidSolver) {
                messages.push_back("sim.fluid.has_particle requires sim.fluid.create first");
                return false;
            }
            const auto idArg = parseIntArg(command, "id");
            if (!idArg || *idArg <= 0) {
                messages.push_back("sim.fluid.has_particle requires valid id=");
                return false;
            }
            const nexus::FluidParticleId id = static_cast<nexus::FluidParticleId>(*idArg);
            if (!context.fluidSolver->hasParticle(id)) {
                messages.push_back("sim.fluid.has_particle not found id=" + std::to_string(id));
                return false;
            }
            messages.push_back("sim.fluid.has_particle exists id=" + std::to_string(id));
            return true;
        });

    registry.registerCommand("sim.fluid.get_particle",
        [](ScriptContext& context, const ScriptCommand& command, std::vector<std::string>& messages) {
            if (!context.hasFluidSolver) {
                messages.push_back("sim.fluid.get_particle requires sim.fluid.create first");
                return false;
            }
            const auto idArg = parseIntArg(command, "id");
            if (!idArg || *idArg <= 0) {
                messages.push_back("sim.fluid.get_particle requires valid id=");
                return false;
            }
            const nexus::FluidParticleId id = static_cast<nexus::FluidParticleId>(*idArg);
            const nexus::FluidState current = context.fluidSolver->captureState();
            for (const auto& p : current.particles) {
                if (p.id == id) {
                    messages.push_back("fluid particle id=" + std::to_string(p.id)
                        + " px=" + formatScalar(p.position.x)
                        + " py=" + formatScalar(p.position.y)
                        + " pz=" + formatScalar(p.position.z)
                        + " vx=" + formatScalar(p.velocity.x)
                        + " vy=" + formatScalar(p.velocity.y)
                        + " vz=" + formatScalar(p.velocity.z)
                        + " density=" + formatScalar(p.density));
                    return true;
                }
            }
            messages.push_back("sim.fluid.get_particle not found id=" + std::to_string(id));
            return false;
        });

    registry.registerCommand("sim.fluid.expect_hash",
        [](ScriptContext& context, const ScriptCommand& command, std::vector<std::string>& messages) {
            if (!context.hasFluidSolver) {
                messages.push_back("sim.fluid.expect_hash requires sim.fluid.create first");
                return false;
            }
            const auto expected = parseExpectedHashArg(command);
            if (!expected) {
                messages.push_back("sim.fluid.expect_hash requires hash=<uint64|0xHEX>");
                return false;
            }
            const nexus::FluidState current = context.fluidSolver->captureState();
            const std::vector<uint8_t> bytes = serializeFluidState(current);
            const uint64_t actual = hashBytesFnv1a64(bytes);
            if (actual != *expected) {
                messages.push_back("sim.fluid.expect_hash mismatch expected="
                    + hashHex(*expected) + " actual=" + hashHex(actual));
                return false;
            }
            messages.push_back("sim.fluid.expect_hash match " + hashHex(actual));
            return true;
        });

    registry.registerCommand("sim.fluid.diff_state",
        [](ScriptContext& context, const ScriptCommand&, std::vector<std::string>& messages) {
            if (!context.hasFluidSolver) {
                messages.push_back("sim.fluid.diff_state requires sim.fluid.create first");
                return false;
            }
            if (!context.hasFluidLastState) {
                messages.push_back("sim.fluid.diff_state requires sim.fluid.capture_state, sim.fluid.export_state, or sim.fluid.import_state first");
                return false;
            }
            const nexus::FluidState current = context.fluidSolver->captureState();
            const std::vector<uint8_t> currBytes = serializeFluidState(current);
            const std::vector<uint8_t> baseBytes = serializeFluidState(context.fluidLastState);
            const size_t changed = byteDiffCount(baseBytes, currBytes);
            const size_t firstDiff = firstByteDiff(baseBytes, currBytes);
            messages.push_back("fluid diff equal=" + std::string(changed == 0 ? "1" : "0")
                + " changed=" + std::to_string(changed)
                + " first_diff=" + std::string(firstDiff == static_cast<size_t>(-1) ? "-1" : std::to_string(firstDiff))
                + " base_hash=" + hashHex(hashBytesFnv1a64(baseBytes))
                + " curr_hash=" + hashHex(hashBytesFnv1a64(currBytes)));
            return true;
        });

    registry.registerCommand("sim.fluid.restore_state",
        [](ScriptContext& context, const ScriptCommand&, std::vector<std::string>& messages) {
            if (!context.hasFluidSolver) {
                messages.push_back("sim.fluid.restore_state requires sim.fluid.create first");
                return false;
            }
            if (!context.hasFluidLastState) {
                messages.push_back("sim.fluid.restore_state requires sim.fluid.capture_state, sim.fluid.export_state, or sim.fluid.import_state first");
                return false;
            }
            if (!context.fluidSolver->restoreState(context.fluidLastState)) {
                messages.push_back("sim.fluid.restore_state failed");
                return false;
            }
            messages.push_back("fluid state restored");
            return true;
        });

    registry.registerCommand("sim.fluid.export_state",
        [](ScriptContext& context, const ScriptCommand& command, std::vector<std::string>& messages) {
            if (!context.hasFluidSolver) {
                messages.push_back("sim.fluid.export_state requires sim.fluid.create first");
                return false;
            }
            const auto pathArg = getArg(command, "path");
            if (!pathArg) {
                messages.push_back("sim.fluid.export_state requires path=");
                return false;
            }
            context.fluidLastState = context.fluidSolver->captureState();
            context.hasFluidLastState = true;
            const std::vector<uint8_t> bytes = serializeFluidState(context.fluidLastState);
            const std::filesystem::path target = ScriptBatchHarness::normalizePath(context.workingDirectory, *pathArg);
            if (!writeBytesToFile(target, bytes, messages)) {
                return false;
            }
            messages.push_back("fluid state exported bytes=" + std::to_string(bytes.size()));
            return true;
        });

    registry.registerCommand("sim.fluid.import_state",
        [](ScriptContext& context, const ScriptCommand& command, std::vector<std::string>& messages) {
            if (!context.hasFluidSolver) {
                messages.push_back("sim.fluid.import_state requires sim.fluid.create first");
                return false;
            }
            const auto pathArg = getArg(command, "path");
            if (!pathArg) {
                messages.push_back("sim.fluid.import_state requires path=");
                return false;
            }
            std::vector<uint8_t> bytes;
            const std::filesystem::path target = ScriptBatchHarness::normalizePath(context.workingDirectory, *pathArg);
            if (!readBytesFromFile(target, bytes, messages)) {
                return false;
            }
            nexus::FluidState state;
            if (!deserializeFluidState(bytes, state)) {
                messages.push_back("sim.fluid.import_state deserialize failed");
                return false;
            }
            if (!context.fluidSolver->restoreState(state)) {
                messages.push_back("sim.fluid.import_state restore failed");
                return false;
            }
            context.fluidLastState = std::move(state);
            context.hasFluidLastState = true;
            messages.push_back("fluid state imported particles="
                + std::to_string(context.fluidLastState.particles.size()));
            return true;
        });
}

void registerGaussianCommands(ScriptRegistry& registry)
{
    using namespace nexus::gfx;

    registry.registerCommand("gaussian.load_ply",
        [](ScriptContext& context, const ScriptCommand& command, std::vector<std::string>& messages) {
            const auto pathArg = getArg(command, "path");
            if (!pathArg) {
                messages.push_back("gaussian.load_ply requires path=");
                return false;
            }
            const std::filesystem::path target = ScriptBatchHarness::normalizePath(context.workingDirectory, *pathArg);
            auto cloud = nexus::gfx::GaussianSplatCloud::loadFromPLY(target.string());
            if (!cloud) {
                messages.push_back("gaussian.load_ply failed: " + target.string());
                return false;
            }
            context.gaussianCloud    = std::move(*cloud);
            context.hasGaussianCloud = true;
            messages.push_back("gaussian cloud loaded splats="
                + std::to_string(context.gaussianCloud.splatCount()));
            return true;
        });

    registry.registerCommand("gaussian.describe",
        [](ScriptContext& context, const ScriptCommand&, std::vector<std::string>& messages) {
            if (!context.hasGaussianCloud) {
                messages.push_back("gaussian.describe requires gaussian.load_ply first");
                return false;
            }
            messages.push_back("gaussian splats=" + std::to_string(context.gaussianCloud.splatCount()));
            messages.push_back("gaussian format=" + context.gaussianCloud.sourceFormat);
            messages.push_back("gaussian sh_degree=" + std::to_string(context.gaussianCloud.shDegree));
            return true;
        });

    registry.registerCommand("gaussian.hash",
        [](ScriptContext& context, const ScriptCommand&, std::vector<std::string>& messages) {
            if (!context.hasGaussianCloud) {
                messages.push_back("gaussian.hash requires gaussian.load_ply first");
                return false;
            }
            const auto bytes = serializeGaussianState(context.gaussianCloud);
            messages.push_back("gaussian hash=" + hashHex(hashBytesFnv1a64(bytes))
                + " splats=" + std::to_string(context.gaussianCloud.splatCount())
                + " bytes=" + std::to_string(bytes.size()));
            return true;
        });

    registry.registerCommand("gaussian.set_baseline",
        [](ScriptContext& context, const ScriptCommand&, std::vector<std::string>& messages) {
            if (!context.hasGaussianCloud) {
                messages.push_back("gaussian.set_baseline requires gaussian.load_ply first");
                return false;
            }
            context.gaussianBaselineBytes = serializeGaussianState(context.gaussianCloud);
            context.hasGaussianBaseline = true;
            messages.push_back("gaussian baseline set splats="
                + std::to_string(context.gaussianCloud.splatCount()));
            return true;
        });

    registry.registerCommand("gaussian.diff",
        [](ScriptContext& context, const ScriptCommand&, std::vector<std::string>& messages) {
            if (!context.hasGaussianCloud) {
                messages.push_back("gaussian.diff requires gaussian.load_ply first");
                return false;
            }
            if (!context.hasGaussianBaseline) {
                messages.push_back("gaussian.diff requires gaussian.set_baseline first");
                return false;
            }
            const auto currBytes = serializeGaussianState(context.gaussianCloud);
            const auto& baseBytes = context.gaussianBaselineBytes;
            const size_t changed = byteDiffCount(baseBytes, currBytes);
            const size_t firstDiff = firstByteDiff(baseBytes, currBytes);
            messages.push_back("gaussian diff equal=" + std::string(changed == 0 ? "1" : "0")
                + " changed=" + std::to_string(changed)
                + " first_diff=" + std::string(firstDiff == static_cast<size_t>(-1) ? "-1" : std::to_string(firstDiff))
                + " base_hash=" + hashHex(hashBytesFnv1a64(baseBytes))
                + " curr_hash=" + hashHex(hashBytesFnv1a64(currBytes)));
            return true;
        });

    registry.registerCommand("gaussian.expect_hash",
        [](ScriptContext& context, const ScriptCommand& command, std::vector<std::string>& messages) {
            if (!context.hasGaussianCloud) {
                messages.push_back("gaussian.expect_hash requires gaussian.load_ply first");
                return false;
            }
            const auto expected = parseExpectedHashArg(command);
            if (!expected) {
                messages.push_back("gaussian.expect_hash requires hash=<uint64|0xHEX>");
                return false;
            }
            const auto bytes = serializeGaussianState(context.gaussianCloud);
            const uint64_t actual = hashBytesFnv1a64(bytes);
            if (actual != *expected) {
                messages.push_back("gaussian.expect_hash mismatch expected="
                    + hashHex(*expected) + " actual=" + hashHex(actual));
                return false;
            }
            messages.push_back("gaussian.expect_hash match " + hashHex(actual));
            return true;
        });

    registry.registerCommand("gaussian.export_bundle",
        [](ScriptContext& context, const ScriptCommand& command, std::vector<std::string>& messages) {
            if (!context.hasGaussianCloud) {
                messages.push_back("gaussian.export_bundle requires gaussian.load_ply first");
                return false;
            }
            const auto pathArg = getArg(command, "path");
            if (!pathArg) {
                messages.push_back("gaussian.export_bundle requires path=");
                return false;
            }
            const std::filesystem::path target = ScriptBatchHarness::normalizePath(context.workingDirectory, *pathArg);
            const auto bytes = serializeGaussianState(context.gaussianCloud);
            const std::string gaussianHash = hashHex(hashBytesFnv1a64(bytes));
            std::ostringstream oss;
            oss << "{\"gaussian_hash\":\"" << gaussianHash << "\""
                << ",\"splat_count\":" << context.gaussianCloud.splatCount()
                << ",\"sh_degree\":" << context.gaussianCloud.shDegree
                << "}";
            const std::string json = oss.str();
            std::vector<uint8_t> outBytes(json.begin(), json.end());
            if (!writeBytesToFile(target, outBytes, messages)) {
                return false;
            }
            messages.push_back("gaussian bundle exported hash=" + gaussianHash
                + " bytes=" + std::to_string(outBytes.size()));
            return true;
        });

    registry.registerCommand("gaussian.verify_bundle",
        [](ScriptContext& context, const ScriptCommand& command, std::vector<std::string>& messages) {
            if (!context.hasGaussianCloud) {
                messages.push_back("gaussian.verify_bundle requires gaussian.load_ply first");
                return false;
            }
            const auto pathArg = getArg(command, "path");
            if (!pathArg) {
                messages.push_back("gaussian.verify_bundle requires path=");
                return false;
            }
            const std::filesystem::path source = ScriptBatchHarness::normalizePath(context.workingDirectory, *pathArg);
            std::vector<uint8_t> input;
            if (!readBytesFromFile(source, input, messages)) {
                return false;
            }
            const std::string text(input.begin(), input.end());
            const auto expectedHash = extractJsonStringField(text, "gaussian_hash");
            if (!expectedHash) {
                messages.push_back("gaussian.verify_bundle missing gaussian_hash in: " + makePathString(source));
                return false;
            }
            const auto currBytes = serializeGaussianState(context.gaussianCloud);
            const std::string actualHash = hashHex(hashBytesFnv1a64(currBytes));
            if (*expectedHash != actualHash) {
                messages.push_back("gaussian.verify_bundle mismatch expected=" + *expectedHash
                    + " actual=" + actualHash);
                return false;
            }
            messages.push_back("gaussian.verify_bundle match: " + actualHash);
            return true;
        });
}

void registerCrossSolverCommands(ScriptRegistry& registry)
{
    registry.registerCommand("sim.cross_solver_hash",
        [](ScriptContext& context, const ScriptCommand&, std::vector<std::string>& messages) {
            const auto bytes = serializeCrossSolverState(context);
            messages.push_back("cross_solver hash=" + hashHex(hashBytesFnv1a64(bytes))
                + " rigid=" + (context.hasRigidSolver ? "1" : "0")
                + " cloth=" + (context.hasClothSolver ? "1" : "0")
                + " fluid=" + (context.hasFluidSolver ? "1" : "0"));
            return true;
        });

    registry.registerCommand("sim.cross_solver.describe",
        [](ScriptContext& context, const ScriptCommand&, std::vector<std::string>& messages) {
            const auto combinedBytes = serializeCrossSolverState(context);
            const std::string combinedHash = hashHex(hashBytesFnv1a64(combinedBytes));

            std::string rigidHash = "inactive";
            size_t rigidBodies = 0;
            double rigidTime = 0.0;
            if (context.hasRigidSolver) {
                const auto st = context.rigidSolver->captureState();
                rigidHash = hashHex(hashBytesFnv1a64(serializeSimState(st)));
                rigidBodies = st.bodies.size();
                rigidTime = st.simulationTime;
            }

            std::string clothHash = "inactive";
            size_t clothNodes = 0;
            double clothTime = 0.0;
            if (context.hasClothSolver) {
                const auto st = context.clothSolver->captureState();
                clothHash = hashHex(hashBytesFnv1a64(serializeClothState(st)));
                clothNodes = st.nodes.size();
                clothTime = st.simulationTime;
            }

            std::string fluidHash = "inactive";
            size_t fluidParticles = 0;
            double fluidTime = 0.0;
            if (context.hasFluidSolver) {
                const auto st = context.fluidSolver->captureState();
                fluidHash = hashHex(hashBytesFnv1a64(serializeFluidState(st)));
                fluidParticles = st.particles.size();
                fluidTime = st.simulationTime;
            }

            messages.push_back("cross_solver describe hash=" + combinedHash
                + " rigid_active=" + std::string(context.hasRigidSolver ? "1" : "0")
                + " rigid_bodies=" + std::to_string(rigidBodies)
                + " rigid_time=" + formatScalar(rigidTime)
                + " rigid_hash=" + rigidHash
                + " cloth_active=" + std::string(context.hasClothSolver ? "1" : "0")
                + " cloth_nodes=" + std::to_string(clothNodes)
                + " cloth_time=" + formatScalar(clothTime)
                + " cloth_hash=" + clothHash
                + " fluid_active=" + std::string(context.hasFluidSolver ? "1" : "0")
                + " fluid_particles=" + std::to_string(fluidParticles)
                + " fluid_time=" + formatScalar(fluidTime)
                + " fluid_hash=" + fluidHash);
            return true;
        });

    registry.registerCommand("sim.cross_solver.set_baseline",
        [](ScriptContext& context, const ScriptCommand&, std::vector<std::string>& messages) {
            context.crossSolverBaselineBytes = serializeCrossSolverState(context);
            context.hasCrossSolverBaseline = true;
            messages.push_back("cross_solver baseline set hash="
                + hashHex(hashBytesFnv1a64(context.crossSolverBaselineBytes)));
            return true;
        });

    registry.registerCommand("sim.cross_solver.diff_state",
        [](ScriptContext& context, const ScriptCommand&, std::vector<std::string>& messages) {
            if (!context.hasCrossSolverBaseline) {
                messages.push_back("sim.cross_solver.diff_state requires sim.cross_solver.set_baseline first");
                return false;
            }
            const std::vector<uint8_t> currBytes = serializeCrossSolverState(context);
            const auto& baseBytes = context.crossSolverBaselineBytes;
            const size_t changed = byteDiffCount(baseBytes, currBytes);
            const size_t firstDiff = firstByteDiff(baseBytes, currBytes);
            messages.push_back("cross_solver diff equal=" + std::string(changed == 0 ? "1" : "0")
                + " changed=" + std::to_string(changed)
                + " first_diff="
                + std::string(firstDiff == static_cast<size_t>(-1) ? "-1" : std::to_string(firstDiff))
                + " base_hash=" + hashHex(hashBytesFnv1a64(baseBytes))
                + " curr_hash=" + hashHex(hashBytesFnv1a64(currBytes)));
            return true;
        });

    registry.registerCommand("sim.cross_solver.expect_hash",
        [](ScriptContext& context, const ScriptCommand& command, std::vector<std::string>& messages) {
            const auto expected = parseExpectedHashArg(command);
            if (!expected) {
                messages.push_back("sim.cross_solver.expect_hash requires hash=<uint64|0xHEX>");
                return false;
            }
            const std::vector<uint8_t> currBytes = serializeCrossSolverState(context);
            const uint64_t actual = hashBytesFnv1a64(currBytes);
            if (actual != *expected) {
                messages.push_back("sim.cross_solver.expect_hash mismatch expected="
                    + hashHex(*expected) + " actual=" + hashHex(actual));
                return false;
            }
            messages.push_back("sim.cross_solver.expect_hash match " + hashHex(actual));
            return true;
        });

    registry.registerCommand("sim.export_cross_solver_bundle",
        [](ScriptContext& context, const ScriptCommand& command, std::vector<std::string>& messages) {
            const auto pathArg = getArg(command, "path");
            if (!pathArg) {
                messages.push_back("sim.export_cross_solver_bundle requires path=");
                return false;
            }
            const std::filesystem::path target = ScriptBatchHarness::normalizePath(context.workingDirectory, *pathArg);
            const std::string json = makeCrossSolverBundleJson(context);
            std::vector<uint8_t> outBytes(json.begin(), json.end());
            if (!writeBytesToFile(target, outBytes, messages)) {
                return false;
            }
            messages.push_back("cross_solver bundle exported bytes=" + std::to_string(outBytes.size()));
            return true;
        });

    registry.registerCommand("sim.verify_cross_solver_bundle",
        [](ScriptContext& context, const ScriptCommand& command, std::vector<std::string>& messages) {
            const auto pathArg = getArg(command, "path");
            if (!pathArg) {
                messages.push_back("sim.verify_cross_solver_bundle requires path=");
                return false;
            }
            const std::filesystem::path source = ScriptBatchHarness::normalizePath(context.workingDirectory, *pathArg);
            std::vector<uint8_t> input;
            if (!readBytesFromFile(source, input, messages)) {
                return false;
            }
            const std::string text(input.begin(), input.end());
            const auto expectedHash = extractJsonStringField(text, "cross_solver_hash");
            if (!expectedHash) {
                messages.push_back("sim.verify_cross_solver_bundle missing cross_solver_hash in: "
                    + makePathString(source));
                return false;
            }
            const auto currBytes = serializeCrossSolverState(context);
            const std::string actualHash = hashHex(hashBytesFnv1a64(currBytes));
            if (*expectedHash != actualHash) {
                messages.push_back("sim.verify_cross_solver_bundle mismatch expected=" + *expectedHash
                    + " actual=" + actualHash);
                return false;
            }
            messages.push_back("sim.verify_cross_solver_bundle match: " + actualHash);
            return true;
        });
}

void registerParametricCommands(ScriptRegistry& registry)
{
    using namespace nexus::parametric;

    registry.registerCommand("parametric.new",
        [](ScriptContext& context, const ScriptCommand&, std::vector<std::string>& messages) {
            context.parametricGraph    = nexus::parametric::ConstraintGraph{};
            context.hasParametricGraph = true;
            messages.push_back("parametric graph created");
            return true;
        });

    registry.registerCommand("parametric.add_point",
        [](ScriptContext& context, const ScriptCommand& command, std::vector<std::string>& messages) {
            if (!context.hasParametricGraph) {
                messages.push_back("parametric.add_point requires parametric.new first");
                return false;
            }
            const double x = parseDoubleArg(command, "x").value_or(0.0);
            const double y = parseDoubleArg(command, "y").value_or(0.0);
            const double z = parseDoubleArg(command, "z").value_or(0.0);
            const auto id = context.parametricGraph.addPoint({x, y, z});
            messages.push_back("parametric entity id=" + std::to_string(id)
                + " x=" + formatScalar(x)
                + " y=" + formatScalar(y)
                + " z=" + formatScalar(z));
            return true;
        });

    registry.registerCommand("parametric.remove_entity",
        [](ScriptContext& context, const ScriptCommand& command, std::vector<std::string>& messages) {
            if (!context.hasParametricGraph) {
                messages.push_back("parametric.remove_entity requires parametric.new first");
                return false;
            }
            const auto idArg = parseIntArg(command, "id");
            if (!idArg || *idArg <= 0) {
                messages.push_back("parametric.remove_entity requires valid id=");
                return false;
            }
            const auto entityId = static_cast<nexus::parametric::ParametricEntityId>(*idArg);
            if (!context.parametricGraph.removeEntity(entityId)) {
                messages.push_back("parametric.remove_entity not found id=" + std::to_string(entityId));
                return false;
            }
            messages.push_back("parametric entity removed id=" + std::to_string(entityId));
            return true;
        });

    registry.registerCommand("parametric.has_entity",
        [](ScriptContext& context, const ScriptCommand& command, std::vector<std::string>& messages) {
            if (!context.hasParametricGraph) {
                messages.push_back("parametric.has_entity requires parametric.new first");
                return false;
            }
            const auto idArg = parseIntArg(command, "id");
            if (!idArg || *idArg <= 0) {
                messages.push_back("parametric.has_entity requires valid id=");
                return false;
            }
            const auto entityId = static_cast<nexus::parametric::ParametricEntityId>(*idArg);
            const bool exists = context.parametricGraph.hasEntity(entityId);
            messages.push_back("parametric entity id=" + std::to_string(entityId)
                + " exists=" + std::string(exists ? "1" : "0"));
            return exists;
        });

    registry.registerCommand("parametric.set_point",
        [](ScriptContext& context, const ScriptCommand& command, std::vector<std::string>& messages) {
            if (!context.hasParametricGraph) {
                messages.push_back("parametric.set_point requires parametric.new first");
                return false;
            }
            const auto idArg = parseIntArg(command, "id");
            if (!idArg || *idArg <= 0) {
                messages.push_back("parametric.set_point requires valid id=");
                return false;
            }
            const double x = parseDoubleArg(command, "x").value_or(0.0);
            const double y = parseDoubleArg(command, "y").value_or(0.0);
            const double z = parseDoubleArg(command, "z").value_or(0.0);
            const auto entityId = static_cast<nexus::parametric::ParametricEntityId>(*idArg);
            if (!context.parametricGraph.setPoint(entityId, {x, y, z})) {
                messages.push_back("parametric.set_point not found id=" + std::to_string(entityId));
                return false;
            }
            messages.push_back("parametric point updated id=" + std::to_string(entityId)
                + " x=" + formatScalar(x)
                + " y=" + formatScalar(y)
                + " z=" + formatScalar(z));
            return true;
        });

    registry.registerCommand("parametric.add_distance_constraint",
        [](ScriptContext& context, const ScriptCommand& command, std::vector<std::string>& messages) {
            if (!context.hasParametricGraph) {
                messages.push_back("parametric.add_distance_constraint requires parametric.new first");
                return false;
            }
            const auto aArg    = parseIntArg(command, "a");
            const auto bArg    = parseIntArg(command, "b");
            const auto distArg = parseDoubleArg(command, "dist");
            if (!aArg || !bArg || !distArg) {
                messages.push_back("parametric.add_distance_constraint requires a= b= dist=");
                return false;
            }
            const auto idA = static_cast<nexus::parametric::ParametricEntityId>(*aArg);
            const auto idB = static_cast<nexus::parametric::ParametricEntityId>(*bArg);
            const auto cid = context.parametricGraph.addDistanceConstraint(idA, idB, *distArg);
            if (cid == nexus::parametric::kInvalidConstraintId) {
                messages.push_back("parametric.add_distance_constraint failed: invalid entity ids");
                return false;
            }
            messages.push_back("parametric distance constraint id=" + std::to_string(cid)
                + " a=" + std::to_string(idA) + " b=" + std::to_string(idB)
                + " dist=" + formatScalar(*distArg));
            return true;
        });

    registry.registerCommand("parametric.add_coincident_constraint",
        [](ScriptContext& context, const ScriptCommand& command, std::vector<std::string>& messages) {
            if (!context.hasParametricGraph) {
                messages.push_back("parametric.add_coincident_constraint requires parametric.new first");
                return false;
            }
            const auto aArg = parseIntArg(command, "a");
            const auto bArg = parseIntArg(command, "b");
            if (!aArg || !bArg) {
                messages.push_back("parametric.add_coincident_constraint requires a= b=");
                return false;
            }
            const auto idA = static_cast<nexus::parametric::ParametricEntityId>(*aArg);
            const auto idB = static_cast<nexus::parametric::ParametricEntityId>(*bArg);
            const auto cid = context.parametricGraph.addCoincidentConstraint(idA, idB);
            if (cid == nexus::parametric::kInvalidConstraintId) {
                messages.push_back("parametric.add_coincident_constraint failed: invalid entity ids");
                return false;
            }
            messages.push_back("parametric coincident constraint id=" + std::to_string(cid)
                + " a=" + std::to_string(idA) + " b=" + std::to_string(idB));
            return true;
        });

    registry.registerCommand("parametric.add_axis_aligned_distance_constraint",
        [](ScriptContext& context, const ScriptCommand& command, std::vector<std::string>& messages) {
            if (!context.hasParametricGraph) {
                messages.push_back("parametric.add_axis_aligned_distance_constraint requires parametric.new first");
                return false;
            }
            const auto aArg = parseIntArg(command, "a");
            const auto bArg = parseIntArg(command, "b");
            const auto distArg = parseDoubleArg(command, "dist");
            const auto axisArg = getArg(command, "axis");
            if (!aArg || !bArg || !distArg || !axisArg || axisArg->empty()) {
                messages.push_back("parametric.add_axis_aligned_distance_constraint requires a= b= axis= dist=");
                return false;
            }

            nexus::parametric::Axis axis = nexus::parametric::Axis::X;
            if (*axisArg == "x" || *axisArg == "X") {
                axis = nexus::parametric::Axis::X;
            } else if (*axisArg == "y" || *axisArg == "Y") {
                axis = nexus::parametric::Axis::Y;
            } else if (*axisArg == "z" || *axisArg == "Z") {
                axis = nexus::parametric::Axis::Z;
            } else {
                messages.push_back("parametric.add_axis_aligned_distance_constraint invalid axis (use x|y|z)");
                return false;
            }

            const auto idA = static_cast<nexus::parametric::ParametricEntityId>(*aArg);
            const auto idB = static_cast<nexus::parametric::ParametricEntityId>(*bArg);
            const auto cid = context.parametricGraph.addAxisAlignedDistanceConstraint(idA, idB, axis, *distArg);
            if (cid == nexus::parametric::kInvalidConstraintId) {
                messages.push_back("parametric.add_axis_aligned_distance_constraint failed: invalid entity ids");
                return false;
            }
            messages.push_back("parametric axis constraint id=" + std::to_string(cid)
                + " a=" + std::to_string(idA)
                + " b=" + std::to_string(idB)
                + " axis=" + *axisArg
                + " dist=" + formatScalar(*distArg));
            return true;
        });

    registry.registerCommand("parametric.remove_constraint",
        [](ScriptContext& context, const ScriptCommand& command, std::vector<std::string>& messages) {
            if (!context.hasParametricGraph) {
                messages.push_back("parametric.remove_constraint requires parametric.new first");
                return false;
            }
            const auto idArg = parseIntArg(command, "id");
            if (!idArg || *idArg <= 0) {
                messages.push_back("parametric.remove_constraint requires valid id=");
                return false;
            }
            const auto constraintId = static_cast<nexus::parametric::ParametricConstraintId>(*idArg);
            if (!context.parametricGraph.removeConstraint(constraintId)) {
                messages.push_back("parametric.remove_constraint not found id=" + std::to_string(constraintId));
                return false;
            }
            messages.push_back("parametric constraint removed id=" + std::to_string(constraintId));
            return true;
        });

    registry.registerCommand("parametric.has_constraint",
        [](ScriptContext& context, const ScriptCommand& command, std::vector<std::string>& messages) {
            if (!context.hasParametricGraph) {
                messages.push_back("parametric.has_constraint requires parametric.new first");
                return false;
            }
            const auto idArg = parseIntArg(command, "id");
            if (!idArg || *idArg <= 0) {
                messages.push_back("parametric.has_constraint requires valid id=");
                return false;
            }
            const auto constraintId = static_cast<nexus::parametric::ParametricConstraintId>(*idArg);
            const bool exists = context.parametricGraph.hasConstraint(constraintId);
            messages.push_back("parametric constraint id=" + std::to_string(constraintId)
                + " exists=" + std::string(exists ? "1" : "0"));
            return exists;
        });

    registry.registerCommand("parametric.solve",
        [](ScriptContext& context, const ScriptCommand& command, std::vector<std::string>& messages) {
            if (!context.hasParametricGraph) {
                messages.push_back("parametric.solve requires parametric.new first");
                return false;
            }
            nexus::parametric::ParametricSolverConfig cfg;
            if (const auto maxIter = parseIntArg(command, "max_iterations"))
                cfg.maxIterations = static_cast<uint32_t>(std::max(1, *maxIter));
            if (const auto epsilonText = getArg(command, "convergence_epsilon")) {
                const auto epsilon = parseDoubleArg(command, "convergence_epsilon");
                if (!epsilon || *epsilon <= 0.0) {
                    messages.push_back("parametric.solve requires valid convergence_epsilon=");
                    return false;
                }
                cfg.convergenceEpsilon = *epsilon;
            }
            const auto report = nexus::parametric::ParametricSolver::solve(context.parametricGraph, cfg);
            messages.push_back("parametric solved converged=" + std::string(report.converged ? "1" : "0")
                + " iterations=" + std::to_string(report.iterationsRan)
                + " error=" + formatScalar(report.maxConstraintError));
            if (!report.converged) {
                for (const auto& err : report.errors)
                    messages.push_back("parametric solver error: " + err);
            }
            return report.converged;
        });

    registry.registerCommand("parametric.get_point",
        [](ScriptContext& context, const ScriptCommand& command, std::vector<std::string>& messages) {
            if (!context.hasParametricGraph) {
                messages.push_back("parametric.get_point requires parametric.new first");
                return false;
            }
            const auto idArg = parseIntArg(command, "id");
            if (!idArg || *idArg <= 0) {
                messages.push_back("parametric.get_point requires valid id=");
                return false;
            }
            const auto entityId = static_cast<nexus::parametric::ParametricEntityId>(*idArg);
            const auto* pt = context.parametricGraph.point(entityId);
            if (!pt) {
                messages.push_back("parametric.get_point not found id=" + std::to_string(entityId));
                return false;
            }
            messages.push_back("parametric point x=" + formatScalar(pt->x)
                + " y=" + formatScalar(pt->y)
                + " z=" + formatScalar(pt->z));
            return true;
        });

    registry.registerCommand("parametric.describe",
        [](ScriptContext& context, const ScriptCommand&, std::vector<std::string>& messages) {
            if (!context.hasParametricGraph) {
                messages.push_back("parametric.describe requires parametric.new first");
                return false;
            }
            messages.push_back(
                "parametric entities=" + std::to_string(context.parametricGraph.entityCount())
                + " distance_constraints=" + std::to_string(context.parametricGraph.distanceConstraintCount())
                + " coincident_constraints=" + std::to_string(context.parametricGraph.coincidentConstraintCount())
                + " axis_constraints=" + std::to_string(context.parametricGraph.axisAlignedDistanceConstraintCount()));
            return true;
        });

    registry.registerCommand("parametric.list_entities",
        [](ScriptContext& context, const ScriptCommand&, std::vector<std::string>& messages) {
            if (!context.hasParametricGraph) {
                messages.push_back("parametric.list_entities requires parametric.new first");
                return false;
            }
            for (const auto& entity : context.parametricGraph.entities()) {
                messages.push_back("parametric entity id=" + std::to_string(entity.id)
                    + " x=" + formatScalar(entity.point.x)
                    + " y=" + formatScalar(entity.point.y)
                    + " z=" + formatScalar(entity.point.z));
            }
            return true;
        });

    registry.registerCommand("parametric.list_constraints",
        [](ScriptContext& context, const ScriptCommand&, std::vector<std::string>& messages) {
            if (!context.hasParametricGraph) {
                messages.push_back("parametric.list_constraints requires parametric.new first");
                return false;
            }
            for (const auto& c : context.parametricGraph.distanceConstraints()) {
                messages.push_back("parametric constraint id=" + std::to_string(c.id)
                    + " type=distance a=" + std::to_string(c.entityA)
                    + " b=" + std::to_string(c.entityB)
                    + " dist=" + formatScalar(c.targetDistance));
            }
            for (const auto& c : context.parametricGraph.coincidentConstraints()) {
                messages.push_back("parametric constraint id=" + std::to_string(c.id)
                    + " type=coincident a=" + std::to_string(c.entityA)
                    + " b=" + std::to_string(c.entityB));
            }
            for (const auto& c : context.parametricGraph.axisAlignedDistanceConstraints()) {
                const std::string axisStr = c.axis == nexus::parametric::Axis::X ? "x"
                                          : c.axis == nexus::parametric::Axis::Y ? "y" : "z";
                messages.push_back("parametric constraint id=" + std::to_string(c.id)
                    + " type=axis_aligned a=" + std::to_string(c.entityA)
                    + " b=" + std::to_string(c.entityB)
                    + " axis=" + axisStr
                    + " dist=" + formatScalar(c.targetDistance));
            }
            return true;
        });

    registry.registerCommand("parametric.get_constraint",
        [](ScriptContext& context, const ScriptCommand& command, std::vector<std::string>& messages) {
            if (!context.hasParametricGraph) {
                messages.push_back("parametric.get_constraint requires parametric.new first");
                return false;
            }
            const auto idArg = parseIntArg(command, "id");
            if (!idArg || *idArg <= 0) {
                messages.push_back("parametric.get_constraint requires valid id=");
                return false;
            }
            const auto cid = static_cast<nexus::parametric::ParametricConstraintId>(*idArg);
            for (const auto& c : context.parametricGraph.distanceConstraints()) {
                if (c.id == cid) {
                    messages.push_back("parametric constraint id=" + std::to_string(c.id)
                        + " type=distance a=" + std::to_string(c.entityA)
                        + " b=" + std::to_string(c.entityB)
                        + " dist=" + formatScalar(c.targetDistance));
                    return true;
                }
            }
            for (const auto& c : context.parametricGraph.coincidentConstraints()) {
                if (c.id == cid) {
                    messages.push_back("parametric constraint id=" + std::to_string(c.id)
                        + " type=coincident a=" + std::to_string(c.entityA)
                        + " b=" + std::to_string(c.entityB));
                    return true;
                }
            }
            for (const auto& c : context.parametricGraph.axisAlignedDistanceConstraints()) {
                if (c.id == cid) {
                    const std::string axisStr = c.axis == nexus::parametric::Axis::X ? "x"
                                              : c.axis == nexus::parametric::Axis::Y ? "y" : "z";
                    messages.push_back("parametric constraint id=" + std::to_string(c.id)
                        + " type=axis_aligned a=" + std::to_string(c.entityA)
                        + " b=" + std::to_string(c.entityB)
                        + " axis=" + axisStr
                        + " dist=" + formatScalar(c.targetDistance));
                    return true;
                }
            }
            messages.push_back("parametric.get_constraint not found id=" + std::to_string(cid));
            return false;
        });

    registry.registerCommand("parametric.hash",
        [](ScriptContext& context, const ScriptCommand&, std::vector<std::string>& messages) {
            if (!context.hasParametricGraph) {
                messages.push_back("parametric.hash requires parametric.new first");
                return false;
            }
            const auto bytes = serializeParametricState(context.parametricGraph);
            messages.push_back("parametric hash=" + hashHex(hashBytesFnv1a64(bytes))
                + " entities=" + std::to_string(context.parametricGraph.entityCount())
                + " bytes=" + std::to_string(bytes.size()));
            return true;
        });

    registry.registerCommand("parametric.set_baseline",
        [](ScriptContext& context, const ScriptCommand&, std::vector<std::string>& messages) {
            if (!context.hasParametricGraph) {
                messages.push_back("parametric.set_baseline requires parametric.new first");
                return false;
            }
            context.parametricBaselineBytes = serializeParametricState(context.parametricGraph);
            context.hasParametricBaseline = true;
            messages.push_back("parametric baseline set entities="
                + std::to_string(context.parametricGraph.entityCount()));
            return true;
        });

    registry.registerCommand("parametric.diff",
        [](ScriptContext& context, const ScriptCommand&, std::vector<std::string>& messages) {
            if (!context.hasParametricGraph) {
                messages.push_back("parametric.diff requires parametric.new first");
                return false;
            }
            if (!context.hasParametricBaseline) {
                messages.push_back("parametric.diff requires parametric.set_baseline first");
                return false;
            }
            const auto currBytes = serializeParametricState(context.parametricGraph);
            const auto& baseBytes = context.parametricBaselineBytes;
            const size_t changed = byteDiffCount(baseBytes, currBytes);
            const size_t firstDiff = firstByteDiff(baseBytes, currBytes);
            messages.push_back("parametric diff equal=" + std::string(changed == 0 ? "1" : "0")
                + " changed=" + std::to_string(changed)
                + " first_diff=" + std::string(firstDiff == static_cast<size_t>(-1) ? "-1" : std::to_string(firstDiff))
                + " base_hash=" + hashHex(hashBytesFnv1a64(baseBytes))
                + " curr_hash=" + hashHex(hashBytesFnv1a64(currBytes)));
            return true;
        });

    registry.registerCommand("parametric.expect_hash",
        [](ScriptContext& context, const ScriptCommand& command, std::vector<std::string>& messages) {
            if (!context.hasParametricGraph) {
                messages.push_back("parametric.expect_hash requires parametric.new first");
                return false;
            }
            const auto expected = parseExpectedHashArg(command);
            if (!expected) {
                messages.push_back("parametric.expect_hash requires hash=<uint64|0xHEX>");
                return false;
            }
            const auto bytes = serializeParametricState(context.parametricGraph);
            const uint64_t actual = hashBytesFnv1a64(bytes);
            if (actual != *expected) {
                messages.push_back("parametric.expect_hash mismatch expected="
                    + hashHex(*expected) + " actual=" + hashHex(actual));
                return false;
            }
            messages.push_back("parametric.expect_hash match " + hashHex(actual));
            return true;
        });

    registry.registerCommand("parametric.serialize",
        [](ScriptContext& context, const ScriptCommand& command, std::vector<std::string>& messages) {
            if (!context.hasParametricGraph) {
                messages.push_back("parametric.serialize requires parametric.new first");
                return false;
            }
            const auto pathArg = getArg(command, "path");
            if (!pathArg) {
                messages.push_back("parametric.serialize requires path=");
                return false;
            }
            const std::string serialized =
                nexus::parametric::ParametricGraphSerializer::serialize(context.parametricGraph);
            const std::vector<uint8_t> bytes(serialized.begin(), serialized.end());
            const std::filesystem::path target = ScriptBatchHarness::normalizePath(context.workingDirectory, *pathArg);
            if (!writeBytesToFile(target, bytes, messages)) {
                return false;
            }
            messages.push_back("parametric graph serialized entities="
                + std::to_string(context.parametricGraph.entityCount())
                + " bytes=" + std::to_string(bytes.size()));
            return true;
        });

    registry.registerCommand("parametric.load",
        [](ScriptContext& context, const ScriptCommand& command, std::vector<std::string>& messages) {
            const auto pathArg = getArg(command, "path");
            if (!pathArg) {
                messages.push_back("parametric.load requires path=");
                return false;
            }
            std::vector<uint8_t> rawBytes;
            const std::filesystem::path source = ScriptBatchHarness::normalizePath(context.workingDirectory, *pathArg);
            if (!readBytesFromFile(source, rawBytes, messages)) {
                return false;
            }
            const std::string text(rawBytes.begin(), rawBytes.end());
            nexus::parametric::ConstraintGraph graph;
            const auto result =
                nexus::parametric::ParametricGraphSerializer::deserialize(text, graph);
            if (!result.valid) {
                messages.push_back("parametric.load deserialize failed");
                return false;
            }
            context.parametricGraph    = std::move(graph);
            context.hasParametricGraph = true;
            messages.push_back("parametric graph loaded entities="
                + std::to_string(context.parametricGraph.entityCount()));
            return true;
        });

    registry.registerCommand("parametric.export_bundle",
        [](ScriptContext& context, const ScriptCommand& command, std::vector<std::string>& messages) {
            if (!context.hasParametricGraph) {
                messages.push_back("parametric.export_bundle requires parametric.new first");
                return false;
            }
            const auto pathArg = getArg(command, "path");
            if (!pathArg) {
                messages.push_back("parametric.export_bundle requires path=");
                return false;
            }
            const std::filesystem::path target = ScriptBatchHarness::normalizePath(context.workingDirectory, *pathArg);
            const auto bytes = serializeParametricState(context.parametricGraph);
            const std::string graphHash = hashHex(hashBytesFnv1a64(bytes));

            std::ostringstream oss;
            oss << "{\"parametric_hash\":\"" << graphHash << "\""
                << ",\"entity_count\":" << context.parametricGraph.entityCount()
                << ",\"distance_constraint_count\":" << context.parametricGraph.distanceConstraintCount()
                << ",\"coincident_constraint_count\":" << context.parametricGraph.coincidentConstraintCount()
                << ",\"axis_constraint_count\":" << context.parametricGraph.axisAlignedDistanceConstraintCount()
                << "}";
            const std::string json = oss.str();
            const std::vector<uint8_t> outBytes(json.begin(), json.end());
            if (!writeBytesToFile(target, outBytes, messages)) {
                return false;
            }
            messages.push_back("parametric bundle exported hash=" + graphHash
                + " bytes=" + std::to_string(outBytes.size()));
            return true;
        });

    registry.registerCommand("parametric.verify_bundle",
        [](ScriptContext& context, const ScriptCommand& command, std::vector<std::string>& messages) {
            if (!context.hasParametricGraph) {
                messages.push_back("parametric.verify_bundle requires parametric.new first");
                return false;
            }
            const auto pathArg = getArg(command, "path");
            if (!pathArg) {
                messages.push_back("parametric.verify_bundle requires path=");
                return false;
            }
            const std::filesystem::path source = ScriptBatchHarness::normalizePath(context.workingDirectory, *pathArg);
            std::vector<uint8_t> input;
            if (!readBytesFromFile(source, input, messages)) {
                return false;
            }
            const std::string text(input.begin(), input.end());
            const auto expectedHash = extractJsonStringField(text, "parametric_hash");
            if (!expectedHash) {
                messages.push_back("parametric.verify_bundle missing parametric_hash in: " + makePathString(source));
                return false;
            }
            const auto currBytes = serializeParametricState(context.parametricGraph);
            const std::string actualHash = hashHex(hashBytesFnv1a64(currBytes));
            if (*expectedHash != actualHash) {
                messages.push_back("parametric.verify_bundle mismatch expected=" + *expectedHash
                    + " actual=" + actualHash);
                return false;
            }
            messages.push_back("parametric.verify_bundle match: " + actualHash);
            return true;
        });
}

} // namespace nexus::automation
