# Incremental Parsing Design (Draft)

## Goals
- Reduce on-edit latency from ~0.34ms (cached full parse) toward ~0.05ms for single-line edits.
- Reparse only affected regions while preserving unchanged AST and scope info.
- Integrate with existing AST cache; provide safe fallbacks to full parse when needed.

## Principles
- **Correctness first**: incremental result must match full parse.
- **Deterministic**: same input → same AST regardless of edit path.
- **Fail-safe**: if uncertainty arises, fall back to full parse.
- **Bounded complexity**: prefer coarse invalidation over risky fine-grain reuse.

## Change Tracking
- Input: LSP `textDocument/didChange` provides ranges + text.
- Track per-document edit ranges (line/col). Maintain version + hash.
- Compute affected line span: expand to statement/construct boundaries using simple heuristics (see invalidation rules).

## Invalidation Boundaries (initial conservative rules)
- Invalidate whole **function/method body** if an edit touches inside it.
- Invalidate whole **class/struct/union** block if header or body touched.
- Invalidate enclosing **block** (if/while/for/try/match) if edit inside its span.
- If edit touches **import/module** region, reparse entire file (simplify).
- If edit touches **top-level** between constructs, invalidate nearest surrounding constructs; if ambiguous, reparse full file.

## Tokenization Strategy
- Keep token stream slices with start/end offsets.
- On edit: adjust offsets after edit; discard tokens overlapping edit range.
- Retokenize only the edited span plus a small **context window** (e.g., ±2 lines) to capture cross-line tokens (strings, comments).
- Merge: prefix (unchanged tokens) + retokenized segment + suffix (shifted offsets).

## Parsing Strategy
- Maintain parse tree with node spans (start/end line/col) and parent links.
- On edit, find lowest ancestor node whose span intersects the edit range; expand to invalidation boundary per rules.
- Reparse tokens for that span; replace subtree; update parent spans.
- If reparse fails or spans conflict, fall back to full parse.

## Scope & Symbols
- Preserve scope tables for unchanged regions.
- When replacing subtree, recompute scopes within the invalidated span; re-link parents.
- For name resolution/type info, invalidate only affected scopes; reuse outer scopes unchanged.

## AST Cache Interaction
- Cache key remains file + content hash.
- For incremental path, we maintain an in-memory mutable AST and token buffer; on success, store new AST + hash in cache.
- If incremental fails, full parse; cache that result.

## Fallbacks & Safety
- Parsing error or mismatch → full-file reparse.
- Large edit (e.g., >20% lines) → full-file reparse.
- Structural edits (insert/delete brace/`end`) in top-level → full-file reparse.

## API Sketch
- `IncrementalDocumentState` (per URI):
  - `text` (current), `version`, `tokens`, `ast`, `scopes`
- Methods:
  - `apply_edit(range, text)`
  - `retokenize_span(span)`
  - `reparse_span(span, tokens)`
  - `fallback_full_parse()`

## LSP Integration
- `didChange`: apply edit → incremental update; on failure, full parse.
- Diagnostics/completions/hover/refs use latest incremental AST/token stream.
- Expose metrics: incremental vs fallback counts, avg latency.

## Metrics & Targets
- Latency targets (single-line edit): **≤0.1ms** stretch, **≤0.2ms** acceptable.
- Fallback rate: **<5%** under typical editing.
- Memory: reuse existing cache footprint; no more than +20%.

## Phased Implementation Plan
1) Instrument spans: ensure parser emits start/end positions for major nodes; add parent links where missing.
2) Token buffer: store tokens with offsets; implement splice + retokenize window.
3) Boundary finder: map edit range → invalidation span (function/class/block heuristics).
4) Partial parse: parse token slice; graft subtree; update spans/scopes.
5) Fallback logic: robust full-parse path on any uncertainty.
6) LSP wiring: use incremental state in diagnostics/completions/refs.
7) Benchmarks: measure latency, fallback %, correctness vs full parse.

## Open Questions
- How to cheaply detect unmatched `end`/indent changes in partial spans? (possible: quick balance check before accept)
- Do we need gap buffers/rope for text? (likely not initially; rely on LSP text + slicing)
- Scope persistence: minimal structure needed to reuse outer scopes without re-walking?

## Test Plan
- Unit: token splice, span invalidation rules, subtree graft correctness.
- Property: incremental_result == full_parse for random small edits.
- Fuzz: random inserts/deletes around strings/comments/ends.
- Bench: 1-line edits in functions/classes; measure latency.

## Dependencies
- Parser span accuracy; AST parent links; stable token offsets.

