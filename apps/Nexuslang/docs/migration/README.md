# Migration Guides

Practical guides for developers coming to NexusLang from other languages.

| Guide | Best for |
|-------|----------|
| [From Python](from-python.md) | Scripting, data work, web backends |
| [From Rust](from-rust.md) | Systems programming, performance-critical code |
| [From C / C++](from-c-cpp.md) | Embedded, OS-level, real-time, low-level hardware |

## Common Themes Across All Guides

- NexusLang uses **natural English keywords** rather than punctuation (`set x to 5`, not `x = 5`)
- Every block ends with an explicit `end` keyword (no braces or indentation rules)
- Types are written **after** the identifier: `x as Integer`, not `int x`
- Calling a function with arguments uses `with` and `and`: `f with a: 1 and b: 2`
- The standard library is split into small modules; import what you need: `import math, io, network`

See the [language reference](../reference/syntax.md) for the complete grammar.
