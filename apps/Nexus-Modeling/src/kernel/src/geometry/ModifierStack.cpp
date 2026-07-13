#include <nexus/geometry/ModifierStack.h>

#include <nexus/geometry/MeshDisplace.h>
#include <nexus/geometry/MeshThicken.h>
#include <nexus/geometry/MeshTransform.h>

#include <algorithm>
#include <bit>
#include <cstdint>
#include <utility>
#include <vector>

namespace nexus::geometry {

namespace {

constexpr uint32_t kModifierStackFormatVersion = 1;

// -ffast-math makes std::isfinite unreliable; detect via IEEE-754 bit inspection.
bool isFiniteFloat(float v) noexcept
{
    constexpr uint32_t kExpMask = 0x7F800000u;
    return (std::bit_cast<uint32_t>(v) & kExpMask) != kExpMask;
}

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
    // Canonical stable element IDs on the evaluated result, so downstream
    // ID-based selection/history sees a consistent, rebuilt set regardless of
    // which modifiers ran (or none).
    result.rebuildStableElementIds();
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

std::vector<uint8_t> serializeModifierStack(const ModifierStack& stack)
{
    std::vector<uint8_t> out;
    auto putU32 = [&](uint32_t v) {
        out.push_back(static_cast<uint8_t>(v & 0xFF));
        out.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
        out.push_back(static_cast<uint8_t>((v >> 16) & 0xFF));
        out.push_back(static_cast<uint8_t>((v >> 24) & 0xFF));
    };
    auto putF32 = [&](float f) { putU32(std::bit_cast<uint32_t>(f)); };
    auto putI32 = [&](int32_t v) { putU32(static_cast<uint32_t>(v)); };

    putU32(kModifierStackFormatVersion);
    putU32(static_cast<uint32_t>(stack.modifierCount()));
    for (const Modifier& m : stack.modifiers()) {
        out.push_back(static_cast<uint8_t>(m.type));
        out.push_back(m.enabled ? 1u : 0u);
        putF32(m.vec.x);
        putF32(m.vec.y);
        putF32(m.vec.z);
        putF32(m.scalar);
        putI32(m.axis);
        putI32(m.count);
        out.push_back(m.merge ? 1u : 0u);
    }
    return out;
}

bool deserializeModifierStack(const uint8_t* data, size_t size, ModifierStack& out)
{
    if (data == nullptr) {
        return false;
    }
    size_t p = 0;
    auto getU32 = [&](uint32_t& v) -> bool {
        if (p + 4 > size) return false;
        v = static_cast<uint32_t>(data[p]) | (static_cast<uint32_t>(data[p + 1]) << 8)
          | (static_cast<uint32_t>(data[p + 2]) << 16) | (static_cast<uint32_t>(data[p + 3]) << 24);
        p += 4;
        return true;
    };
    auto getF32 = [&](float& f) -> bool {
        uint32_t bits = 0;
        if (!getU32(bits)) return false;
        f = std::bit_cast<float>(bits);
        return isFiniteFloat(f);  // reject non-finite payloads
    };
    auto getI32 = [&](int32_t& v) -> bool {
        uint32_t bits = 0;
        if (!getU32(bits)) return false;
        v = static_cast<int32_t>(bits);
        return true;
    };

    uint32_t version = 0;
    uint32_t count   = 0;
    if (!getU32(version) || version == 0 || version > kModifierStackFormatVersion) {
        return false;
    }
    if (!getU32(count)) {
        return false;
    }
    // Each modifier is 27 bytes; reject counts that can't possibly fit.
    if (static_cast<size_t>(count) * 27u > size - p) {
        return false;
    }

    std::vector<Modifier> parsed;
    parsed.reserve(count);
    for (uint32_t i = 0; i < count; ++i) {
        if (p + 2 > size) return false;
        const uint8_t type = data[p++];
        if (type > static_cast<uint8_t>(ModifierType::Displace)) return false;  // unknown modifier
        Modifier m;
        m.type    = static_cast<ModifierType>(type);
        m.enabled = data[p++] != 0;
        if (!getF32(m.vec.x) || !getF32(m.vec.y) || !getF32(m.vec.z) || !getF32(m.scalar)) return false;
        if (!getI32(m.axis) || !getI32(m.count)) return false;
        if (p + 1 > size) return false;
        m.merge = data[p++] != 0;
        parsed.push_back(m);
    }

    // Commit only on full success — replace the modifier list, keep the base mesh.
    out.clearModifiers();
    for (const Modifier& m : parsed) {
        out.addModifier(m);
    }
    return true;
}

}  // namespace nexus::geometry
