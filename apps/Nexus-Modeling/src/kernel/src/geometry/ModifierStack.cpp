#include <nexus/geometry/ModifierStack.h>

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

}  // namespace

void ModifierStack::setBaseMesh(Mesh mesh)
{
    m_base = std::move(mesh);
}

void ModifierStack::addModifier(const Modifier& modifier)
{
    m_modifiers.push_back(modifier);
}

bool ModifierStack::insertModifier(size_t index, const Modifier& modifier)
{
    if (index > m_modifiers.size()) {
        return false;
    }
    m_modifiers.insert(m_modifiers.begin() + static_cast<std::ptrdiff_t>(index), modifier);
    return true;
}

bool ModifierStack::removeModifier(size_t index)
{
    if (index >= m_modifiers.size()) {
        return false;
    }
    m_modifiers.erase(m_modifiers.begin() + static_cast<std::ptrdiff_t>(index));
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
    return true;
}

bool ModifierStack::setEnabled(size_t index, bool enabled)
{
    if (index >= m_modifiers.size()) {
        return false;
    }
    m_modifiers[index].enabled = enabled;
    return true;
}

void ModifierStack::clearModifiers() noexcept
{
    m_modifiers.clear();
}

Mesh ModifierStack::evaluate() const
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
        }
    }
    return result;
}

}  // namespace nexus::geometry
