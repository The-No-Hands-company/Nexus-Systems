# Runtime Architecture and Language Strategy

## Problem Statement

Large scenes, high object counts, and complex simulation workloads require stronger runtime performance characteristics than a purely scripting-driven approach.

## Proposed Direction

Adopt a hybrid runtime architecture:

- High-level orchestration, tooling, and fast iteration in TypeScript.
- Performance-critical core systems in Rust (preferred) or another suitable systems language.

## Why Rust (Primary Candidate)

- Strong safety guarantees without garbage collector pauses.
- Good fit for data-oriented and multithreaded workloads.
- Mature ecosystem for native and WebAssembly targets.
- Better long-term maintainability than ad-hoc C/C++ bindings for this project profile.

## Alternatives

- C++: high performance with broader integration complexity and memory safety risks.
- Zig: promising ergonomics, smaller ecosystem footprint.
- C#: strong tooling, runtime/JIT tradeoffs depending on deployment targets.

## Hybrid Layering Model

- Layer 1: Frontend/editor runtime (TypeScript)
- Layer 2: Script runtime adapters and command translation
- Layer 3: Native simulation core (Rust candidate)
- Layer 4: Backend-specific rendering/physics adapters

## Candidate Native-Core Responsibilities

- Transform hierarchy evaluation
- Culling and broadphase scene partitioning
- Animation pose solving and blending hot loops
- Physics stepping integration and collision query acceleration
- Pathfinding and crowd simulation

## Integration Options

- WebAssembly module for browser deployment.
- Native dynamic/static libraries for desktop/server targets.
- FFI contract with explicit serialization and shared schema versioning.

## Data Contract Strategy

- Versioned schemas for cross-language communication.
- Explicit ownership and lifecycle boundaries.
- Deterministic update order between script and native systems.

## Migration Plan (Phased)

1. Keep full TS baseline and profiling harness.
2. Extract one hotspot subsystem behind interface boundaries.
3. Implement Rust prototype behind the same interface.
4. Compare correctness and performance against baseline.
5. Expand to additional subsystems only after measurable gains.

## Decision Gate Criteria

- Performance improvement on representative stress scenes.
- Acceptable complexity in build and deployment pipelines.
- Stable debugging and profiling workflows.
- No unacceptable regressions in iteration speed.

## When To Shift To Rust

The shift should start when all of the following are true:

1. Baseline profiling exists for representative stress scenes and is reproducible in CI/local runs.
2. At least one subsystem consistently consumes a large share of frame time (for example, more than about 20-30% of frame CPU under stress).
3. The subsystem boundary is already interface-driven and can be swapped without changing gameplay semantics.
4. Correctness can be validated with existing tests plus parity checks.
5. Build and debug workflows for Rust/WASM or native target are ready and documented.

The shift should not start if performance data is missing or if boundary contracts are still unstable.

## Rust vs TypeScript Ownership Matrix

### Keep in TypeScript (default unless profiling proves otherwise)

- Editor UX and tooling workflows
- High-level orchestration and feature composition
- Scripting host adapters and gameplay glue logic
- UI/HUD logic and non-hotpath content workflows
- Rapid-iteration gameplay prototypes

### Move to Rust first (high-value native candidates)

- Transform hierarchy solve at large scale
- Culling/spatial partitioning hot loops
- Physics step and heavy collision query workloads
- Animation pose solve/blend hot loops
- Pathfinding/crowd simulation workloads
- Any subsystem with deterministic, data-oriented, CPU-heavy frame costs

### Conditional (depends on profiling and deployment target)

- Audio DSP chains and heavy mixer processing
- Asset decompression/processing pipelines
- Networking serialization/compression hot paths

## Migration Order Recommendation

1. Select one hotspot subsystem with strong interface boundaries.
2. Implement Rust module behind the existing contract.
3. Validate parity with deterministic tests and scene benchmarks.
4. Keep rollback path (feature flag or adapter switch).
5. Expand to next subsystem only after measurable net gain.

## Exit Criteria Per Migrated Subsystem

- Correctness parity against TS baseline.
- Measured performance gain on representative scenes.
- No unacceptable regression in iteration speed or debugging clarity.
- Documented operational workflow (build, run, debug, profile).
