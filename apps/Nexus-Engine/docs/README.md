# Nexus Engine Documentation

This directory contains architecture and system planning documents for Nexus Engine.

## Core Documents

- [DESIGN.md](DESIGN.md): project-level design goals and subsystem boundaries
- [ARCHITECTURE-RUNTIME.md](ARCHITECTURE-RUNTIME.md): hybrid runtime strategy, including Rust/native acceleration direction
- [SCRIPTING.md](SCRIPTING.md): visual scripting and text scripting plan (Lua, Python, C#, extensible)
- [ANIMATION.md](ANIMATION.md): animation system architecture and milestones
- [PHYSICS.md](PHYSICS.md): robust physics system design and integration strategy
- [AUDIO.md](AUDIO.md): audio engine architecture and milestones
- [SYSTEMS-LANDSCAPE.md](SYSTEMS-LANDSCAPE.md): broader map of engine systems beyond current focused modules
- [ROADMAP.md](ROADMAP.md): phased development plan
- [AUTONOMOUS-WORKFLOW.md](AUTONOMOUS-WORKFLOW.md): canonical autonomous execution loop and quality gates
- [AUTONOMOUS-CYCLES.md](AUTONOMOUS-CYCLES.md): per-cycle milestone tracking log
- [AUTONOMOUS-ROLE-TEMPLATES.md](AUTONOMOUS-ROLE-TEMPLATES.md): role handoff and acceptance templates

## System Family Stub Docs
- [systems/CORE-RUNTIME.md](systems/CORE-RUNTIME.md)
- [systems/GAMEPLAY-INTERACTION.md](systems/GAMEPLAY-INTERACTION.md)
- [systems/CONTENT-AUTHORING.md](systems/CONTENT-AUTHORING.md)
- [systems/SIMULATION.md](systems/SIMULATION.md)
- [systems/MEDIA.md](systems/MEDIA.md)
- [systems/PLATFORM-PRODUCTION.md](systems/PLATFORM-PRODUCTION.md)
- [systems/RELIABILITY-QUALITY.md](systems/RELIABILITY-QUALITY.md)

## Updating Policy
- Update docs in the same change when architecture or system behavior changes.
- Keep milestones and implementation reality aligned.
- Prefer explicit interfaces and migration criteria over vague goals.
