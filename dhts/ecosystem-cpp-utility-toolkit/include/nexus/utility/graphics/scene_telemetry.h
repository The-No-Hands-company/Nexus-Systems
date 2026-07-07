#pragma once

#include <string>
#include <sstream>
#include <vector>
#include <chrono>
#include <nexus/utility/math/vector3_utils.h>
#include <nexus/utility/graphics/screen_space_projector.h>

namespace nexus::utility::graphics {

/// @brief Captures and serializes 3D scene state for visual debugging.
/// Dumps camera, object transforms, visibility, and selection state as JSON.
/// Feeds the "spatial telemetry" side of the AI vision loop.

class SceneTelemetry {
public:
    using Vector3 = nexus::utility::math::Vector3;
    using Matrix4 = ScreenSpaceProjector::Matrix4;

    struct ObjectState {
        std::string name;
        Vector3 position;
        Vector3 rotation;   // Euler angles in radians
        Vector3 scale;
        Vector3 bboxMin, bboxMax;
        bool visible = true;
        bool selected = false;
        bool highlighted = false;
        std::string materialName;
    };

    struct CameraState {
        Vector3 position;
        Vector3 target;
        Vector3 up;
        float fov = 60.0f;
        float near = 0.1f, far = 1000.0f;
        int viewportWidth = 1920, viewportHeight = 1080;
    };

    struct TelemetrySnapshot {
        std::chrono::system_clock::time_point timestamp;
        CameraState camera;
        std::vector<ObjectState> objects;
        std::vector<std::string> selectedObjects;
        int frameNumber = 0;
        double frameTimeMs = 0.0;
    };

    SceneTelemetry() = default;

    /// @brief Set camera state.
    void setCamera(const CameraState& cam) { camera_ = cam; }
    void setCamera(const Vector3& pos, const Vector3& target, const Vector3& up, float fov, int vpW, int vpH) {
        camera_ = {pos, target, up, fov, 0.1f, 1000.0f, vpW, vpH};
    }

    /// @brief Register an object for tracking.
    void registerObject(const std::string& name) {
        ObjectState s; s.name = name; s.scale = {1,1,1};
        objects_[name] = s;
        if (objects_.size() == 1) objectOrder_.push_back(name);
        else {
            bool found = false;
            for (auto& o : objectOrder_) if (o == name) { found = true; break; }
            if (!found) objectOrder_.push_back(name);
        }
    }

    /// @brief Update an object's transform.
    void setTransform(const std::string& name, const Vector3& pos, const Vector3& rot, const Vector3& scale) {
        auto& o = objects_[name]; o.position = pos; o.rotation = rot; o.scale = scale;
    }

    /// @brief Set object bounding box.
    void setBoundingBox(const std::string& name, const Vector3& bmin, const Vector3& bmax) {
        auto& o = objects_[name]; o.bboxMin = bmin; o.bboxMax = bmax;
    }

    /// @brief Set selection/highlight state.
    void setSelected(const std::string& name, bool sel) {
        objects_[name].selected = sel;
        if (sel) selected_.insert(name); else selected_.erase(name);
    }
    void setHighlighted(const std::string& name, bool hl) { objects_[name].highlighted = hl; }
    void setVisible(const std::string& name, bool vis) { objects_[name].visible = vis; }
    void setMaterial(const std::string& name, const std::string& mat) { objects_[name].materialName = mat; }

    /// @brief Set frame timing.
    void setFrameInfo(int frame, double ms) { frameNum_ = frame; frameTime_ = ms; }

    /// @brief Take a snapshot of current state.
    [[nodiscard]] TelemetrySnapshot snapshot() const {
        TelemetrySnapshot snap;
        snap.timestamp = std::chrono::system_clock::now();
        snap.camera = camera_;
        snap.frameNumber = frameNum_;
        snap.frameTimeMs = frameTime_;
        for (auto& name : objectOrder_) {
            auto it = objects_.find(name);
            if (it != objects_.end()) {
                snap.objects.push_back(it->second);
                if (it->second.selected) snap.selectedObjects.push_back(name);
            }
        }
        return snap;
    }

    /// @brief Serialize a snapshot to JSON string.
    [[nodiscard]] static std::string toJSON(const TelemetrySnapshot& snap) {
        std::ostringstream j;
        j << "{\n  \"frame\": " << snap.frameNumber << ",\n";
        j << "  \"time_ms\": " << snap.frameTimeMs << ",\n";
        j << "  \"camera\": {\n";
        j << "    \"position\": [" << snap.camera.position.x << "," << snap.camera.position.y << "," << snap.camera.position.z << "],\n";
        j << "    \"target\": [" << snap.camera.target.x << "," << snap.camera.target.y << "," << snap.camera.target.z << "],\n";
        j << "    \"fov\": " << snap.camera.fov << ",\n";
        j << "    \"viewport\": [" << snap.camera.viewportWidth << "," << snap.camera.viewportHeight << "]\n";
        j << "  },\n";
        j << "  \"selected\": [";
        for (size_t i = 0; i < snap.selectedObjects.size(); ++i) {
            if (i) j << ","; j << "\"" << snap.selectedObjects[i] << "\"";
        }
        j << "],\n  \"objects\": [\n";
        for (size_t i = 0; i < snap.objects.size(); ++i) {
            auto& o = snap.objects[i];
            j << "    {\"name\":\"" << o.name << "\",";
            j << "\"pos\":[" << o.position.x << "," << o.position.y << "," << o.position.z << "],";
            j << "\"bbox_min\":[" << o.bboxMin.x << "," << o.bboxMin.y << "," << o.bboxMin.z << "],";
            j << "\"bbox_max\":[" << o.bboxMax.x << "," << o.bboxMax.y << "," << o.bboxMax.z << "],";
            j << "\"visible\":" << (o.visible ? "true" : "false") << ",";
            j << "\"selected\":" << (o.selected ? "true" : "false") << ",";
            j << "\"highlighted\":" << (o.highlighted ? "true" : "false");
            j << "}";
            if (i + 1 < snap.objects.size()) j << ",";
            j << "\n";
        }
        j << "  ]\n}\n";
        return j.str();
    }

    /// @brief Serialize current state to JSON.
    [[nodiscard]] std::string toJSON() const { return toJSON(snapshot()); }

    /// @brief Compute screen-space projections for all objects.
    [[nodiscard]] std::string screenSpaceReport(const ScreenSpaceProjector& projector) const {
        std::ostringstream r;
        r << "{\"objects\":[\n";
        for (size_t i = 0; i < objectOrder_.size(); ++i) {
            auto it = objects_.find(objectOrder_[i]);
            if (it == objects_.end()) continue;
            auto& o = it->second;
            auto scr = projector.projectBox(o.bboxMin, o.bboxMax);
            Vector3 center = {(o.bboxMin.x+o.bboxMax.x)*0.5f,(o.bboxMin.y+o.bboxMax.y)*0.5f,(o.bboxMin.z+o.bboxMax.z)*0.5f};
            auto cp = projector.project(center);
            r << "  {\"name\":\"" << o.name << "\","
              << "\"screen_x\":" << scr[0] << ",\"screen_y\":" << scr[1]
              << ",\"screen_w\":" << scr[2] << ",\"screen_h\":" << scr[3]
              << ",\"center_depth\":" << cp.depth
              << ",\"behind\":" << (cp.behind ? "true" : "false") << "}";
            if (i + 1 < objectOrder_.size()) r << ",";
            r << "\n";
        }
        r << "]}\n";
        return r.str();
    }

    void clear() { objects_.clear(); objectOrder_.clear(); selected_.clear(); }

private:
    CameraState camera_;
    std::map<std::string, ObjectState> objects_;
    std::vector<std::string> objectOrder_;
    std::set<std::string> selected_;
    int frameNum_ = 0;
    double frameTime_ = 0.0;
};

} // namespace nexus::utility::graphics
