# Nexus Modeling — Tolerance System

*Centralized, scale-aware tolerance management for robust geometric computations.*

---

## Overview

The `Tolerance` system replaces scattered epsilon literals with a **centralized, scale-aware** mechanism. It provides absolute + relative tolerance that scales with the characteristic length of the problem.

---

## Core Type

```cpp
struct Tolerance {
    static constexpr float DEFAULT_ABS = 1e-6f;
    static constexpr float DEFAULT_REL = 1e-4f;

    float abs = DEFAULT_ABS;   // Absolute floor (meters)
    float rel = DEFAULT_REL;   // Relative to characteristic length

    [[nodiscard]] float at(float characteristicLength) const noexcept {
        return std::max(abs, rel * characteristicLength);
    }

    [[nodiscard]] bool nearlyEqual(float a, float b, float charLen) const noexcept {
        return std::abs(a - b) <= at(charLen);
    }

    [[nodiscard]] bool isZero(float x, float charLen) const noexcept {
        return std::abs(x) <= at(charLen);
    }

    [[nodiscard]] bool coincident(const Vec3& a, const Vec3& b, float charLen) const noexcept {
        return (a - b).length() <= at(charLen);
    }
};

Tolerance Tolerance::forCharacteristicLength(float L) noexcept {
    return {1e-6f, 1e-4f};  // Scales with L
}
```

---

## Usage

### Characteristic Length

```cpp
// For a mesh operation, use bounding box diagonal
float charLen = mesh.computeBounds().extents().length();
Tolerance tol = Tolerance::forCharacteristicLength(charLen);

// For a boolean operation, use the larger object's diagonal
float charLen = max(meshA.bounds().diagonal(), meshB.bounds().diagonal());
Tolerance tol = Tolerance::forCharacteristicLength(charLen);
```

### Comparison Helpers

```cpp
Tolerance tol = Tolerance::forCharacteristicLength(L);

// Float comparison
if (tol.nearlyEqual(a, b, L)) { /* equal */ }

// Zero test
if (tol.isZero(value, L)) { /* value is effectively zero */ }

// Point coincidence
if (tol.coincident(p1, p2, L)) { /* points are same */ }

// Distance test
if ((p1 - p2).length() <= tol.at(L)) { /* within tolerance */ }
```

---

## Migration Guide

### Legacy → Tolerance

| Legacy Pattern | Tolerance Equivalent |
|----------------|---------------------|
| `std::abs(a-b) < 1e-5f` | `tol.nearlyEqual(a,b,L)` |
| `len < 1e-6f` | `tol.isZero(len, L)` |
| `(a-b).length() < 1e-5f` | `tol.coincident(a,b,L)` |
| `std::abs(a-b) < 1e-4f * L` | `tol.nearlyEqual(a,b,L)` |
| `dist < 1e-3f * scale` | `dist <= tol.at(L)` |

### Migration Checklist

- [ ] Replace all `1e-6f`, `1e-5f`, `1e-4f` literals
- [ ] Compute characteristic length per operation context
- [ ] Use `Tolerance::forCharacteristicLength(L)` factory
- [ ] Pass `Tolerance` by value (small, trivially copyable)
- [ ] Update tests to verify scale invariance

---

## Configuration

```cpp
// Defaults (in Tolerance.h)
static constexpr float DEFAULT_ABS = 1e-6f;  // 1 micron floor
static constexpr float DEFAULT_REL = 1e-4f;  // 0.01% relative
```

### Customization

```cpp
// High precision (jewelry, micro-machining)
Tolerance tol = {1e-9f, 1e-6f};

// Coarse (architecture, terrain)
Tolerance tol = {1e-3f, 1e-2f};

// Default (general CAD)
Tolerance tol = Tolerance::forCharacteristicLength(L);
```

---

## Migration Checklist

- [ ] Audit all floating-point comparisons
- [ ] Identify characteristic length per operation
- [ ] Replace literals with `Tolerance` calls
- [ ] Add `Tolerance` parameter to public APIs
- [ ] Update tests for scale invariance
- [ ] Add fast-math regression tests

---

## Fast Math Guard

```cpp
// In Tolerance.h
static_assert(!__FAST_MATH__, "Tolerance requires IEEE-754 compliance; -ffast-math breaks it");

// Regression test (in tests/kernel/test_Tolerance.cpp)
TEST(Tolerance, FastMathGuard) {
    #ifdef __FAST_MATH__
    FAIL() << "Tolerance requires strict IEEE-754; compile without -ffast-math";
    #endif
}
```

---

## Migration Example

### Before
```cpp
// File: MeshBoolean.cpp
bool MeshBoolean::classifyVertex(const Vec3& p, const Mesh& other) {
    if (std::abs(dot(p - center, normal)) < 1e-5f) return OnSurface;
    // ...
}
```

### After
```cpp
VertexClass classifyVertex(const Vec3& p, const Mesh& other, const Tolerance& tol, float L) {
    if (tol.coincident(projected, center, L)) return OnSurface;
    // ...
}
```

### Call Sites
```cpp
// Before
if (classifyVertex(p, mesh) == OnSurface) ...

// After
Tolerance tol = Tolerance::forCharacteristicLength(mesh.diagonal());
if (classifyVertex(p, mesh, tol, mesh.diagonal()) == OnSurface) ...
```

---

## Testing Scale Invariance

```cpp
TEST(Tolerance, ScaleInvariance) {
    // Same geometry at 1mm and 1km scale
    Mesh mm = makeBox(10, 10, 10);      // 10mm cube
    Mesh km = makeBox(10000, 10000, 5000); // 10km cube

    Tolerance tol_mm = Tolerance::forCharacteristicLength(mm.diagonal());
    Tolerance tol_km = Tolerance::forCharacteristicLength(km.diagonal());

    // Same relative operations should behave identically
    auto r1 = MeshBoolean::compute(mm, mm, Union);
    auto r2 = MeshBoolean::compute(km, km, Union);

    EXPECT_EQ(r1.ok, r2.ok);
    EXPECT_EQ(r1.result.faceCount(), r2.result.faceCount());
}
```

---

## Fast Math Guard

```cpp
// In Tolerance.h
static_assert(!__FAST_MATH__, "Tolerance requires IEEE-754 compliance; -ffast-math breaks it");

// Regression test
TEST(Tolerance, FastMathGuard) {
    #ifdef __FAST_MATH__
    FAIL() << "Tolerance requires strict IEEE-754; compile without -ffast-math";
    #endif
}
```

---

## Migration Checklist

- [ ] Audit all floating-point comparisons
- [ ] Identify characteristic length per operation
- [ ] Replace literals with `Tolerance` calls
- [ ] Add `Tolerance` parameter to public APIs
- [ ] Update tests for scale invariance
- [ ] Add fast-math regression test

---

*See also: [Mesh Operations](mesh-operations.md) | [Robust Predicates](robust-predicates.md) | [Analytic B-rep](analytic-brep.md)*