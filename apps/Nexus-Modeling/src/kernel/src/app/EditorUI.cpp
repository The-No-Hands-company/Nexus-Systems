#include <nexus/app/EditorUI.h>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <GLFW/glfw3.h>
#include <nexus/parametric/ParametricSketchProfile.h>
#include <nexus/geometry/MeshBoolean.h>
#include <nexus/geometry/SurfaceOffset.h>
#include <nexus/geometry/FaceFillet.h>

#include <cstdio>
#include <vector>

namespace nexus::app {

void EditorUI::initialize(GLFWwindow* w) {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::GetIO().IniFilename = nullptr;
    ImGui_ImplGlfw_InitForOpenGL(w, false);
    ImGui_ImplOpenGL3_Init("#version 330");
    ImGui::StyleColorsDark();
}

void EditorUI::shutdown() {
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}

void EditorUI::beginFrame() {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void EditorUI::endFrame() {
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

// Helper: place a primitive mesh into the document as a feature.
static void placePrimitive(AppContext& ctx, int primType) {
    using namespace nexus::geometry;
    Mesh prim;
    const char* name = "?";
    switch(primType) {
        case 0: prim = primitives::makeBox(2,2,2);     name="Box"; break;
        case 1: prim = primitives::makeSphere(1.5f);   name="Sphere"; break;
        case 2: prim = primitives::makeCylinder(1,3);  name="Cylinder"; break;
        case 3: prim = primitives::makeCone(1,3);      name="Cone"; break;
        case 4: prim = primitives::makeTorus(2,0.5f);  name="Torus"; break;
        case 5: prim = primitives::makePlane(4,4);     name="Plane"; break;
    }
    if(!prim.isValid() || !ctx.document) return;
    // Position at cursor world position.
    auto pos = prim.attributes().positions();
    auto& cwp = ctx.cursorWorldPos;
    for(auto& v : pos) { v.x += cwp.x; v.y += cwp.y; v.z += cwp.z; }
    prim.attributes().setPositions(std::move(pos));
    auto sk = nexus::parametric::ParametricSketchFactory::createSketch();
    auto fid = ctx.document->addSketch(sk);
    auto* node = ctx.document->history().node(fid);
    if(node) { node->mesh.emplace(std::move(prim)); node->dirty = false; }
    printf("Placed %s at (%.2f, %.2f, %.2f)\n", name, cwp.x, cwp.y, cwp.z);
}

static void exportObj(AppContext& ctx) {
    if(!ctx.document) return;
    FILE* f = fopen("export.obj","w");
    FILE* mtl = fopen("export.mtl","w");
    if(!f) { printf("Cannot open export.obj\n"); return; }
    int matId = 1;
    fprintf(f,"# Nexus Modeling export\n");
    fprintf(f,"mtllib export.mtl\n");
    size_t totalV=0, totalF=0;
    auto& hist = ctx.document->history();
    for(parametric::FeatureId i=1; i<=static_cast<parametric::FeatureId>(hist.featureCount()); ++i) {
        auto* n = hist.node(i);
        if(!n||!n->mesh||n->deleted||n->hidden) continue;
        const auto& pos = n->mesh->attributes().positions();
        const auto& topo = n->mesh->topology();
        bool hasNorms = n->mesh->attributes().hasNormals();
        fprintf(f,"usemtl mat_%d\n", matId);
        if(mtl) {
            auto& mat = n->material;
            fprintf(mtl,"newmtl mat_%d\n",matId);
            fprintf(mtl,"Kd %.3f %.3f %.3f\n",mat.albedo[0],mat.albedo[1],mat.albedo[2]);
            fprintf(mtl,"Ns %.1f\n",(1.f-mat.roughness)*128.f);
            fprintf(mtl,"Ni 1.0\n");
            fprintf(mtl,"d %.3f\n",mat.albedo[3]);
        }
        matId++;
        uint32_t voff = static_cast<uint32_t>(totalV);
        for(size_t vi=0; vi<pos.size(); ++vi) {
            fprintf(f,"v %f %f %f\n",pos[vi].x,pos[vi].y,pos[vi].z);
            if(hasNorms) {
                const auto& nrm = n->mesh->attributes().normals();
                fprintf(f,"vn %f %f %f\n",nrm[vi].x,nrm[vi].y,nrm[vi].z);
            }
        }
        for(uint32_t fi=0; fi<topo.faceCount(); ++fi) {
            const auto& face = topo.face(fi);
            fprintf(f,"f");
            for(size_t j=0; j<face.vertexCount(); ++j) {
                uint32_t idx = face.indices[j]+1+voff;
                fprintf(f, hasNorms ? " %u//%u" : " %u", idx, idx);
            }
            fprintf(f,"\n");
        }
        totalV += pos.size(); totalF += topo.faceCount();
    }
    fclose(f); if(mtl) fclose(mtl);
    printf("Exported OBJ: %zu verts, %zu faces -> export.obj\n", totalV, totalF);
}

static void importObj(AppContext& ctx) {
    if(!ctx.document) return;
    FILE* f = fopen("import.obj","r");
    if(!f) { printf("Cannot open import.obj\n"); return; }
    std::vector<Vec3> verts, normals;
    std::vector<std::vector<uint32_t>> faces;
    char line[512];
    while(fgets(line,sizeof(line),f)) {
        if(line[0]=='v' && line[1]=='n' && line[2]==' ') {
            float x,y,z; if(sscanf(line+3,"%f%f%f",&x,&y,&z)==3) normals.emplace_back(x,y,z);
        } else if(line[0]=='v' && line[1]==' ') {
            float x,y,z; if(sscanf(line+2,"%f%f%f",&x,&y,&z)==3) verts.emplace_back(x,y,z);
        } else if(line[0]=='f' && line[1]==' ') {
            std::vector<uint32_t> fi; char* p=line+2; int idx;
            while(sscanf(p,"%d",&idx)==1) {
                fi.push_back(static_cast<uint32_t>(idx < 0 ? static_cast<int>(verts.size()) + idx : idx - 1));
                while(*p && *p!=' ' && *p!='/') ++p;
                while(*p=='/'||*p==' ') ++p;
                if(!*p||*p=='\n') break;
            }
            if(fi.size()>=3) faces.push_back(std::move(fi));
        }
    }
    fclose(f);
    if(verts.empty()||faces.empty()) { printf("OBJ empty\n"); return; }
    nexus::geometry::Mesh mesh;
    mesh.attributes().setPositions(std::move(verts));
    for(auto& fc : faces) {
        nexus::geometry::Face face; face.indices = std::move(fc);
        mesh.topology().addFace(std::move(face));
    }
    (void)mesh.computeVertexNormals();
    auto sk = nexus::parametric::ParametricSketchFactory::createSketch();
    auto fid = ctx.document->addSketch(sk);
    auto* node = ctx.document->history().node(fid);
    if(node) { node->mesh.emplace(std::move(mesh)); node->dirty=false; }
    printf("Imported OBJ: %zu faces → feature %u\n", faces.size(), fid);
}

static void exportStl(AppContext& ctx) {
    if(!ctx.document) return;
    FILE* f = fopen("export.stl","wb");
    if(!f) { printf("Cannot open export.stl\n"); return; }
    char header[80]={}; fprintf(f,"%-80s","Nexus STL export"); fwrite(header,1,80,f);
    uint32_t totalTris=0;
    auto& hist = ctx.document->history();
    // Count triangles.
    for(parametric::FeatureId i=1; i<=static_cast<parametric::FeatureId>(hist.featureCount()); ++i) {
        auto* n=hist.node(i); if(!n||!n->mesh||n->deleted||n->hidden) continue;
        const auto& topo=n->mesh->topology();
        for(uint32_t fi=0; fi<topo.faceCount(); ++fi) {
            const auto& face=topo.face(fi);
            if(face.vertexCount()>=3) totalTris += static_cast<uint32_t>(face.vertexCount()-2);
        }
    }
    fwrite(&totalTris,4,1,f);
    for(parametric::FeatureId i=1; i<=static_cast<parametric::FeatureId>(hist.featureCount()); ++i) {
        auto* n=hist.node(i); if(!n||!n->mesh||n->deleted||n->hidden) continue;
        const auto& pos=n->mesh->attributes().positions();
        const auto& topo=n->mesh->topology();
        for(uint32_t fi=0; fi<topo.faceCount(); ++fi) {
            const auto& face=topo.face(fi);
            if(face.vertexCount()<3) continue;
            auto& p0=pos[face.indices[0]];
            for(size_t j=0; j+2<face.vertexCount(); ++j) {
                auto& p1=pos[face.indices[j+1]], &p2=pos[face.indices[j+2]];
                Vec3 n=(p1-p0).cross(p2-p0).normalize();
                float nx=n.x,ny=n.y,nz=n.z;
                fwrite(&nx,4,1,f);fwrite(&ny,4,1,f);fwrite(&nz,4,1,f);
                float v[9]={p0.x,p0.y,p0.z,p1.x,p1.y,p1.z,p2.x,p2.y,p2.z};
                fwrite(v,4,9,f);
                uint16_t attr=0; fwrite(&attr,2,1,f);
            }
        }
    }
    fclose(f);
    printf("Exported STL: %u triangles → export.stl\n", totalTris);
}

static void exportGltf(AppContext& ctx) {
    if(!ctx.document) return;
    // Minimal glTF 2.0 GLB export.
    auto& hist = ctx.document->history();
    std::string json = "{";
    json += "\"asset\":{\"version\":\"2.0\"},";
    json += "\"scene\":0,";
    json += "\"scenes\":[{\"nodes\":[]}],";
    json += "\"nodes\":[],";
    json += "\"meshes\":[";
    std::vector<float> allFloats; // interleaved pos + normal per vertex
    std::vector<uint32_t> allIndices;
    std::vector<uint32_t> meshPrimCounts;
    uint32_t totalVertices=0;
    bool firstMesh=true;
    for(parametric::FeatureId i=1; i<=static_cast<parametric::FeatureId>(hist.featureCount()); ++i) {
        auto* n=hist.node(i); if(!n||!n->mesh||n->deleted||n->hidden) continue;
        const auto& pos=n->mesh->attributes().positions();
        bool hasN=n->mesh->attributes().hasNormals();
        const auto& nrm=hasN?n->mesh->attributes().normals():pos;
        uint32_t vc=(uint32_t)pos.size();
        uint32_t baseIdx=(uint32_t)allIndices.size();
        // Write indices.
        const auto& topo=n->mesh->topology();
        for(uint32_t fi=0;fi<topo.faceCount();++fi){
            const auto& f=topo.face(fi);if(f.vertexCount()<3)continue;
            for(size_t j=0;j+2<f.vertexCount();++j){
                allIndices.push_back(totalVertices+f.indices[0]);
                allIndices.push_back(totalVertices+f.indices[j+1]);
                allIndices.push_back(totalVertices+f.indices[j+2]);
            }
        }
        // Write vertices (pos + normal interleaved).
        for(uint32_t vi=0;vi<vc;++vi){
            allFloats.push_back(pos[vi].x);allFloats.push_back(pos[vi].y);allFloats.push_back(pos[vi].z);
            allFloats.push_back(hasN?nrm[vi].x:0);allFloats.push_back(hasN?nrm[vi].y:0);allFloats.push_back(hasN?nrm[vi].z:1);
        }
        totalVertices+=vc;
        // Mesh primitive entry.
        char mbuf[512];
        snprintf(mbuf,sizeof(mbuf),
            "%s{\"primitives\":[{\"attributes\":{\"POSITION\":%d,\"NORMAL\":%d},\"indices\":%d,\"material\":%d}]}",
            firstMesh?"":",", (int)(meshPrimCounts.size()*2), (int)(meshPrimCounts.size()*2+1),
            (int)(meshPrimCounts.size()*3), 0);
        json+=mbuf;
        meshPrimCounts.push_back((uint32_t)allIndices.size()-baseIdx);
        firstMesh=false;
    }
    json+="],\"materials\":[{\"pbrMetallicRoughness\":{\"baseColorFactor\":[0.7,0.7,0.7,1.0],\"metallicFactor\":0,\"roughnessFactor\":0.5}}],";
    // Accessors, bufferViews, buffers.
    json+="\"accessors\":[";
    uint32_t accCount=0;
    for(size_t mi=0;mi<meshPrimCounts.size();++mi){
        char abuf[256];
        snprintf(abuf,sizeof(abuf),"%s{\"bufferView\":%d,\"componentType\":5126,\"count\":%d,\"type\":\"VEC3\",\"min\":[0,0,0],\"max\":[0,0,0]},",mi>0?",":"",accCount++,totalVertices);
        json+=abuf;
        snprintf(abuf,sizeof(abuf),"{\"bufferView\":%d,\"componentType\":5126,\"count\":%d,\"type\":\"VEC3\"},",accCount++,totalVertices);
        json+=abuf;
        snprintf(abuf,sizeof(abuf),"{\"bufferView\":%d,\"componentType\":5125,\"count\":%d,\"type\":\"SCALAR\"}%s",accCount++,(int)meshPrimCounts[mi],mi+1<meshPrimCounts.size()?",":"");
        json+=abuf;
    }
    json+="],\"bufferViews\":[";
    uint32_t byteOffset=0;
    for(size_t mi=0;mi<meshPrimCounts.size();++mi){
        char bbuf[256];
        uint32_t posSize=totalVertices*12, nrmSize=totalVertices*12, idxSize=(uint32_t)(meshPrimCounts[mi]*4);
        snprintf(bbuf,sizeof(bbuf),"%s{\"buffer\":0,\"byteOffset\":%d,\"byteLength\":%d},{\"buffer\":0,\"byteOffset\":%d,\"byteLength\":%d},{\"buffer\":0,\"byteOffset\":%d,\"byteLength\":%d}",mi>0?",":"",byteOffset,posSize,byteOffset+posSize,nrmSize,byteOffset+posSize+nrmSize,idxSize);
        json+=bbuf;
        byteOffset+=posSize+nrmSize+idxSize;
    }
    json+="],\"buffers\":[{\"byteLength\":"+std::to_string(byteOffset)+"}]}";
    // Pad JSON to 4-byte alignment.
    while(json.size()%4)json+=' ';
    // Write GLB.
    FILE* f=fopen("export.glb","wb");
    if(!f){printf("Cannot write export.glb\n");return;}
    uint32_t magic=0x46546C67, version=2;
    uint32_t totalLen=12+8+(uint32_t)json.size()+8+(uint32_t)(allFloats.size()*4+allIndices.size()*4);
    fwrite(&magic,4,1,f);fwrite(&version,4,1,f);fwrite(&totalLen,4,1,f);
    uint32_t jsonLen=(uint32_t)json.size(), jsonType=0x4E4F534A;
    fwrite(&jsonLen,4,1,f);fwrite(&jsonType,4,1,f);fwrite(json.data(),1,jsonLen,f);
    uint32_t binLen=(uint32_t)(allFloats.size()*4+allIndices.size()*4), binType=0x004E4942;
    fwrite(&binLen,4,1,f);fwrite(&binType,4,1,f);
    fwrite(allFloats.data(),4,allFloats.size(),f);
    fwrite(allIndices.data(),4,allIndices.size(),f);
    fclose(f);
    printf("Exported glTF: %zu verts, %zu indices → export.glb\n", (size_t)totalVertices, allIndices.size());
}

static void importStl(AppContext& ctx) {
    if(!ctx.document) return;
    FILE* f = fopen("import.stl","rb");
    if(!f) { printf("Cannot open import.stl\n"); return; }
    fseek(f,80,SEEK_SET);
    uint32_t triCount; if(fread(&triCount,4,1,f)!=1){fclose(f);return;}
    if(triCount>50000000){fclose(f);return;}
    std::vector<Vec3> verts;
    std::vector<std::vector<uint32_t>> faces;
    for(uint32_t t=0; t<triCount; ++t) {
        float nx,ny,nz;
        fread(&nx,4,1,f);fread(&ny,4,1,f);fread(&nz,4,1,f);
        float v[9]; fread(v,4,9,f);
        uint16_t attr; fread(&attr,2,1,f);
        uint32_t base = static_cast<uint32_t>(verts.size());
        verts.emplace_back(v[0],v[1],v[2]);
        verts.emplace_back(v[3],v[4],v[5]);
        verts.emplace_back(v[6],v[7],v[8]);
        faces.push_back({base,base+1,base+2});
    }
    fclose(f);
    geometry::Mesh mesh;
    mesh.attributes().setPositions(std::move(verts));
    for(auto& fc:faces){geometry::Face face;face.indices=std::move(fc);mesh.topology().addFace(std::move(face));}
    (void)mesh.computeVertexNormals();
    auto sk = nexus::parametric::ParametricSketchFactory::createSketch();
    auto fid = ctx.document->addSketch(sk);
    auto* node = ctx.document->history().node(fid);
    if(node){node->mesh.emplace(std::move(mesh));node->dirty=false;}
    printf("Imported STL: %u triangles → feature %u\n", triCount, fid);
}

static void mirrorSelected(AppContext& ctx, FeatureId selectedId, int axis) {
    if(!ctx.document || selectedId == nexus::parametric::kInvalidFeatureId) return;
    auto* n = ctx.document->history().node(selectedId);
    if(!n || !n->mesh) return;
    auto pos = n->mesh->attributes().positions();
    for(auto& v : pos) {
        if(axis == 0) v.x = -v.x;
        else if(axis == 1) v.y = -v.y;
        else v.z = -v.z;
    }
    n->mesh->attributes().setPositions(std::move(pos));
    printf("Mirrored across %s plane\n", axis==0?"YZ":axis==1?"XZ":"XY");
}

static void screenshotPPM(AppContext& ctx) {
    (void)ctx;
    GLint vp[4]; glGetIntegerv(GL_VIEWPORT, vp);
    int w=vp[2], h=vp[3];
    std::vector<unsigned char> pixels(w*h*3);
    glReadPixels(0,0,w,h,GL_RGB,GL_UNSIGNED_BYTE,pixels.data());
    FILE* f=fopen("screenshot.ppm","wb");
    if(!f){printf("Cannot write screenshot.ppm\n");return;}
    fprintf(f,"P6\n%d %d\n255\n",w,h);
    // Flip vertically (OpenGL origin is bottom-left).
    for(int y=h-1;y>=0;--y) fwrite(&pixels[y*w*3],1,w*3,f);
    fclose(f);
    printf("Screenshot: %dx%d → screenshot.ppm\n",w,h);
}

// Apply boolean operation between selected feature and most recent other feature.
static void applyBool(AppContext& ctx, FeatureId selectedId, nexus::geometry::BooleanOp op) {
    if(!ctx.document || selectedId == nexus::parametric::kInvalidFeatureId) {
        printf("Boolean: need document and selection\n"); return;
    }
    using namespace nexus::geometry;
    auto& hist = ctx.document->history();
    size_t fc = hist.featureCount();
    if(fc < 2 || selectedId == nexus::parametric::kInvalidFeatureId) {
        printf("Boolean: need at least 2 features with 1 selected\n"); return;
    }
    FeatureId otherId = static_cast<FeatureId>(fc); // most recent
    if(otherId == selectedId) {
        otherId = static_cast<FeatureId>(fc - 1);
        if(otherId == nexus::parametric::kInvalidFeatureId) { printf("Boolean: no second feature\n"); return; }
    }
    auto* nodeA = hist.node(selectedId);
    auto* nodeB = hist.node(otherId);
    if(!nodeA || !nodeB || !nodeA->mesh || !nodeB->mesh) {
        printf("Boolean: both features must have meshes\n"); return;
    }
    auto result = MeshBoolean::compute(*nodeA->mesh, *nodeB->mesh, op);
    if(!result.ok) { printf("Boolean failed: %s\n", result.error.c_str()); return; }
    const char* opName = (op==BooleanOp::Union)?"Union":(op==BooleanOp::Difference)?"Difference":"Intersection";
    auto sk = nexus::parametric::ParametricSketchFactory::createSketch();
    auto fid = ctx.document->addSketch(sk);
    auto* node = hist.node(fid);
    if(node) { node->mesh.emplace(std::move(result.result)); node->dirty = false; }
    printf("Boolean %s: feature %u = %u %s %u\n", opName, fid, selectedId, opName, otherId);
}

bool EditorUI::renderMenuBar(AppContext& ctx, TransformGizmo& gizmo,
                               ViewportController& viewport) {
    bool action = false;
    if(!ImGui::BeginMainMenuBar()) return false;

    if(ImGui::BeginMenu("File")) {
        if(ImGui::MenuItem("Save", "Ctrl+S")) {
            auto data = ctx.document->serialize();
            FILE* f = fopen("scene.nxm","wb");
            if(f){fwrite(data.data(),1,data.size(),f);fclose(f); printf("Saved\n");}
            action=true;
        }
        if(ImGui::MenuItem("Load", "Ctrl+O")) {
            FILE* f = fopen("scene.nxm","rb");
            if(f){fseek(f,0,SEEK_END);long sz=ftell(f);fseek(f,0,SEEK_SET);
                std::vector<uint8_t> d(sz);fread(d.data(),1,sz,f);fclose(f);
                (void)ctx.document->deserialize(d.data(),d.size()); printf("Loaded\n");}
            action=true;
        }
        ImGui::Separator();
        if(ImGui::MenuItem("Export OBJ")) {
            exportObj(ctx);
            action=true;
        }
        if(ImGui::MenuItem("Export STL")) {
            exportStl(ctx);
            action=true;
        }
        if(ImGui::MenuItem("Export glTF")) {
            exportGltf(ctx);
            action=true;
        }
        if(ImGui::MenuItem("Import OBJ")) {
            importObj(ctx);
            action=true;
        }
        if(ImGui::MenuItem("Import STL")) {
            importStl(ctx);
            action=true;
        }
        ImGui::Separator();
        if(ImGui::MenuItem("Screenshot (PPM)")) {
            screenshotPPM(ctx);
            action=true;
        }
        ImGui::Separator();
        if(ImGui::MenuItem("Quit")) { glfwSetWindowShouldClose(glfwGetCurrentContext(),1); }
        ImGui::EndMenu();
    }

    if(ImGui::BeginMenu("Edit")) {
        if(ImGui::MenuItem("Undo","Ctrl+Z")) { ctx.document->undo(); action=true; }
        if(ImGui::MenuItem("Redo","Ctrl+Y"))  { ctx.document->redo(); action=true; }
        ImGui::Separator();
        if(ImGui::MenuItem("Delete","Del")) {
            auto fid = static_cast<FeatureId>(ctx.activeSelectedFeature);
            if(ctx.document && fid != nexus::parametric::kInvalidFeatureId) {
                ctx.document->deleteFeature(fid);
                printf("Deleted feature %u\n", fid);
            }
            action=true;
        }
        ImGui::EndMenu();
    }

    if(ImGui::BeginMenu("View")) {
        const char* vp[]={"Wireframe","Solid","Shaded","Material","Rendered","XRay","HiddenLine"};
        for(int i=0;i<7;++i)
            if(ImGui::MenuItem(vp[i],nullptr,static_cast<int>(viewport.mode())==i))
                viewport.setMode(static_cast<ViewportMode>(i));
        ImGui::EndMenu();
    }

    if(ImGui::BeginMenu("Create")) {
        if(ImGui::MenuItem("Box"))      placePrimitive(ctx,0);
        if(ImGui::MenuItem("Sphere"))   placePrimitive(ctx,1);
        if(ImGui::MenuItem("Cylinder")) placePrimitive(ctx,2);
        if(ImGui::MenuItem("Cone"))     placePrimitive(ctx,3);
        if(ImGui::MenuItem("Torus"))    placePrimitive(ctx,4);
        if(ImGui::MenuItem("Plane"))    placePrimitive(ctx,5);
        ImGui::EndMenu();
    }

    if(ImGui::BeginMenu("Mode")) {
        const char* ids[]={"select","sketch","extrude","revolve","fillet","modeling","dimension","face-edit","edge-edit","vertex-edit","boolean","pattern","mirror"};
        const char* names[]={"Select","Sketch","Extrude","Revolve","Fillet","Modeling","Dimension","FaceEdit","EdgeEdit","VertexEdit","Boolean","Pattern","Mirror"};
        const int n = 13;
        for(int i=0;i<n;++i) {
            bool active = ctx.orchestrator && ctx.orchestrator->activeModeId()==ids[i];
            if(ImGui::MenuItem(names[i],nullptr,active)) {
                if(ctx.orchestrator) ctx.orchestrator->switchTo(ids[i]);
                printf("Mode: %s\n", ids[i]);
            }
        }
        ImGui::EndMenu();
    }

    if(ImGui::BeginMenu("Gizmo")) {
        bool t = gizmo.mode()==TransformGizmo::Mode::Translate;
        bool s = gizmo.mode()==TransformGizmo::Mode::Scale;
        bool r = gizmo.mode()==TransformGizmo::Mode::Rotate;
        if(ImGui::MenuItem("Translate","W",&t)) gizmo.setMode(TransformGizmo::Mode::Translate);
        if(ImGui::MenuItem("Scale","E",&s))     gizmo.setMode(TransformGizmo::Mode::Scale);
        if(ImGui::MenuItem("Rotate","R",&r))    gizmo.setMode(TransformGizmo::Mode::Rotate);
        ImGui::EndMenu();
    }

    ImGui::EndMainMenuBar();
    return action;
}

void EditorUI::renderToolbar(AppContext& ctx, TransformGizmo& gizmo) {
    ImGui::Begin("Toolbar",nullptr,ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoResize|ImGuiWindowFlags_AlwaysAutoResize);
    if(ctx.orchestrator) {
        if(ImGui::Button("Select"))   ctx.orchestrator->switchTo("select");
        ImGui::SameLine();
        if(ImGui::Button("Sketch"))   ctx.orchestrator->switchTo("sketch");
        ImGui::SameLine();
        if(ImGui::Button("Modeling")) ctx.orchestrator->switchTo("modeling");
        ImGui::SameLine();
    }
    ImGui::Separator(); ImGui::SameLine();
    if(ImGui::Button("Translate")) gizmo.setMode(TransformGizmo::Mode::Translate);
    ImGui::SameLine();
    if(ImGui::Button("Rotate")) gizmo.setMode(TransformGizmo::Mode::Rotate);
    ImGui::SameLine();
    if(ImGui::Button("Scale")) gizmo.setMode(TransformGizmo::Mode::Scale);
    ImGui::SameLine(); ImGui::Separator(); ImGui::SameLine();
    if(ImGui::SmallButton("Box"))      { placePrimitive(ctx,0); } ImGui::SameLine();
    if(ImGui::SmallButton("Sphere"))   { placePrimitive(ctx,1); } ImGui::SameLine();
    if(ImGui::SmallButton("Cyl"))      { placePrimitive(ctx,2); } ImGui::SameLine();
    if(ImGui::SmallButton("Cone"))     { placePrimitive(ctx,3); } ImGui::SameLine();
    if(ImGui::SmallButton("Torus"))    { placePrimitive(ctx,4); } ImGui::SameLine();
    if(ImGui::SmallButton("Plane"))    { placePrimitive(ctx,5); }
    ImGui::End();
}

void EditorUI::renderContextMenu(AppContext& ctx, TransformGizmo& gizmo, FeatureId selectedId) {
    (void)gizmo;
    if(!ImGui::BeginPopupContextVoid("ViewportContext", ImGuiPopupFlags_MouseButtonRight)) return;
    if(ImGui::MenuItem("Create Box"))      placePrimitive(ctx,0);
    if(ImGui::MenuItem("Create Sphere"))   placePrimitive(ctx,1);
    if(ImGui::MenuItem("Create Cylinder")) placePrimitive(ctx,2);
    if(ImGui::MenuItem("Create Cone"))     placePrimitive(ctx,3);
    if(ImGui::MenuItem("Create Torus"))    placePrimitive(ctx,4);
    if(ImGui::MenuItem("Create Plane"))    placePrimitive(ctx,5);
    ImGui::Separator();
    if(ImGui::MenuItem("Delete Selected")) {
        if(selectedId != nexus::parametric::kInvalidFeatureId)
            ctx.document->deleteFeature(selectedId);
    }
    if(ImGui::MenuItem("Instance")) {
        if(selectedId != nexus::parametric::kInvalidFeatureId) {
            auto* n = ctx.document->history().node(selectedId);
            if(n && n->mesh) {
                nexus::geometry::Mesh copy = *n->mesh;
                auto sk = nexus::parametric::ParametricSketchFactory::createSketch();
                auto fid = ctx.document->addSketch(sk);
                auto* newNode = ctx.document->history().node(fid);
                if(newNode) { newNode->mesh.emplace(std::move(copy)); newNode->dirty=false; }
                printf("Instanced %u → %u\n", selectedId, fid);
            }
        }
    }
    if(ImGui::MenuItem("Set Material to All")) {
        if(selectedId != nexus::parametric::kInvalidFeatureId && ctx.document) {
            auto* src = ctx.document->history().node(selectedId);
            if(src) {
                auto& hist = ctx.document->history();
                for(FeatureId i=1; i<=static_cast<FeatureId>(hist.featureCount()); ++i) {
                    auto* n = hist.node(i);
                    if(n && n->mesh && !n->deleted) n->material = src->material;
                }
                printf("Material set to all features\n");
            }
        }
    }
    ImGui::Separator();
    if(ImGui::MenuItem("Mirror XZ"))   { mirrorSelected(ctx, selectedId, 1); }
    if(ImGui::MenuItem("Mirror XY"))   { mirrorSelected(ctx, selectedId, 2); }
    if(ImGui::MenuItem("Mirror YZ"))   { mirrorSelected(ctx, selectedId, 0); }
    ImGui::Separator();
    if(ImGui::MenuItem("Align to Ground")) {
        if(selectedId != nexus::parametric::kInvalidFeatureId && ctx.document) {
            auto* n = ctx.document->history().node(selectedId);
            if(n && n->mesh) {
                auto b = n->mesh->computeBounds();
                auto pos = n->mesh->attributes().positions();
                for(auto& v : pos) v.y -= b.min.y;
                n->mesh->attributes().setPositions(std::move(pos));
                printf("Aligned to ground (Y=%.2f)\n", b.min.y);
            }
        }
    }
    if(ImGui::MenuItem("Sweep (Z)")) {
        if(ctx.document && selectedId != nexus::parametric::kInvalidFeatureId) {
            auto* n = ctx.document->history().node(selectedId);
            if(n && n->mesh) {
                // Simple sweep: copy vertices, offset Z, bridge.
                auto orig = n->mesh->attributes().positions();
                std::vector<Vec3> newPos = orig;
                for(auto& v : newPos) v.z += 2.f;
                auto allPos = orig;
                allPos.insert(allPos.end(), newPos.begin(), newPos.end());
                n->mesh->attributes().setPositions(std::move(allPos));
                printf("Sweep: extended mesh (Z+2)\n");
            }
        }
    }
    ImGui::Separator();
    if(ImGui::BeginMenu("Boolean")) {
        if(ImGui::MenuItem("Union", "Shift+U"))     applyBool(ctx, selectedId, nexus::geometry::BooleanOp::Union);
        if(ImGui::MenuItem("Difference", "Shift+D")) applyBool(ctx, selectedId, nexus::geometry::BooleanOp::Difference);
        if(ImGui::MenuItem("Intersection", "Shift+I")) applyBool(ctx, selectedId, nexus::geometry::BooleanOp::Intersection);
        ImGui::EndMenu();
    }
    ImGui::Separator();
    if(ImGui::MenuItem("Unhide All", "Ctrl+H")) {
        if(ctx.document) ctx.document->history().unhideAll();
    }
    if(ImGui::MenuItem("View All", nullptr, false, true)) {
        auto& cam = ctx.cameraPosition;
        cam = {0,0,10};
    }
    ImGui::EndPopup();
}

void EditorUI::renderStatusBar(const AppContext& ctx, bool snapToGrid, FeatureId selectedId) {
    ImGui::Begin("Status",nullptr,ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoScrollbar|ImGuiWindowFlags_AlwaysAutoResize);
    int liveFc = 0;
    if(ctx.document) {
        auto& hist = ctx.document->history();
        for(FeatureId i=1; i<=static_cast<FeatureId>(hist.featureCount()); ++i) {
            auto* n=hist.node(i); if(n&&!n->deleted) liveFc++;
        }
    }
    const char* mode = ctx.orchestrator ? ctx.orchestrator->activeModeId().c_str() : "none";
    const char* plane = (ctx.workPlane == AppContext::WorkPlane::XZ) ? "XZ" :
                        (ctx.workPlane == AppContext::WorkPlane::XY) ? "XY" : "YZ";
    ImGui::Text("Objects: %d | Sel: %llu | Snap: %s | Plane: %s | Mode: %s | Cursor: (%.1f, %.1f, %.1f)", liveFc,
        static_cast<unsigned long long>(selectedId), snapToGrid ? "ON" : "OFF", plane, mode,
        ctx.cursorWorldPos.x, ctx.cursorWorldPos.y, ctx.cursorWorldPos.z);
    ImGui::End();
}

void EditorUI::renderOutliner(AppContext& ctx, FeatureId& selectedId) {
    ImGui::Begin("Outliner");
    const auto& hist = ctx.document->history();
    size_t fc = hist.featureCount();
    for(FeatureId i = 1; i <= static_cast<FeatureId>(fc); ++i) {
        auto* n = hist.node(i);
        if(!n || n->deleted || n->hidden) continue;
        const char* kindStr = "?";
        switch(n->kind) {
            case nexus::parametric::FeatureKind::Sketch:  kindStr="Sketch"; break;
            case nexus::parametric::FeatureKind::Extrude: kindStr="Extrude"; break;
            case nexus::parametric::FeatureKind::Revolve: kindStr="Revolve"; break;
        }
        char label[128];
        (void)snprintf(label,sizeof(label),"[%u] %s",i,kindStr);
        bool sel = (selectedId == i);
        if(ImGui::Selectable(label,sel) && !sel) {
            selectedId = i;
            ctx.selectedFace = ~0u;
            ctx.selectedVertex = ~0u;
        }
        if(ImGui::BeginPopupContextItem(label)) {
            if(ImGui::MenuItem("Delete")) {
                if(ctx.document) ctx.document->deleteFeature(i);
                if(selectedId == i) selectedId = nexus::parametric::kInvalidFeatureId;
            }
            if(ImGui::BeginMenu("Display")) {
                using DM = nexus::parametric::FeatureNode::DisplayMode;
                auto* mutableNode = ctx.document ? ctx.document->history().node(i) : nullptr;
                if(mutableNode) {
                    if(ImGui::MenuItem("Solid", nullptr, mutableNode->displayMode==DM::Solid)) mutableNode->displayMode=DM::Solid;
                    if(ImGui::MenuItem("Wireframe", nullptr, mutableNode->displayMode==DM::Wireframe)) mutableNode->displayMode=DM::Wireframe;
                    if(ImGui::MenuItem("BoundingBox", nullptr, mutableNode->displayMode==DM::BoundingBox)) mutableNode->displayMode=DM::BoundingBox;
                }
                ImGui::EndMenu();
            }
            ImGui::EndPopup();
        }
    }
    ImGui::End();
}

void EditorUI::renderProperties(AppContext& ctx, FeatureId selectedId) {
    ImGui::Begin("Inspector");
    if(selectedId == nexus::parametric::kInvalidFeatureId || ctx.document->history().featureCount() == 0) {
        ImGui::TextUnformatted("No object selected.");
        ImGui::End(); return;
    }
    auto* n = ctx.document->history().node(selectedId);
    if(!n || !n->mesh) { ImGui::TextUnformatted("Selected object not found."); ImGui::End(); return; }

    const char* kindStr = "?";
    switch(n->kind) {
        case nexus::parametric::FeatureKind::Sketch:  kindStr="Sketch"; break;
        case nexus::parametric::FeatureKind::Extrude: kindStr="Extrude"; break;
        case nexus::parametric::FeatureKind::Revolve: kindStr="Revolve"; break;
    }
    ImGui::Text("Feature ID: %u", selectedId);
    ImGui::Text("Type: %s", kindStr);
    // Editable name.
    char nameBuf[128];
    snprintf(nameBuf,sizeof(nameBuf),"%s",n->name.c_str());
    if(ImGui::InputText("Name",nameBuf,sizeof(nameBuf))) {
        ctx.document->history().setName(selectedId, nameBuf);
    }
    ImGui::Text("Vertices: %zu", n->mesh->attributes().vertexCount());
    ImGui::Text("Faces: %zu", n->mesh->topology().faceCount());

    ImGui::Separator();
    if(ImGui::Button("Instance")) {
        nexus::geometry::Mesh copy = *n->mesh;
        auto sk = nexus::parametric::ParametricSketchFactory::createSketch();
        auto fid = ctx.document->addSketch(sk);
        auto* newNode = ctx.document->history().node(fid);
        if(newNode) { newNode->mesh.emplace(std::move(copy)); newNode->dirty=false; }
        printf("Instanced feature %u → %u\n", selectedId, fid);
    }
    ImGui::Separator();
    ImGui::TextUnformatted("Transform");

    auto bounds = n->mesh->computeBounds();
    Vec3 center = bounds.center();
    float pos[3] = {center.x, center.y, center.z};
    if(ImGui::DragFloat3("Position", pos, 0.1f)) {
        Vec3 delta{pos[0]-center.x, pos[1]-center.y, pos[2]-center.z};
        auto verts = n->mesh->attributes().positions();
        for(auto& v : verts) { v.x+=delta.x; v.y+=delta.y; v.z+=delta.z; }
        n->mesh->attributes().setPositions(std::move(verts));
    }

    Vec3 extents = bounds.extents();
    float scl[3] = {extents.x*2, extents.y*2, extents.z*2};
    ImGui::Text("Size: %.2f x %.2f x %.2f", scl[0], scl[1], scl[2]);

    // Parametric primitive editing.
    if(n->primType != nexus::parametric::FeatureNode::PrimType::None) {
        ImGui::Separator();
        ImGui::TextUnformatted("Primitive Params");
        bool changed = false;
        auto& pp = n->primParams;
        using PT = nexus::parametric::FeatureNode::PrimType;
        if(n->primType == PT::Box) {
            changed|=ImGui::DragFloat("Width",&pp[0],0.1f,0.1f,100.f);
            changed|=ImGui::DragFloat("Height",&pp[1],0.1f,0.1f,100.f);
            changed|=ImGui::DragFloat("Depth",&pp[2],0.1f,0.1f,100.f);
        } else if(n->primType == PT::Sphere) {
            changed|=ImGui::DragFloat("Radius",&pp[0],0.1f,0.1f,100.f);
        } else if(n->primType == PT::Cylinder||n->primType==PT::Cone) {
            changed|=ImGui::DragFloat("Radius",&pp[0],0.1f,0.1f,100.f);
            changed|=ImGui::DragFloat("Height",&pp[1],0.1f,0.1f,100.f);
        } else if(n->primType == PT::Torus) {
            changed|=ImGui::DragFloat("Major R",&pp[0],0.1f,0.1f,100.f);
            changed|=ImGui::DragFloat("Minor R",&pp[1],0.1f,0.1f,100.f);
        } else if(n->primType == PT::Plane) {
            changed|=ImGui::DragFloat("Width",&pp[0],0.1f,0.1f,100.f);
            changed|=ImGui::DragFloat("Height",&pp[1],0.1f,0.1f,100.f);
        }
        if(changed && ctx.document) {
            auto ctr = n->mesh->computeBounds().center();
            geometry::Mesh newMesh;
            if(n->primType==PT::Box) newMesh=nexus::geometry::primitives::makeBox(pp[0],pp[1],pp[2]);
            else if(n->primType==PT::Sphere) newMesh=nexus::geometry::primitives::makeSphere(pp[0]);
            else if(n->primType==PT::Cylinder) newMesh=nexus::geometry::primitives::makeCylinder(pp[0],pp[1]);
            else if(n->primType==PT::Cone) newMesh=nexus::geometry::primitives::makeCone(pp[0],pp[1]);
            else if(n->primType==PT::Torus) newMesh=nexus::geometry::primitives::makeTorus(pp[0],pp[1]);
            else if(n->primType==PT::Plane) newMesh=nexus::geometry::primitives::makePlane(pp[0],pp[1]);
            if(newMesh.isValid()) {
                auto pos=newMesh.attributes().positions();
                for(auto& v:pos){v.x+=ctr.x;v.y+=ctr.y;v.z+=ctr.z;}
                newMesh.attributes().setPositions(std::move(pos));
                (void)newMesh.computeVertexNormals();
                n->mesh.emplace(std::move(newMesh));
            }
        }
    }

    ImGui::Separator();
    ImGui::TextUnformatted("Transform");
    ImGui::DragFloat3("Position", pos, 0.1f);
    // Scale derived from bounds.
    if(ImGui::DragFloat3("Size", scl, 0.1f, 0.01f, 1000.f)) {
        Vec3 newExt{scl[0]/2, scl[1]/2, scl[2]/2};
        auto oldExt = n->mesh->computeBounds().extents();
        Vec3 scaleFac{newExt.x/(oldExt.x+1e-6f), newExt.y/(oldExt.y+1e-6f), newExt.z/(oldExt.z+1e-6f)};
        auto ctr = n->mesh->computeBounds().center();
        auto verts = n->mesh->attributes().positions();
        for(auto& v : verts) {
            v.x = ctr.x + (v.x - ctr.x) * scaleFac.x;
            v.y = ctr.y + (v.y - ctr.y) * scaleFac.y;
            v.z = ctr.z + (v.z - ctr.z) * scaleFac.z;
        }
        n->mesh->attributes().setPositions(std::move(verts));
    }
    if(ImGui::Button("Shell")) {
        if(ctx.document && selectedId != nexus::parametric::kInvalidFeatureId) {
            auto* nd = ctx.document->history().node(selectedId);
            if(nd && nd->mesh) {
                geometry::Mesh saved = *nd->mesh;
                geometry::SurfaceOffsetOptions opts;
                opts.distance = -0.15f;
                auto shelled = geometry::SurfaceOffset::offset(*nd->mesh, opts);
                if(shelled.isValid()) {
                    nd->mesh.emplace(std::move(shelled));
                    printf("Shell: offset -0.15\n");
                } else { printf("Shell failed\n"); }
            }
        }
    }
    ImGui::SameLine();
    if(ImGui::Button("Draft")) {
        if(ctx.document && selectedId != nexus::parametric::kInvalidFeatureId) {
            auto* nd = ctx.document->history().node(selectedId);
            if(nd && nd->mesh && ctx.selectedFace != ~0u) {
                auto heOpt = geometry::HalfEdgeMesh::fromMesh(*nd->mesh);
                if(heOpt) {
                    geometry::FaceOffsetOptions opts;
                    opts.offset = 0.f;
                    opts.draftAngleDeg = 5.f;
                    auto result = geometry::offsetFaceWithDraft(*heOpt, ctx.selectedFace, opts);
                    if(result.isValid()) { nd->mesh.emplace(std::move(result)); printf("Draft: 5 deg on face %u\n", ctx.selectedFace); }
                    else printf("Draft failed\n");
                }
            }
        }
    }
    ImGui::SameLine();
    if(ImGui::Button("Center")) {
        if(ctx.document && selectedId != nexus::parametric::kInvalidFeatureId) {
            auto* nd = ctx.document->history().node(selectedId);
            if(nd && nd->mesh) {
                auto ctr = nd->mesh->computeBounds().center();
                auto pos = nd->mesh->attributes().positions();
                for(auto& v : pos) { v.x -= ctr.x; v.y -= ctr.y; v.z -= ctr.z; }
                nd->mesh->attributes().setPositions(std::move(pos));
                printf("Centered to origin\n");
            }
        }
    }

    ImGui::Separator();
    ImGui::TextUnformatted("Material (PBR)");
    ImGui::ColorEdit4("Albedo", n->material.albedo);
    ImGui::DragFloat("Roughness", &n->material.roughness, 0.01f, 0.f, 1.f);
    ImGui::DragFloat("Metallic", &n->material.metallic, 0.01f, 0.f, 1.f);

    ImGui::End();
}

void EditorUI::renderTimeline(FeatureId /*selectedId*/, bool& playing,
                                int& frame, int maxFrame) {
    ImGui::Begin("Timeline", nullptr, ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoResize|ImGuiWindowFlags_AlwaysAutoResize);
    if(ImGui::Button(playing ? "Pause" : "Play")) playing = !playing;
    ImGui::SameLine();
    if(ImGui::Button("|<")) frame = 0;
    ImGui::SameLine();
    if(ImGui::Button("<"))  frame = std::max(0, frame-1);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(100);
    ImGui::DragInt("##frame", &frame, 1.f, 0, maxFrame, "frame %d");
    ImGui::SameLine();
    if(ImGui::Button(">"))  frame = std::min(maxFrame, frame+1);
    ImGui::SameLine();
    if(ImGui::Button(">|")) frame = maxFrame;
    ImGui::SameLine();
    ImGui::Text("I=key   K=clear");
    ImGui::End();
}

void EditorUI::renderKeybindings() {
    ImGui::Begin("Keybinding Reference", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
    ImGui::Text("Modes: ESC=Select  S=Sketch  E=Extrude  R=Revolve  F=Fillet  M=Modeling  D=Dimension");
    ImGui::Text("       Q=FaceEdit  B=EdgeEdit  V=VertexEdit  K=Boolean  P=Pattern  N=Mirror");
    ImGui::Separator();
    ImGui::Text("Gizmo: W=Translate  E=Scale  R=Rotate  G=SnapToggle  Ctrl=Snap-to-geometry");
    ImGui::Text("       Shift+Tab=Cycle snap mode  Tab=Cycle work plane");
    ImGui::Separator();
    ImGui::Text("Edit:  Ctrl+Z=Undo  Ctrl+Y=Redo  Ctrl+D=Duplicate  Del=Delete");
    ImGui::Text("       Ctrl+C=Copy  Ctrl+V=Paste  Ctrl+A=SelectAll  A=Deselect");
    ImGui::Separator();
    ImGui::Text("View:  Home=ViewAll  F=FrameSel  KP5/O=Ortho  L=Lighting");
    ImGui::Text("       KP0=ISO  KP1=Front  KP3=Right  KP7=Top  Ctrl+Q=QuadView");
    ImGui::Separator();
    ImGui::Text("Bool:  Shift+U=Union  Shift+D=Diff  Shift+I=Intersect");
    ImGui::Text("Sketch: 1=Point  2=Rect  3=Circle");
    ImGui::Text("Anim:  Space=Play  I=Keyframe  K=Clear  Arrows=Frame");
    ImGui::Text("Hide:  H=Hide  Shift+H=Isolate  Ctrl+H=UnhideAll");
    ImGui::Text("Grid:  [=Finer  ]=Coarser");
    ImGui::End();
}

void EditorUI::renderUndoHistory(AppContext& ctx) {
    ImGui::Begin("Undo History");
    if(!ctx.document) { ImGui::End(); return; }
    auto undoNames = ctx.document->undoDescriptions();
    auto redoNames = ctx.document->redoDescriptions();
    ImGui::Text("Undo stack (%zu):", undoNames.size());
    for(auto& name : undoNames) ImGui::Text("  %s", name.c_str());
    ImGui::Separator();
    ImGui::Text("Redo stack (%zu):", redoNames.size());
    for(auto& name : redoNames) ImGui::Text("  %s", name.c_str());
    ImGui::End();
}

} // namespace nexus::app
