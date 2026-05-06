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
    std::string outputPath;
};

Args parseArgs(int argc, char** argv)
{
    Args args{};
    for (int i = 1; i < argc; ++i) {
        const std::string_view token = argv[i];
        if (token == "--frames" && i + 1 < argc) {
            args.frames = static_cast<uint32_t>(std::stoul(argv[++i]));
        } else if (token == "--output" && i + 1 < argc) {
            args.outputPath = argv[++i];
        }
    }
    return args;
}

std::string buildReport(uint32_t frames,
                        double totalMs,
                        double averageMs,
                        double medianMs,
                        uint32_t drawCalls)
{
    std::string report;
    report += "backend=null\n";
    report += "scenario=single_triangle_direct_path\n";
    report += "frames=" + std::to_string(frames) + "\n";
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

    RenderContextDesc desc{};
    desc.preferredBackend = Backend::Null;
    desc.validation = ValidationLevel::Off;

    auto ctx = RenderContext::create(desc);
    if (!ctx) {
        std::cerr << "failed to create render context\n";
        return 1;
    }

    SwapchainDesc sd{};
    sd.extent = {1280, 720};
    auto sc = ctx->createSwapchain(sd);
    if (!sc) {
        std::cerr << "failed to create swapchain\n";
        return 1;
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
        std::cerr << "failed to create sampler\n";
        return 1;
    }

    BufferDesc materialTableDesc{};
    materialTableDesc.sizeBytes = 64;
    materialTableDesc.usage = BufferUsage::StorageBuffer;
    materialTableDesc.memory = MemoryHint::GpuOnly;
    materialTableDesc.debugName = "perf.smoke.materialTable";
    BufferHandle materialTable = device.createBuffer(materialTableDesc);
    if (!materialTable.valid()) {
        device.destroySampler(sampler);
        std::cerr << "failed to create material table\n";
        return 1;
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
        std::cerr << "failed to upload geometry\n";
        return 1;
    }

    SceneGraph scene;
    Node* node = scene.createNode("perf-smoke-triangle");
    if (!node) {
        GeometryRenderBridge::destroyUploadedMesh(device, uploaded);
        device.destroyBuffer(materialTable);
        device.destroySampler(sampler);
        std::cerr << "failed to create scene node\n";
        return 1;
    }
    GeometryRenderBridge::assignUploadedMeshToNode(uploaded, *node);
    node->localTransform().translation = {0.f, 0.f, -5.f};

    Camera cam;
    cam.setPerspective(70.f, 1280.f / 720.f, 0.1f, 1000.f);
    cam.lookAt({0.f, 0.f, 5.f}, {0.f, 0.f, 0.f});

    std::vector<double> frameTimesMs;
    frameTimesMs.reserve(args.frames);
    for (uint32_t frame = 0; frame < args.frames; ++frame) {
        const auto start = std::chrono::steady_clock::now();
        renderer.beginFrame();
        renderer.render(cam, scene);
        renderer.endFrame();
        const auto end = std::chrono::steady_clock::now();
        const auto elapsed = std::chrono::duration<double, std::milli>(end - start).count();
        frameTimesMs.push_back(elapsed);
    }

    std::sort(frameTimesMs.begin(), frameTimesMs.end());
    const double totalMs = std::accumulate(frameTimesMs.begin(), frameTimesMs.end(), 0.0);
    const double averageMs = frameTimesMs.empty() ? 0.0 : totalMs / static_cast<double>(frameTimesMs.size());
    const double medianMs = frameTimesMs.empty() ? 0.0 : frameTimesMs[frameTimesMs.size() / 2];

    const std::string report = buildReport(
        args.frames,
        totalMs,
        averageMs,
        medianMs,
        renderer.lastFrameStats().drawCalls);

    std::cout << report;
    if (!args.outputPath.empty()) {
        std::ofstream out(args.outputPath, std::ios::trunc);
        out << report;
    }

    GeometryRenderBridge::destroyNodeMeshPayload(device, *node);
    device.destroyBuffer(materialTable);
    device.destroySampler(sampler);
    return 0;
}