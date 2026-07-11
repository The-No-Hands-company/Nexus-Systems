#include <nexus/geometry/ModifierStack.h>

#include <nexus/geometry/MeshDisplace.h>
#include <nexus/geometry/MeshThicken.h>
#include <nexus/geometry/MeshTransform.h>

#include <algorithm>
#include <utility>

namespace nexus::geometry {

namespace {

// Mirror across an axis-aligned plane through the origin: reflect a copy of the
// mesh, reverse its winding (a negative-determinant scale flips handedness, so
// faces must be re-wound to keep normals outward), append it to the original,
// and optionally weld the coincident vertices along the mirror plane.
Mesh applyMirror(const Mesh& in, int axis, bool merge, float epsilon)
{
    nexus::render::Vec3 s{1.f, 1.f, 1.f};
    if (axis == 0)      s.x = -1.f;
    else if (axis == 1) s.y = -1.f;
    else                s.z = -1.f;

    const Mesh reflected = MeshTransform::scale(in, s);

    // Rebuild the reflected topology with reversed winding.
    Mesh fixed = reflected;
    fixed.topology().clearFaces();
    for (size_t f = 0; f < reflected.topology().faceCount(); ++f) {
        Face face = reflected.topology().face(f);
        std::reverse(face.indices.begin(), face.indices.end());
        fixed.topology().addFace(std::move(face));
    }

    Mesh out = in;
    (void)out.appendMesh(fixed);
    if (merge) {
        (void)out.weldCoincidentVertices(epsilon > 0.f ? epsilon : 1e-4f);
    }
    return out;
}

// Linear array: the original plus (count-1) copies, each stepped by `offset`.
Mesh applyArray(const Mesh& in, const nexus::render::Vec3& offset, int count, bool merge, float epsilon)
{
    Mesh out = in;
    const int copies = count < 1 ? 1 : count;
    for (int i = 1; i < copies; ++i) {
        const nexus::render::Vec3 step{offset.x * static_cast<float>(i),
                                       offset.y * static_cast<float>(i),
                                       offset.z * static_cast<float>(i)};
        const Mesh instance = MeshTransform::translate(in, step);
        (void)out.appendMesh(instance);
    }
    if (merge && copies > 1) {
        (void)out.weldCoincidentVertices(epsilon > 0.f ? epsilon : 1e-4f);
    }
    return out;
}

}  // namespace

void ModifierStack::setBaseMesh(Mesh mesh)
{
    m_base = std::move(mesh);
    invalidate();
}

void ModifierStack::addModifier(const Modifier& modifier)
{
    m_modifiers.push_back(modifier);
    invalidate();
}

bool ModifierStack::insertModifier(size_t index, const Modifier& modifier)
{
    if (index > m_modifiers.size()) {
        return false;
    }
    m_modifiers.insert(m_modifiers.begin() + static_cast<std::ptrdiff_t>(index), modifier);
    invalidate();
    return true;
}

bool ModifierStack::setModifier(size_t index, const Modifier& modifier)
{
    if (index >= m_modifiers.size()) {
        return false;
    }
    m_modifiers[index] = modifier;
    invalidate();
    return true;
}

bool ModifierStack::removeModifier(size_t index)
{
    if (index >= m_modifiers.size()) {
        return false;
    }
    m_modifiers.erase(m_modifiers.begin() + static_cast<std::ptrdiff_t>(index));
    invalidate();
    return true;
}

bool ModifierStack::moveModifier(size_t index, size_t newIndex)
{
    if (index >= m_modifiers.size() || newIndex >= m_modifiers.size()) {
        return false;
    }
    if (index == newIndex) {
        return true;
    }
    const Modifier m = m_modifiers[index];
    m_modifiers.erase(m_modifiers.begin() + static_cast<std::ptrdiff_t>(index));
    m_modifiers.insert(m_modifiers.begin() + static_cast<std::ptrdiff_t>(newIndex), m);
    invalidate();
    return true;
}

bool ModifierStack::setEnabled(size_t index, bool enabled)
{
    if (index >= m_modifiers.size()) {
        return false;
    }
    m_modifiers[index].enabled = enabled;
    invalidate();
    return true;
}

void ModifierStack::clearModifiers() noexcept
{
    m_modifiers.clear();
    invalidate();
}

Mesh ModifierStack::computeResult() const
{
    Mesh result = m_base;
    for (const Modifier& m : m_modifiers) {
        if (!m.enabled) {
            continue;
        }
        switch (m.type) {
            case ModifierType::Translate:
                result = MeshTransform::translate(result, m.vec);
                break;
            case ModifierType::Rotate:
                result = MeshTransform::rotate(result, Quat::fromAxisAngle(m.vec, m.scalar));
                break;
            case ModifierType::Scale:
                result = MeshTransform::scale(result, m.vec);
                break;
            case ModifierType::Mirror:
                result = applyMirror(result, m.axis, m.merge, m.scalar);
                break;
            case ModifierType::Solidify: {
                ThickenOptions opts;
                opts.thickness = m.scalar;
                result = MeshThicken::solidify(result, opts);
                break;
            }
            case ModifierType::Array:
                result = applyArray(result, m.vec, m.count, m.merge, m.scalar);
                break;
            case ModifierType::Displace: {
                DisplaceOptions opts;
                opts.magnitude = m.scalar;
                result = MeshDisplace::displaceByScalar(
                    result, [](const nexus::render::Vec3&) { return 1.f; }, opts);
                break;
            }
        }
    }
    return result;
}

Mesh ModifierStack::evaluate() const
{
    return computeResult();
}

const Mesh& ModifierStack::evaluated() const
{
    if (!m_cacheValid) {
        m_cache      = computeResult();
        m_cacheValid = true;
    }
    return m_cache;
}

}  // namespace nexus::geometry
