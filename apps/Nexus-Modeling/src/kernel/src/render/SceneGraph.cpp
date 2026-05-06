// ─────────────────────────────────────────────────────────────────────────────
//  SceneGraph implementation stub
// ─────────────────────────────────────────────────────────────────────────────
#include <nexus/render/SceneGraph.h>
#include <nexus/geometry/GeometryKernel.h>
#include <algorithm>
#include <cmath>

namespace nexus::render {

// ── Transform ────────────────────────────────────────────────────────────────
Mat4 Transform::toMatrix() const noexcept
{
    // Quaternion to rotation matrix, then apply scale and translation.
    const float& qx = rotation.x, &qy = rotation.y, &qz = rotation.z, &qw = rotation.w;
    const float& sx = scale.x,    &sy = scale.y,     &sz = scale.z;

    float xx = qx*qx, yy = qy*qy, zz = qz*qz;
    float xy = qx*qy, xz = qx*qz, yz = qy*qz;
    float wx = qw*qx, wy = qw*qy, wz = qw*qz;

    Mat4 m{};
    m.m[0][0] = sx * (1 - 2*(yy+zz));
    m.m[0][1] = sx * 2*(xy - wz);
    m.m[0][2] = sx * 2*(xz + wy);
    m.m[1][0] = sy * 2*(xy + wz);
    m.m[1][1] = sy * (1 - 2*(xx+zz));
    m.m[1][2] = sy * 2*(yz - wx);
    m.m[2][0] = sz * 2*(xz - wy);
    m.m[2][1] = sz * 2*(yz + wx);
    m.m[2][2] = sz * (1 - 2*(xx+yy));
    m.m[0][3] = translation.x;
    m.m[1][3] = translation.y;
    m.m[2][3] = translation.z;
    m.m[3][3] = 1.f;
    return m;
}

// ── Node ─────────────────────────────────────────────────────────────────────
Node::Node(std::string name, NodeID id)
    : m_name(std::move(name)), m_id(id)
{}

void Node::addChild(std::unique_ptr<Node> child)
{
    child->m_parent = this;
    m_children.push_back(std::move(child));
}

Mat4 Node::worldMatrix() const noexcept
{
    if (!m_dirty) return m_cachedWorld;
    Mat4 local = m_local.toMatrix();
    m_cachedWorld = m_parent ? (m_parent->worldMatrix() * local) : local;
    m_dirty = false;
    return m_cachedWorld;
}

void Node::markDirty() noexcept
{
    m_dirty = true;
    for (auto& c : m_children) c->markDirty();
}

// ── SceneGraph ────────────────────────────────────────────────────────────────
SceneGraph::SceneGraph()
    : m_root(std::make_unique<Node>("__root__", 0))
{}

SceneGraph::~SceneGraph() = default;

Node* SceneGraph::createNode(std::string name, Node* parent)
{
    auto node  = std::make_unique<Node>(std::move(name), m_nextId++);
    Node* ptr  = node.get();
    Node* par  = parent ? parent : m_root.get();
    par->addChild(std::move(node));
    m_nodeIndex[ptr->id()] = ptr;   // O(1) fast lookup
    return ptr;
}

Node* SceneGraph::findNode(Node::NodeID id) const noexcept
{
    auto it = m_nodeIndex.find(id);
    return it != m_nodeIndex.end() ? it->second : nullptr;
}

Node* SceneGraph::findNode(std::string_view name) const noexcept
{
    Node* result = nullptr;
    traverse([&](Node& n, const Mat4&) {
        if (n.name() == name) result = &n;
    });
    return result;
}

void SceneGraph::enableAutoDestroyNodeGpuPayloadOnRemove(nexus::gfx::IDevice& device) noexcept
{
    m_autoDestroyDevice = &device;
}

void SceneGraph::disableAutoDestroyNodeGpuPayloadOnRemove() noexcept
{
    m_autoDestroyDevice = nullptr;
}

void SceneGraph::removeNode(Node::NodeID id)
{
    Node* node = findNode(id);
    if (!node || node == m_root.get()) return;  // cannot remove root

    if (m_autoDestroyDevice) {
        auto destroyPayloadRecursive = [this](auto& self, Node* n) -> void {
            if (!n) {
                return;
            }
            nexus::geometry::GeometryRenderBridge::destroyNodeMeshPayload(*m_autoDestroyDevice, *n);
            for (auto& child : n->children()) {
                self(self, child.get());
            }
        };
        destroyPayloadRecursive(destroyPayloadRecursive, node);
    }

    // Remove from flat index (this node + all descendants)
    node->markDirty();
    auto removeFromIndex = [this](auto& self, Node* n) -> void {
        m_nodeIndex.erase(n->id());
        for (auto& child : n->children())
            self(self, child.get());
    };
    removeFromIndex(removeFromIndex, node);

    // Detach from parent's children list
    Node* parent = node->parent();
    if (parent) {
        auto& children = parent->children();
        children.erase(
            std::remove_if(children.begin(), children.end(),
                [node](const std::unique_ptr<Node>& c) { return c.get() == node; }),
            children.end());
    }
}

void SceneGraph::clear()
{
    std::vector<Node::NodeID> childrenToRemove;
    childrenToRemove.reserve(m_root->children().size());
    for (const auto& child : m_root->children()) {
        childrenToRemove.push_back(child->id());
    }
    for (const Node::NodeID id : childrenToRemove) {
        removeNode(id);
    }
}

void SceneGraph::clearAndDestroyTLAS(nexus::gfx::IDevice& device)
{
    clear();
    if (m_tlas.valid()) {
        device.destroyAccelStruct(m_tlas);
        m_tlas = {};
    }
}

void SceneGraph::traverse(const VisitorFn& visitor) const
{
    traverseImpl(*m_root, Mat4::identity(), visitor);
}

void SceneGraph::traverseImpl(Node& node, const Mat4& parentWorld, const VisitorFn& visitor) const
{
    Mat4 world = parentWorld * node.localTransform().toMatrix();
    visitor(node, world);
    for (auto& child : node.children())
        traverseImpl(*child, world, visitor);
}

void SceneGraph::collectVisible(const Frustum& frustum, std::vector<Node*>& output) const
{
    // Sphere-frustum test (fast conservative cull).
    // Uses the world matrix to derive a bounding sphere center.
    traverse([&](Node& node, const Mat4& world) {
        if (!node.visible) return;
        if (!node.mesh.vertexBuffer.valid()) return;  // non-renderable node

        // Center of bounding sphere approximation from world translation
        float cx = world.m[0][3], cy = world.m[1][3], cz = world.m[2][3];
        // Conservative radius: take max scale component × 1 (unit mesh assumed)
        float radius = 1.f;

        bool inside = true;
        for (const auto& plane : frustum.planes) {
            float dist = plane.x*cx + plane.y*cy + plane.z*cz + plane.w;
            if (dist < -radius) { inside = false; break; }
        }
        if (inside) output.push_back(&node);
    });
}

bool SceneGraph::rebuildTLAS(nexus::gfx::IDevice& device)
{
    std::vector<nexus::gfx::AccelStructHandle> blases;
    traverse([&](Node& node, const Mat4&) {
        if (node.mesh.blas.valid()) blases.push_back(node.mesh.blas);
    });
    if (blases.empty()) return false;

    if (m_tlas.valid()) device.destroyAccelStruct(m_tlas);
    m_tlas = device.buildTLAS(blases);
    return true;
}

} // namespace nexus::render
