---
name: RFC (Request for Comments)
about: Formal design proposal for a significant language or architecture change
title: "[RFC] "
labels: rfc
assignees: ''
---

## RFC number

_Assigned by maintainers after triage._

## Status

`Draft` | `Active` | `Accepted` | `Rejected` | `Superseded`

## Summary

One paragraph abstract of the proposed change.

## Motivation

What is the problem? Cite prior art, benchmarks, or user reports that demonstrate the
need. Explain why the current design is insufficient.

## Detailed design

### Language changes (if any)

- Grammar additions or modifications (reference `grammar/NLPL.g4`)
- New AST nodes required (`parser/ast.py`)
- Lexer tokens required (`parser/lexer.py`)
- Parser rules (`parser/parser.py`)

### Runtime / interpreter changes (if any)

- New `execute_*` methods in `interpreter/interpreter.py`
- Runtime environment changes (`runtime/runtime.py`, `runtime/memory.py`)
- Type system changes (`typesystem/`)

### Standard library changes (if any)

- New modules or functions in `stdlib/`

### Examples

Show idiomatic NexusLang code that uses the new feature:

```nlpl
# Before (workaround or unsupported)

# After (with this RFC)
```

## Drawbacks

What are the costs of this proposal? Performance? Complexity? Breaking changes?

## Alternatives

What other designs were considered and why were they rejected?

## Unresolved questions

List open questions that must be answered before this RFC can be accepted.

## Implementation plan

Ordered list of pull requests / milestones needed to ship this RFC.
