#include <nexus/gfx/RenderContext.h>
#include <nexus/geometry/GeometryKernel.h>
#include <nexus/geometry/Mesh.h>
#include <nexus/render/Camera.h>
#include <nexus/render/Renderer.h>
#include <nexus/render/SceneGraph.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <numeric>
#include <string>
#include <vector>

using namespace nexus::gfx;
using namespace nexus::geometry;
using namespace nexus::render;

namespace {

struct Args {
    uint32_t frames = 120;
    uint32_t determinismRuns = 1;
    std::string outputPath;
};

Args parseArgs(int argc, char** argv)
{
    Args args{};
    for (int i = 1; i < argc; ++i) {
        const std::string_view token = argv[i];
        if (token == "--frames" && i + 1 < argc) {
            args.frames = static_cast<uint32_t>(std::stoul(argv[++i]));
        } else if (token == "--determinism-runs" && i + 1 < argc) {
            args.determinismRuns = static_cast<uint32_t>(std::stoul(argv[++i]));
            if (args.determinismRuns == 0) {
                args.determinismRuns = 1;
            }
        } else if (token == "--output" && i + 1 < argc) {
            args.outputPath = argv[++i];
        }
    }
    return args;
}

struct ScenarioResult {
    bool valid = false;
    std::string error;
    double totalMs = 0.0;
    double averageMs = 0.0;
    double medianMs = 0.0;
    uint32_t drawCalls = 0;
};

ScenarioResult runScenario(uint32_t frames)
{
    ScenarioResult result{};

    RenderContextDesc desc{};
    desc.preferredBackend = Backend::Null;
    desc.validation = ValidationLevel::Off;

    auto ctx = RenderContext::create(desc);
    if (!ctx) {
        result.error = "failed to create render context";
        return result;
    }

    SwapchainDesc sd{};
    sd.extent = {1280, 720};
    auto sc = ctx->createSwapchain(sd);
    if (!sc) {
        result.error = "failed to create swapchain";
        return result;
    }

    Renderer renderer(*ctx, *sc);

    PipelineHandle geomPipe{};
    geomPipe.id = 1001;
    PipelineHandle lightPipe{};
    lightPipe.id = 1002;
    renderer.setFallbackGeometryPipeline(geomPipe);
    renderer.setLightingCompositePipeline(lightPipe);

    auto& device = ctx->device();
    SamplerHandle sampler = device.createSampler({});
    if (!sampler.valid()) {
        result.error = "failed to create sampler";
        return result;
    }

    BufferDesc materialTableDesc{};
    materialTableDesc.sizeBytes = 64;
    materialTableDesc.usage = BufferUsage::StorageBuffer;
    materialTableDesc.memory = MemoryHint::GpuOnly;
    materialTableDesc.debugName = "perf.smoke.materialTable";
    BufferHandle materialTable = device.createBuffer(materialTableDesc);
    if (!materialTable.valid()) {
        device.destroySampler(sampler);
        result.error = "failed to create material table";
        return result;
    }

    CompositeMaterialBindings bindings{};
    bindings.albedoMaterialSampler = sampler;
    bindings.normalSampler = sampler;
    bindings.velocitySampler = sampler;
    bindings.depthSampler = sampler;
    bindings.materialTable = materialTable;
    renderer.setCompositeMaterialBindings(bindings);

    Mesh mesh = primitives::makeTriangle(1.f);
    mesh.computeVertexNormals();
    mesh.attributes().setUVs({
        {0.f, 0.f},
        {1.f, 0.f},
        {0.f, 1.f},
    });

    const auto indices = MeshUploadContract::buildTriangulatedIndexBuffer(mesh);
    const auto sections = MeshUploadContract::makeSingleSection(indices, kInvalidMaterial);
    const MeshUploadView view = MeshUploadContract::buildView(mesh, indices, sections);
    const PackedVertexLayout layout = MeshUploadContract::buildPackedVertexLayout(view);

    UploadedGeometryMesh uploaded{};
    const auto uploadReport = GeometryRenderBridge::uploadToDevice(device, view, layout, sections, uploaded);
    if (!uploadReport.valid) {
        device.destroyBuffer(materialTable);
        device.destroySampler(sampler);
        result.error = "failed to upload geometry";
        return result;
    }

    SceneGraph scene;
    Node* node = scene.createNode("perf-smoke-triangle");
    if (!node) {
        GeometryRenderBridge::destroyUploadedMesh(device, uploaded);
        device.destroyBuffer(materialTable);
        device.destroySampler(sampler);
        result.error = "failed to create scene node";
        return result;
    }
    GeometryRenderBridge::assignUploadedMeshToNode(uploaded, *node);
    node->localTransform().translation = {0.f, 0.f, -5.f};

    Camera cam;
    cam.setPerspective(70.f, 1280.f / 720.f, 0.1f, 1000.f);
    cam.lookAt({0.f, 0.f, 5.f}, {0.f, 0.f, 0.f});

    std::vector<double> frameTimesMs;
    frameTimesMs.reserve(frames);
    for (uint32_t frame = 0; frame < frames; ++frame) {
        const auto start = std::chrono::steady_clock::now();
        renderer.beginFrame();
        renderer.render(cam, scene);
        renderer.endFrame();
        const auto end = std::chrono::steady_clock::now();
        const auto elapsed = std::chrono::duration<double, std::milli>(end - start).count();
        frameTimesMs.push_back(elapsed);
    }

    std::sort(frameTimesMs.begin(), frameTimesMs.end());
    result.totalMs = std::accumulate(frameTimesMs.begin(), frameTimesMs.end(), 0.0);
    result.averageMs = frameTimesMs.empty() ? 0.0 : result.totalMs / static_cast<double>(frameTimesMs.size());
    result.medianMs = frameTimesMs.empty() ? 0.0 : frameTimesMs[frameTimesMs.size() / 2];
    result.drawCalls = renderer.lastFrameStats().drawCalls;

    GeometryRenderBridge::destroyNodeMeshPayload(device, *node);
    device.destroyBuffer(materialTable);
    device.destroySampler(sampler);
    result.valid = true;
    return result;
}

std::string buildReport(uint32_t frames,
                        uint32_t determinismRuns,
                        bool determinismConsistent,
                        double totalMs,
                        double averageMs,
                        double medianMs,
                        uint32_t drawCalls)
{
    std::string report;
    report += "backend=null\n";
    report += "scenario=single_triangle_direct_path\n";
    report += "frames=" + std::to_string(frames) + "\n";
    report += "determinism_runs=" + std::to_string(determinismRuns) + "\n";
    report += "determinism_consistent=" + std::string(determinismConsistent ? "true" : "false") + "\n";
    report += "total_ms=" + std::to_string(totalMs) + "\n";
    report += "average_ms=" + std::to_string(averageMs) + "\n";
    report += "median_ms=" + std::to_string(medianMs) + "\n";
    report += "final_frame_draw_calls=" + std::to_string(drawCalls) + "\n";
    return report;
}

} // namespace

int main(int argc, char** argv)
{
    const Args args = parseArgs(argc, argv);

    ScenarioResult baseline{};
    bool determinismConsistent = true;

    for (uint32_t run = 0; run < args.determinismRuns; ++run) {
        ScenarioResult current = runScenario(args.frames);
        if (!current.valid) {
            std::cerr << current.error << "\n";
            return 1;
        }

        if (run == 0) {
            baseline = current;
        } else {
            if (current.drawCalls != baseline.drawCalls) {
                determinismConsistent = false;
            }
        }
    }

    const std::string report = buildReport(
        args.frames,
        args.determinismRuns,
        determinismConsistent,
        baseline.totalMs,
        baseline.averageMs,
        baseline.medianMs,
        baseline.drawCalls);

    std::cout << report;
    if (!args.outputPath.empty()) {
        std::ofstream out(args.outputPath, std::ios::trunc);
        out << report;
    }

    return determinismConsistent ? 0 : 2;
}