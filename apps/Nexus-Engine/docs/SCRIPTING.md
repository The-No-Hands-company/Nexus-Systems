# Scripting Systems

## Scope

Nexus Engine will support both visual scripting and text scripting.

## Goals

- Fast prototyping for designers and technical artists.
- Strong extensibility for advanced gameplay engineering.
- Safe execution boundaries for user-authored scripts.

## Visual Scripting (Planned)

### Model

- Node graph with typed pins.
- Event-driven and data-flow graph modes.
- Reusable subgraphs and macro libraries.

### Engine Integration

- Graph compiler/interpreter to runtime command stream.
- Access to entity/components through stable APIs.
- Editor-time validation and static checks.

### MVP Features

- Event nodes (input, collision, trigger, timer)
- Flow control nodes (branch, loop, sequence)
- Math and transform nodes
- Variable/state nodes
- Script-call bridge nodes

## Text Scripting (Planned)

Target languages for first design pass:

- Lua
- Python
- C#
- Future candidates based on demand and maintenance cost

### Runtime Model

- Language-specific adapters behind one unified script host interface.
- Sandboxed environment with controlled API surface.
- Hot-reload support where possible.

### API Design Principles

- Explicit, versioned script API namespaces.
- Deterministic lifecycle hooks (`on_init`, `on_update`, `on_event`, `on_shutdown`).
- Stable error reporting and script diagnostics.

## Interop Between Visual and Text

- Visual nodes can call script functions.
- Text scripts can emit graph events.
- Shared typed data contract between systems.

## Security and Stability

- Sandbox and permission model for file/network access.
- Execution budget and timeout guards for runaway scripts.
- Crash isolation strategy for script runtime failures.

## Milestones

1. Define unified script host interface.
2. Deliver visual scripting MVP with interpreted graph runtime.
3. Integrate first text language (Lua candidate for lightweight runtime).
4. Add Python and C# adapters behind same host contract.
