#include <nexus/app/FaceEditMode.h>
#include <nexus/cad/CadDocument.h>
#include <nexus/cad/CadCommand.h>
#include <nexus/geometry/HalfEdgeMesh.h>
#include <nexus/geometry/SolidOperations.h>
#include <cstdio>

namespace nexus::app {

[[nodiscard]] std::string FaceEditMode::modeId() const { return "face-edit"; }
[[nodiscard]] std::string FaceEditMode::displayName() const { return "Face Edit"; }
[[nodiscard]] std::vector<ActionDescriptor> FaceEditMode::actions() const {
    return {{"face.extrude","Extrude Face","Q","modify","Face"}};
}
EventResult FaceEditMode::handleInput(AppContext& ctx, const InputEvent& event) {
    if(event.type!=InputEventType::MouseDown||event.button!=0) return EventResult::Unconsumed;
    if(!ctx.document) return EventResult::Unconsumed;
    if(ctx.selectedFace==~0u) return EventResult::Unconsumed;
    auto target = static_cast<parametric::FeatureId>(ctx.activeSelectedFeature);
    auto* node = ctx.document->history().node(target);
    if(!node||!node->mesh||node->deleted) return EventResult::Unconsumed;

    geometry::Mesh saved = *node->mesh;
    auto heOpt = geometry::HalfEdgeMesh::fromMesh(*node->mesh);
    if(!heOpt) { printf("Face edit: not manifold\n"); return EventResult::Unconsumed; }
    geometry::TweakFaceOptions opts;
    opts.distance = 0.5f;
    opts.direction = geometry::TweakDirection::AlongNormal;
    auto result = geometry::tweakFace(*heOpt, ctx.selectedFace, opts);
    if(result) {
        node->mesh.emplace(std::move(*result));
        auto cmd = std::make_unique<nexus::cad::TransformCommand>(target, std::move(saved));
        ctx.document->executeCommand(std::move(cmd));
        printf("Extruded face %u\n",ctx.selectedFace);
    } else printf("Face extrude failed\n");
    return EventResult::Consumed;
}

} // namespace nexus::app
