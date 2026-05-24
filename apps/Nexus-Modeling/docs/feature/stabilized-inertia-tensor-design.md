# Stabilized Inertia-Tensor Rigid-Body Solver — Design

## Status

Design (architect). Implements roadmap month-13 #5 (remaining half). Targets the
simulation kernel `RigidBodySolver` in `src/kernel/{include/nexus,src}/sim/SimulationCore.*`.

## Motivation

The angular model today uses a single **scalar** moment of inertia
(`SimBodyDesc::inertia : float`), so angular acceleration is `τ / I` about every
axis. That treats every body as a uniform sphere. A best-in-class DCC needs true
rigid-body rotation: anisotropic bodies (boxes, rods, discs) that **precess and
tumble** correctly — including the intermediate-axis (Dzhanibekov) effect.

The physically correct model is Euler's equation with a full inertia tensor:

```
I_body · ω̇_body + ω_body × (I_body · ω_body) = τ_body
```

The cross term `ω × (Iω)` is the **gyroscopic term**. It is what produces
precession/tumbling — and it is **stiff**: integrating `ω̇ = I⁻¹(τ − ω×(Iω))`
with explicit Euler injects energy and diverges (elongated bodies, large `dt`).
This is why naive explicit integration is unacceptable and why PhysX/Bullet
historically disabled or specially handled gyroscopic forces.

## Chosen approach — angular-momentum integration

Integrate **angular momentum L** in world space rather than ω directly:

- Under zero torque, world angular momentum is conserved, so `‖L‖` is constant by
  construction — the integrator cannot blow up the way explicit ω does.
- Torque integrates momentum linearly: `L += τ·dt` (no stiff term to integrate).
- ω is recovered from L each step through the *rotating* world inertia tensor,
  which is exactly what produces precession/tumbling — no explicit gyroscopic
  term is needed; it falls out of `I_world` changing as the body rotates.

Because the body-space tensor is **diagonal** in its principal frame
(`I_body = diag(Ix, Iy, Iz)`), the world tensor multiply/inverse need only
quaternion vector-rotation + component-wise scale — no 3×3 matrix assembly:

```
L_world      = R · ( I_body ⊙ (Rᵀ · ω_world) )
ω_world      = R · ( (Rᵀ · L_world) ⊘ I_body )
```

(`R` = orientation as rotation; `⊙`/`⊘` = component-wise mul/div by (Ix,Iy,Iz).)

### Per-step angular update (per dynamic body)

```
R          = orientation                              // start-of-step
ω_body     = invRotate(R, angularVelocity)
L_world    = rotate(R, ω_body ⊙ inertia)              // L = I_world · ω
L_world   += torque · dt                              // torque integrates momentum
ω_world    = rotate(R, invRotate(R, L_world) ⊘ inertia)   // ω = I_world⁻¹ · L
angularVelocity = ω_world · dampingFactor(angularDamping, dt)
orientation     = integrateOrientation(R, angularVelocity, dt)   // existing helper
// torque cleared per step / once per stepFixed call (unchanged policy)
```

Linear integration (gravity + force + linear damping) is **unchanged**.

### Optional refinement (future, not in first cut)

For extreme inertia ratios at large `dt`, add Bullet's implicit gyroscopic
correction (one Newton step on `ω₂ − ω₁ + I⁻¹(ω₂ × Iω₂)·dt = 0`,
Jacobian `I + dt(skew(ω)I − skew(Iω))`). The momentum form already prevents
blow-up, so this is a precision refinement, gated behind a flag if added.

## Backward compatibility (key property)

When the principal moments are equal (`Ix = Iy = Iz = I`), the momentum update
reduces **exactly** to the current scalar model: `L = I·ω`, `L += τ·dt`,
`ω = L/I = ω + (τ/I)·dt`. So:

- Existing scalar-inertia behavior is preserved for `inertia = {I, I, I}`.
- The **angular state stays ω** (world angular velocity). The snapshot keeps
  storing `orientation` + `angularVelocity` — **no serialization format change;
  SimState stays v2**. Inertia is a body *property* (like mass/damping) and is not
  serialized, so `restoreState` is unaffected.

## API / state changes

- `SimBodyDesc::inertia` and `RigidBodySolver::Body::inertia`: **`float` → `SimVec3`**
  (principal moments in body space, default `{1,1,1}`). This is a breaking change to
  a field added earlier this milestone; acceptable in alpha (no external consumers).
- New file-local helpers in `SimulationCore.cpp` (anonymous namespace):
  `rotateByQuat(q, v)` and `invRotateByQuat(q, v)` via the standard
  `t = 2·cross(q.xyz, v); v' = v + q.w·t + cross(q.xyz, t)` form (deterministic,
  no `quatMul` of a pure-vector quat), plus a small `cross()` on `SimVec3`.
- No new public header → **no API-freeze manifest change**.

## Determinism & build constraints

- All single-precision, fixed operation order → deterministic and Null-backend safe.
- Build is `-ffast-math`: validate finiteness with IEEE-754 bit tests
  (`isFiniteVec3`/`isFiniteFloat`), never `std::isfinite`; keep the existing
  exact-endpoint handling in `integrateOrientation`/`normalizeQuat`.

## Validation / hardening

- `addBody` rejects an inertia with any non-finite or **≤ 0** principal moment
  (replacing the current scalar `inertia <= 0` check, applied per component).
- Per-step runtime guard already rejects non-finite body state; extend it to the
  `SimVec3` inertia.

## Test plan (deterministic, Null backend)

- **Isotropic equivalence:** `inertia = {2,2,2}` reproduces the current scalar-2
  torque result (regression bridge to existing behavior).
- **Angular-momentum conservation:** zero torque, anisotropic inertia, spin about a
  non-principal axis → `‖L_world‖` constant over many steps (tight tolerance) while
  ω direction changes (precession).
- **Energy bounded:** rotational KE `½ ωᵀ I_world ω` stays bounded over a long run
  (no blow-up) — the property naive explicit integration fails.
- **Principal-axis stability:** pure spin about a principal axis stays on that axis
  (no spurious precession).
- **Validation:** reject non-finite / non-positive principal moments.
- **Determinism:** identical setup → identical trajectory.

## Migration impact (files)

- `src/kernel/include/nexus/sim/SimulationCore.h` — `inertia` field type; `Body` field.
- `src/kernel/src/sim/SimulationCore.cpp` — `addBody` validation + emplace; new
  helpers; `step` and `stepFixed` angular blocks.
- `tests/kernel/test_SimulationCore.cpp` — update the few tests setting
  `desc.inertia` to the `SimVec3` form (`TorqueAcceleratesAngularVelocityThenClears`,
  `AddBodyRejectsNonPositiveInertia`); add the new tensor tests above.
- `docs/month-13-rt-and-simulation-coupling.md` — mark #5 fully delivered.

## Risks & alternatives

- **Long-run energy drift:** the momentum integrator conserves `‖L‖` exactly but can
  drift energy slightly over very long runs; bounded and non-explosive. The implicit
  gyroscopic correction (above) is the mitigation if ever needed.
- **Large `dt`:** recommend `stepFixed` with small substeps for fast-spinning,
  high-aspect-ratio bodies; substep angular integration already exists.
- **Alternative considered — store L as state:** rejected; it would change the
  snapshot format (v3) and the public angular accessor for no stability gain over
  recomputing L from ω each step.
