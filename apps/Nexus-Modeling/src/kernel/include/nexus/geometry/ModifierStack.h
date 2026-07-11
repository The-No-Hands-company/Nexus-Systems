#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  Nexus Geometry — Modifier Stack (non-destructive modeling core)
//
//  An ordered list of operations ("modifiers") applied to a base mesh and
//  re-evaluated on demand. The base mesh is never mutated — evaluate() returns
//  a fresh result each call, so edits to the stack or its parameters are fully
//  non-destructive. This is the L5 keystone that lets higher-level features
//  (CAD tree, deformers) be regenerable rather than one-shot.
//
//  Results are cached: evaluated() recomputes only when the stack changed, so
//  repeated reads (e.g. per render frame) are cheap.
// ─────────────────────────────────────────────────────────────────────────────

#include <nexus/geometry/Mesh.h>

#include <cstdint>
#include <vector>

namespace nexus::geometry {

enum class ModifierType : uint8_t {
    Translate,  // vec = offset
    Rotate,     // vec = axis, scalar = angle (radians)
    Scale,      // vec = per-axis factors
    Mirror,     // axis = 0/1/2 plane, merge = weld at plane, scalar = weld epsilon
    Solidify,   // scalar = shell thickness
    Array,      // vec = per-copy step offset, count = number of copies, merge = weld
    Displace,   // scalar = magnitude along vertex normals
};

// A single stack entry. Only the fields relevant to `type` are used; the rest
// are ignored (kept in one struct so the stack is trivially copyable/serialisable).
struct Modifier {
    ModifierType        type    = ModifierType::Translate;
    bool                enabled = true;
    nexus::render::Vec3 vec{};        // offset / rotation axis / scale factors / array step
    float               scalar  = 0.f;  // angle / thickness / weld epsilon / displace magnitude
    int32_t             axis    = 0;     // mirror plane (0=X, 1=Y, 2=Z)
    int32_t             count   = 2;     // array: total number of copies (>= 1)
    bool                merge   = true;  // mirror/array: weld coincident vertices
};

class ModifierStack {
public:
    // Base geometry the stack operates on. Never mutated by evaluate().
    void setBaseMesh(Mesh mesh);
    [[nodiscard]] const Mesh& baseMesh() const noexcept { return m_base; }

    void addModifier(const Modifier& modifier);
    bool insertModifier(size_t index, const Modifier& modifier);
    bool setModifier(size_t index, const Modifier& modifier);  // replace params (for editing)
    bool removeModifier(size_t index);
    bool moveModifier(size_t index, size_t newIndex);
    bool setEnabled(size_t index, bool enabled);
    void clearModifiers() noexcept;

    [[nodiscard]] size_t modifierCount() const noexcept { return m_modifiers.size(); }
    [[nodiscard]] const Modifier& modifierAt(size_t index) const { return m_modifiers.at(index); }
    [[nodiscard]] const std::vector<Modifier>& modifiers() const noexcept { return m_modifiers; }

    // Applies every enabled modifier, in order, to a copy of the base mesh, and
    // returns a fresh copy. Deterministic and non-destructive.
    [[nodiscard]] Mesh evaluate() const;

    // Cached variant: recomputes only when the stack changed since the last call,
    // otherwise returns the stored result. Cheap for repeated reads.
    [[nodiscard]] const Mesh& evaluated() const;

private:
    [[nodiscard]] Mesh computeResult() const;
    void invalidate() noexcept { m_cacheValid = false; }

    Mesh                  m_base;
    std::vector<Modifier> m_modifiers;
    mutable Mesh          m_cache;
    mutable bool          m_cacheValid = false;
};

} // namespace nexus::geometry
