# AST-based Find References Plan (Draft)

## Goals

- Zero false positives/negatives by using parsed AST instead of regex.
- Scope-aware: distinguish symbols with same name in different scopes.
- Reuse existing cached ASTs; avoid re-tokenizing when possible.

## Approach

1) **AST traversal**: walk Program tree; collect identifiers by kind:
   - Function definitions/calls
   - Class/struct/union definitions/instantiations/type annotations
   - Method definitions/calls
   - Variable declarations/assignments/references (Identifier nodes)
2) **Position mapping**: ensure all relevant AST nodes carry start/end (line, col).
3) **Scope filtering**: attach scope id to identifiers; match only compatible scopes when searching.
4) **Symbol resolution**: leverage interpreter/parser symbol tables if available; otherwise simple scope stack during traversal.
5) **Caching**: use cached AST (from cached_parser) for each document; no extra parse cost.

## Matching Rules

- Input: URI, position → determine symbol + kind by inspecting AST node at position.
- For variables: match same scope or outer-scope bindings; exclude inner shadowed names.
- For functions/methods: match by name; include definitions + call sites.
- For classes: include definitions, instantiations (`new Class()`), type annotations (`as Class`).

## Data Structures

- `SymbolOccurrence`: {uri, kind, name, scope_id, range}
- `Scope`: {id, parent_id, symbols}
- Index built per document on demand (quick walk) and cached alongside AST version/hash.

## Integration Steps

1) Add AST-position helpers to locate node at (line, col).
2) Implement per-document symbol index builder from AST.
3) Update ReferencesProvider to: lookup symbol-at-pos via AST, query index across docs.
4) Preserve regex path as fallback if AST missing.
5) Update tests to assert no false positives (e.g., keywords, comments, strings).

## Tests

- Existing LSP Test 9 cases ported to AST path.
- Add shadowing scenario: inner `counter` should not match outer.
- Add comment/string negative cases.
- Cross-file matching for functions/classes.

## Performance Expectations

- AST traversal per document: O(nodes), negligible vs parse; reused across requests.
- No measurable latency increase; improved accuracy.

## Open Items

- Ensure all Identifier/FunctionCall/MemberAccess nodes have accurate spans.
- Decide on lightweight scope builder vs reuse interpreter symbol tables.


