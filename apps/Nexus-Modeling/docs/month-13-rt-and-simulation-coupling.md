# Track C Simulation Coupling Integration — Month 13 Delivery

## Overview

This delivery completes the stubbed RT renderer integration and establishes the Track C simulation/scene coupling API contract. The work provides a deterministic bridge between simulation state snapshots and render-visible scene transformations.

## Completed Work

### 1. Ray Tracing Renderer Stub Integration

**Files Modified:**
- [src/kernel/include/nexus/render/Renderer.h](src/kernel/include/nexus/render/Renderer.h#L118)
  - Added `enableRayTracingStub()` API.
  - Added RT stats counters: `rayTracingDrawCalls`, `rayPayloads`, `rayHits`.
  - Added `setRayTracingPipeline()` setter.

- [src/kernel/include/nexus/render/RenderGraphValidator.h](src/kernel/include/nexus/render/RenderGraphValidator.h#L42)
  - Added `RenderPassType::RayTracing` enumeration.

- [src/kernel/src/render/Renderer.cpp](src/kernel/src/render/Renderer.cpp)
  - Integrated RT dispatch stub in frame orchestration (`runRayTracingPass`).
  - Added RT validation and frame capture logging for diagnostic exports.
  - Preserved existing GBuffer/composite sequencing guarantees.
  - Null backend uses existing no-op RT support; Vulkan backend will dispatch `traceRays()` when available.

- [src/kernel/src/render/RenderGraphValidator.cpp](src/kernel/src/render/RenderGraphValidator.cpp#L91)
  - Added RayTracing case handler to compile without warnings.

### 2. Track C Simulation/Scene Coupling API

**Files Created:**
- [src/kernel/include/nexus/sim/SimulationCoupling.h](src/kernel/include/nexus/sim/SimulationCoupling.h)
  - Public coupling API: `SimulationSceneBinding`, `SimulationSceneCoupling`.
  - Lightweight state application: `applyState(const SimState&)`.
  - Deterministic binding interface for solver body ↔ scene node mapping.

- [src/kernel/src/sim/SimulationCoupling.cpp](src/kernel/src/sim/SimulationCoupling.cpp)
  - Implementation of scene transform updates from solver snapshots.
  - Position-only coupling for stable integration point.

**Tests:**
- [tests/kernel/test_SimulationCoupling.cpp](tests/kernel/test_SimulationCoupling.cpp)
  - `AppliesBoundBodyPositionToSceneNode`: Validates position binding.
  - `IgnoresUnboundBodies`: Ensures safety for unbound bodies.

### 3. Rigid-Body Angular Dynamics + Rotation Coupling

Extends the rigid-body solver from a point-mass model to one that also tracks
orientation, unblocking the rotation half of full transform coupling.

**Solver ([src/kernel/src/sim/SimulationCore.cpp](src/kernel/src/sim/SimulationCore.cpp)):**
- `SimQuat` orientation + `angularVelocity` + scalar `inertia` on `SimBodyDesc`,
  `SimBodySnapshot`, and the internal body record.
- `applyTorque(id, torque)`: torque accumulation mirroring force; angular
  acceleration = torque / inertia, consumed once per step (and once per
  `stepFixed` call across substeps), then cleared.
- Orientation integrated via first-order quaternion integration
  (`q += 0.5·dt·ω⊗q`) with renormalization each step. Static bodies (mass 0) are
  not integrated, consistent with the linear path.
- `getBodyAngularState(id, outOrientation, outAngularVelocity)` accessor.
- Entry-point validation rejects non-positive/non-finite inertia and non-finite
  orientation / angular velocity, both at `addBody` and per-step.

**Snapshot format (v2):**
- `serializeSimState` now emits orientation (quaternion) + angular velocity per
  body. `deserializeSimState` accepts both v2 and legacy v1 blobs; v1 records
  decode with identity orientation and zero angular velocity. Replay/rollback
  determinism is preserved.

**Coupling ([src/kernel/src/sim/SimulationCoupling.cpp](src/kernel/src/sim/SimulationCoupling.cpp)):**
- `applyState` now drives both node translation and rotation from the snapshot.
  Scale is left untouched (the solver does not model it).

**Tests:** angular spin determinism, torque acceleration + clearing, static-body
invariance, capture/restore of angular state, v2 round-trip, legacy v1 decode,
non-finite rejection, and node-rotation coupling.

### 4. Fixed-Timestep Driver + Render Interpolation (Solver Output Streaming)

Couples the deterministic solver to a variable-rate render loop the way a DCC
runtime must — via the canonical fixed-timestep accumulator pattern (cf.
Gaffer-on-Games "Fix Your Timestep"), **not** a naive per-frame `step → apply`.

**Files Created:**
- [src/kernel/include/nexus/sim/SimulationDriver.h](src/kernel/include/nexus/sim/SimulationDriver.h)
- [src/kernel/src/sim/SimulationDriver.cpp](src/kernel/src/sim/SimulationDriver.cpp)

**Behavior:**
- `SimulationDriver::advance(solver, frameDt)` accumulates real frame time and
  steps the solver at a fixed `dt`, so trajectories are reproducible regardless
  of frame pacing (framerate independence).
- Leftover time is exposed as `alpha()` in [0,1]; `interpolatedState()` blends
  the previous and current solver snapshots (lerp position/velocity, nlerp
  orientation, shortest-path) into a jitter-free transform the coupling applies.
- A configurable substep cap guards against the "spiral of death" on
  pathological frame deltas; backlog beyond the cap is discarded.
- `interpolateSimState(from, to, alpha)` is exposed as a standalone utility
  (bodies matched by id, ordered output).

**Implementation note:** the kernel builds with `-ffast-math`, so finiteness is
checked via IEEE-754 bit inspection (not `std::isfinite`) and quaternion
interpolation short-circuits exact endpoints to avoid approximate-rsqrt drift —
matching the existing `SimulationCore` convention.

**Tests:** fixed-step accounting + alpha, framerate-independence (one big frame
== many small frames, bit-identical solver state), interpolation blending,
nlerp unit-quaternion + exact endpoints, spiral-of-death cap, non-finite frameDt
rejection, reset seeding, and end-to-end driver → coupling → scene node.

### 5. Composite RT Output Merge

Closes the loop on the RT stub: the ray-traced output is now merged into the lit
composite color before present, instead of being dispatched and discarded.

**Files Modified:**
- [src/kernel/include/nexus/render/Renderer.h](src/kernel/include/nexus/render/Renderer.h)
  - `setRayTracingMergePipeline()` setter; `FrameStats::rayMergeDispatches` counter.
- [src/kernel/src/render/Renderer.cpp](src/kernel/src/render/Renderer.cpp)
  - After `traceRays`, when a merge pipeline is bound, transition the color target
    `ColorAttachment → General`, dispatch a compute merge over the image in 8×8 tiles,
    then transition `→ Present`. Recorded in the render graph and frame capture.
- [src/kernel/include/nexus/render/RenderGraphValidator.h](src/kernel/include/nexus/render/RenderGraphValidator.h)
  + [.cpp](src/kernel/src/render/RenderGraphValidator.cpp) — new `RenderPassType::RayTracingMerge`.

**Why a compute dispatch, not a second render pass:** re-opening a render pass on the
color target would clear it (load-op is clear in the backend), wiping the composite.
A compute post-pass blends RT output into the color storage image without a render pass —
also the standard shape for RT reflection/denoise compositing. Gated on the RT pass
running **and** a merge pipeline being set; fully deterministic and Null-backend testable.

**Tests:** `RendererBehavior.RayTracingMergeRunsComputePassAfterTraceRays` (dispatch fires
after `traceRays`, correct 8×8 tile counts, `rayMergeDispatches == 1`); the existing RT
stub test now also asserts no merge when no merge pipeline is bound.

### 6. Build and Manifest Updates

- [src/kernel/CMakeLists.txt](src/kernel/CMakeLists.txt)
  - Added `src/sim/SimulationCoupling.cpp` to `nexus_gfx_core`.

- [tests/CMakeLists.txt](tests/CMakeLists.txt)
  - Registered `kernel/test_SimulationCoupling.cpp` in test suite.

- [tests/kernel/fixtures/api_surface_manifest_alpha_v1.txt](tests/kernel/fixtures/api_surface_manifest_alpha_v1.txt)
  - Added `src/kernel/include/nexus/sim/SimulationCoupling.h` to public API manifest.

## Design Rationale

### RT Stub Path
- **Deterministic:** Null backend no-op preserves reproducibility for CI.
- **Extensible:** Frame capture records pass metadata for debugging; Vulkan will plug `traceRays()` when ready.
- **Non-Regressive:** Preserved GBuffer/composite layout transitions; RT runs post-composite.

### Simulation Coupling
- **Minimal Scope:** Body position only (no rotation, scale, or complex constraints initially).
- **Binding Model:** Explicit `SimulationSceneBinding` prevents accidental updates to unmanaged nodes.
- **Deterministic:** Snapshot-based state application mirrors existing solver contract.
- **Composable:** Allows independent solver stepping and render scene updates.

## Verification Status

### RT Renderer
- [x] Public API additions compile.
- [x] Frame capture logic integrated without breaking existing paths.
- [x] Null backend remains functional.
- [x] RenderGraphValidator handles new pass type.
- [x] RT pass appears in render graph reports and frame capture exports.

### Simulation Coupling
- [x] Public header added to manifest.
- [x] CMake integration complete.
- [x] Unit tests defined (pending full build success).
- [x] API contract follows existing solver snapshot semantics.

## Next Steps (Post-Alpha)

1. **RT Vulkan Integration:** shader binding table construction + dispatch wiring
   **landed** (pipeline → SBT → bind → `vkCmdTraceRaysKHR`); SBT alignment math is
   unit-tested and the wiring is compile-verified. **Runtime verification still pending
   on RT-capable hardware** (tier 2). See
   [docs/feature/vulkan-rt-dispatch.md](feature/vulkan-rt-dispatch.md).
2. ~~**Solver Output Streaming:** Pipeline solver snapshot updates through coupling per-frame.~~
   **Delivered** (§4): `SimulationDriver` — fixed-timestep accumulator with render interpolation.
3. ~~**Rotation/Scale Coupling:** Extend to full transform state if simulation provides orientation.~~
   **Delivered** (rotation): the solver now integrates orientation and the coupling
   applies it. Scale remains out of scope (not modeled by the solver).
4. ~~**Composite RT Output:** Merge ray traced results into final composite.~~
   **Delivered** (§5): compute merge post-pass blends RT output into the composite color.
5. ~~**Angular Damping / Inertia Tensor**~~ **Delivered:**
   - Damping: `SimBodyDesc::linearDamping` / `angularDamping` (per-second decay,
     factor `clamp(1 - damping·dt, 0, 1)` per step and per `stepFixed` substep;
     default 0 preserves prior trajectories; hardened against negative/non-finite).
   - Inertia tensor: `SimBodyDesc::inertia` is now a `SimVec3` of body-space
     principal moments. Rotation integrates via the **angular-momentum
     formulation** (derive `L = I_world·ω`, advance orientation, recover ω from the
     conserved `L` through the rotated tensor) — stable, momentum-conserving, and it
     reproduces precession/tumbling for anisotropic bodies without the stiff explicit
     gyroscopic term. Reduces exactly to the scalar model when the moments are equal;
     the v2 snapshot format is unchanged (ω remains the angular state). See
     [docs/feature/stabilized-inertia-tensor-design.md](feature/stabilized-inertia-tensor-design.md).
   - Optional future refinement: Bullet-style implicit gyroscopic correction for
     extreme inertia ratios at large `dt` (the momentum form already prevents blow-up).

## References

- [Design RT Pipeline (Month 13)](../docs/design-rt-pipeline-month-13.md) — Extended RT implementation plan.
- [Simulation Core API](src/kernel/include/nexus/sim/SimulationCore.h) — Solver snapshot contract.
- [Scene Graph API](src/kernel/include/nexus/render/SceneGraph.h) — Node update interface.
