// ─────────────────────────────────────────────────────────────────────────────
//  SceneGraph implementation
// ─────────────────────────────────────────────────────────────────────────────
#include <nexus/render/SceneGraph.h>
#include <nexus/geometry/GeometryKernel.h>
#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdint>

namespace nexus::render {

namespace {

[[nodiscard]] bool isFiniteFloat(float value) noexcept
{
    const std::uint32_t bits = std::bit_cast<std::uint32_t>(value);
    return (bits & 0x7F800000u) != 0x7F800000u;
}

[[nodiscard]] float finiteOr(float value, float fallback) noexcept
{
    return isFiniteFloat(value) ? value : fallback;
}

[[nodiscard]] Transform sanitizeTransform(const Transform& in) noexcept
{
    Transform out{};
    out.translation.x = finiteOr(in.translation.x, 0.f);
    out.translation.y = finiteOr(in.translation.y, 0.f);
    out.translation.z = finiteOr(in.translation.z, 0.f);

    const bool rotationFinite = isFiniteFloat(in.rotation.x)
        && isFiniteFloat(in.rotation.y)
        && isFiniteFloat(in.rotation.z)
        && isFiniteFloat(in.rotation.w);
    if (rotationFinite) {
        out.rotation = in.rotation;
    } else {
        out.rotation = {0.f, 0.f, 0.f, 1.f};
    }

    out.scale.x = finiteOr(in.scale.x, 1.f);
    out.scale.y = finiteOr(in.scale.y, 1.f);
    out.scale.z = finiteOr(in.scale.z, 1.f);
    return out;
}

[[nodiscard]] float axisScaleLength(const Mat4& world, int column) noexcept
{
    const float x = world.m[0][column];
    const float y = world.m[1][column];
    const float z = world.m[2][column];
    return std::sqrt(x * x + y * y + z * z);
}

[[nodiscard]] float conservativeWorldRadius(const Mat4& world) noexcept
{
    // Unit-sphere mesh assumption scaled by the largest transformed basis axis.
    // This remains conservative for non-uniform and mirrored transforms.
    const float sx = axisScaleLength(world, 0);
    const float sy = axisScaleLength(world, 1);
    const float sz = axisScaleLength(world, 2);
    return std::max({sx, sy, sz, 1e-6f});
}

} // namespace

// ── Transform ────────────────────────────────────────────────────────────────
Mat4 Transform::toMatrix() const noexcept
{
    const Transform sanitized = sanitizeTransform(*this);

    // Quaternion to rotation matrix, then apply scale and translation.
    const float& qx = sanitized.rotation.x;
    const float& qy = sanitized.rotation.y;
    const float& qz = sanitized.rotation.z;
    const float& qw = sanitized.rotation.w;
    const float& sx = sanitized.scale.x;
    const float& sy = sanitized.scale.y;
    const float& sz = sanitized.scale.z;

    float xx = qx*qx, yy = qy*qy, zz = qz*qz;
    float xy = qx*qy, xz = qx*qz, yz = qy*qz;
    float wx = qw*qx, wy = qw*qy, wz = qw*qz;

    // M = T * R * S: the scale is applied first, in the object's local space, so it
    // multiplies COLUMN j of the rotation by s_j (the upper 3x3 is R * S). Scaling by the
    // row index instead builds S * R — scaling world axes after the rotation — which is
    // wrong for any non-uniform scale combined with a rotation, and is also inconsistent
    // with how the engine recovers scale (conservativeWorldRadius reads s_j as the length
    // of world-matrix column j, which only holds for R * S).
    Mat4 m{};
    m.m[0][0] = sx * (1 - 2*(yy+zz));
    m.m[0][1] = sy * 2*(xy - wz);
    m.m[0][2] = sz * 2*(xz + wy);
    m.m[1][0] = sx * 2*(xy + wz);
    m.m[1][1] = sy * (1 - 2*(xx+zz));
    m.m[1][2] = sz * 2*(yz - wx);
    m.m[2][0] = sx * 2*(xz - wy);
    m.m[2][1] = sy * 2*(yz + wx);
    m.m[2][2] = sz * (1 - 2*(xx+yy));
    m.m[0][3] = sanitized.translation.x;
    m.m[1][3] = sanitized.translation.y;
    m.m[2][3] = sanitized.translation.z;
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
    traverse([&](Node& node, const Mat4& world) {
        if (!node.visible) return;
        if (!node.mesh.vertexBuffer.valid()) return;  // non-renderable node

        bool inside;
        if (node.hasLocalBounds) {
            // Tight test: transform the node's local AABB into world space.
            inside = frustum.intersectsAabb(node.localBounds.transformed(world));
        } else {
            // Conservative fallback: bounding sphere from the world translation.
            const Vec3 center{ world.m[0][3], world.m[1][3], world.m[2][3] };
            const float radius = conservativeWorldRadius(world);
            inside = frustum.intersectsSphere(center, radius);
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
