# Nexus Engine

Status: Active development
Blueprint source: Nexus Systems Ecosystem Blueprint v1.3

Nexus Engine is an open-source, general-purpose 2D/3D game engine initiative for high-scale real-time worlds. The current implementation is browser-first, with an ARPG reference game used as a proving ground for engine systems.

## Vision

- Build an engine that scales to extremely dense scenes and large model counts.
- Keep gameplay systems modular and reusable across genres.
- Support both node-based visual scripting and text scripting workflows.
- Evolve toward a high-performance native core (Rust-first candidate) while preserving developer-friendly tooling.

## Current Stack

- Runtime prototype: TypeScript + Babylon.js + Vite
- Architecture direction: hybrid model where performance-critical systems move to native modules (Rust or another fitting systems language)

## Quick Start

1. Install dependencies: `npm install`
2. Start dev server: `npm run dev`
3. Type-check: `npm run type-check`
4. Run tests: `npm test -- --run`
5. Build: `npm run build`

Important: Run commands from this directory:
`apps/Nexus-Engine`

## Documentation Index

- Design document: [docs/DESIGN.md](docs/DESIGN.md)
- Runtime architecture and language strategy: [docs/ARCHITECTURE-RUNTIME.md](docs/ARCHITECTURE-RUNTIME.md)
- Scripting systems (visual + text): [docs/SCRIPTING.md](docs/SCRIPTING.md)
- Animation systems: [docs/ANIMATION.md](docs/ANIMATION.md)
- Physics systems: [docs/PHYSICS.md](docs/PHYSICS.md)
- Audio engine: [docs/AUDIO.md](docs/AUDIO.md)
- Full systems map: [docs/SYSTEMS-LANDSCAPE.md](docs/SYSTEMS-LANDSCAPE.md)
- Development roadmap: [docs/ROADMAP.md](docs/ROADMAP.md)

Note: The module-specific documents are entry points, not the full scope ceiling. Nexus Engine development includes many additional subsystems captured in the systems landscape document.

## Open Source Project Documents

- Contribution guide: [CONTRIBUTING.md](CONTRIBUTING.md)
- Code of conduct: [CODE_OF_CONDUCT.md](CODE_OF_CONDUCT.md)
- Security policy: [SECURITY.md](SECURITY.md)
- Agent workflow rules: [AGENTS.md](AGENTS.md)

## Near-Term Milestones

1. Finalize the engine design baseline and module boundaries.
2. Lock the hybrid runtime plan for Rust/native acceleration and TS orchestration.
3. Ship first pass of animation graph and controller runtime.
4. Ship first pass of robust physics integration and deterministic stepping model.
5. Define scripting MVP for visual graphs plus Lua/Python/C# text scripts.
6. Define and begin implementation of core audio engine architecture.
7. Expand roadmap coverage for wider engine subsystems (AI, navigation, VFX, tooling, serialization, and more).

## Repository Layout

- `src/engine`: core engine runtime and editor foundations
- `src/systems`: extracted gameplay and engine systems
- `src/game`: reference game orchestration layer
- `src/tools`: authoring and helper tools
- `src/types`: domain models and shared type contracts
- `tests`: unit and orchestration tests
- `docs`: architecture and design documentation
