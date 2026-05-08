#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  Nexus Render — Scene Graph
//
//  Lightweight node-based scene graph.
//  Nodes form a transform hierarchy; leaf nodes hold renderable references.
//  Designed for both real-time viewport and batch rendering.
// ─────────────────────────────────────────────────────────────────────────────
#include <nexus/gfx/Types.h>
#include <nexus/gfx/Device.h>
#include <nexus/render/Camera.h>
#include <cstdint>
#include <memory>
#include <vector>
#include <string>
#include <functional>
#include <unordered_map>

namespace nexus::render {

// ── Transform (column-major decomposed, avoids gimbal lock) ──────────────────
struct Transform {
    Vec3  translation = {0.f, 0.f, 0.f};
    Vec4  rotation    = {0.f, 0.f, 0.f, 1.f};  // quaternion xyzw
    Vec3  scale       = {1.f, 1.f, 1.f};

    [[nodiscard]] Mat4 toMatrix() const noexcept;
};

// ── Mesh reference (geometry held by GPU buffers) ─────────────────────────────
struct MeshRef {
    nexus::gfx::BufferHandle  vertexBuffer;
    nexus::gfx::BufferHandle  indexBuffer;
    nexus::gfx::BufferHandle  meshletBuffer;   // valid when mesh shaders active
    uint32_t                  indexCount    = 0;
    uint32_t                  meshletCount  = 0;
    nexus::gfx::AccelStructHandle blas;        // valid when RT active
};

// ── Material reference (opaque; owned by material system, not scene graph) ───
using MaterialID = uint32_t;
inline constexpr MaterialID kInvalidMaterial = UINT32_MAX;

// Optional per-section draw packets produced by geometry upload bridge.
// Allows one node to submit multiple material sections without ad-hoc slicing.
struct MeshSectionDrawPacket {
    uint32_t firstIndex = 0;
    uint32_t indexCount = 0;
    MaterialID material = kInvalidMaterial;
};

// ── Node ─────────────────────────────────────────────────────────────────────
class Node {
public:
    using NodeID = uint64_t;
    static inline constexpr NodeID kInvalidNode = UINT64_MAX;

    explicit Node(std::string name, NodeID id);

    // ── Hierarchy ──────────────────────────────────────────────────────────
    void            addChild   (std::unique_ptr<Node> child);
        [[nodiscard]] Node*           parent     () noexcept       { return m_parent; }
        [[nodiscard]] const Node*     parent     () const noexcept { return m_parent; }
        [[nodiscard]] std::vector<std::unique_ptr<Node>>& children() noexcept { return m_children; }

    // ── Transform ──────────────────────────────────────────────────────────
        [[nodiscard]] Transform&       localTransform()       noexcept { return m_local; }
        [[nodiscard]] const Transform& localTransform() const noexcept { return m_local; }
    [[nodiscard]] Mat4 worldMatrix()  const noexcept;

    // ── Renderable payload ─────────────────────────────────────────────────
    MeshRef    mesh;
    std::vector<MeshSectionDrawPacket> sectionDrawPackets;
    MaterialID material   = kInvalidMaterial;
    bool       visible    = true;
    bool       castShadow = true;

    // ── Identity ───────────────────────────────────────────────────────────
    [[nodiscard]] NodeID            id()   const noexcept { return m_id; }
    [[nodiscard]] const std::string& name() const noexcept { return m_name; }

    // ── Dirty flag for transform cache ────────────────────────────────────
    void markDirty() noexcept;
    [[nodiscard]] bool isDirty() const noexcept { return m_dirty; }
    void clearDirty() noexcept { m_dirty = false; }

private:
    std::string                        m_name;
    NodeID                             m_id;
    Transform                          m_local;
    Node*                              m_parent   = nullptr;
    std::vector<std::unique_ptr<Node>> m_children;
    mutable Mat4                       m_cachedWorld{};
    mutable bool                       m_dirty = true;
};

// ── SceneGraph ────────────────────────────────────────────────────────────────
class SceneGraph {
public:
    SceneGraph();
    ~SceneGraph();

    // ── Node management ────────────────────────────────────────────────────
    [[nodiscard]] Node* createNode(std::string name, Node* parent = nullptr);
    [[nodiscard]] Node* findNode  (Node::NodeID id)  const noexcept;
    [[nodiscard]] Node* findNode  (std::string_view name) const noexcept;
    void  removeNode(Node::NodeID id);
    void  clear();
    void  clearAndDestroyTLAS(nexus::gfx::IDevice& device);

    // Optional ownership hook: when enabled, removeNode() tears down node GPU payloads
    // for the removed subtree via geometry bridge helpers before detaching nodes.
    void enableAutoDestroyNodeGpuPayloadOnRemove(nexus::gfx::IDevice& device) noexcept;
    void disableAutoDestroyNodeGpuPayloadOnRemove() noexcept;
    [[nodiscard]] bool autoDestroyNodeGpuPayloadOnRemoveEnabled() const noexcept {
        return m_autoDestroyDevice != nullptr;
    }

    [[nodiscard]] Node& root() noexcept { return *m_root; }

    // ── Traversal ──────────────────────────────────────────────────────────
    using VisitorFn = std::function<void(Node&, const Mat4& worldMatrix)>;
    void traverse(const VisitorFn& visitor) const;

    // ── Frustum-culled renderable collection ──────────────────────────────
    // Fills output with nodes visible in the frustum (used by Renderer).
    void collectVisible(const Frustum& frustum, std::vector<Node*>& output) const;

    // ── TLAS rebuild (after any node transform change) ────────────────────
    // Returns true if rebuild was necessary.
    bool rebuildTLAS(nexus::gfx::IDevice& device);
    [[nodiscard]] nexus::gfx::AccelStructHandle tlas() const noexcept { return m_tlas; }

private:
    std::unique_ptr<Node>  m_root;
    uint64_t               m_nextId = 1;
    nexus::gfx::AccelStructHandle m_tlas;
    nexus::gfx::IDevice*   m_autoDestroyDevice = nullptr;
    // O(1) handle→pointer index — updated by createNode / removeNode
    std::unordered_map<Node::NodeID, Node*> m_nodeIndex;

    void traverseImpl(Node& node, const Mat4& parentWorld, const VisitorFn& visitor) const;
};

} // namespace nexus::render
