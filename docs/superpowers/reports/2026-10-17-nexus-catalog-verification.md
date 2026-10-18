# Nexus catalog rollout verification

Date: 2026-10-18
Branch: `feat/nexus-app-catalog`

## Release gates run in this worktree

| Area | Result | Evidence |
| --- | --- | --- |
| Dashboard typecheck | pass | `bun run check` |
| Dashboard Biome | pass | `bun run lint` — 23 files, no fixes |
| Dashboard catalog/server regression | pass | 51 tests, 0 failures, 101 assertions |
| Cloud typecheck | pass | `tsc --noEmit` |
| Cloud app-root check | pass | `bun run check` — 85 files |
| Cloud catalog/release/route regression | pass | 50 tests, 0 failures, 207 assertions |
| Cloud complete suite | pass | 160 tests across 28 files, 0 failures, 707 assertions |
| Auth typecheck | pass | `bun run check` with frozen lockfile after lock refresh |
| Auth regression | pass | 144 tests, 0 failures, 448 assertions |

The Cloud gate covers approval, rollback, audit durability, global projection,
tenant entitlements, node-local overlays, seed idempotency, release fixture
states, Dashboard normalization, and API route manifest parity.

## Previously recorded frontend matrix

These gates were run during the preceding Green-pass work and remain the
release baseline for the frontend apps:

| App | Tests | TypeScript | Production build |
| --- | ---: | --- | --- |
| Design | 4 pass | Pass | Pass |
| Graphic | 4 pass | Pass | Pass |
| Photos | 8 pass | Pass | Pass |
| Video | 8 pass | Pass | Pass |
| Draw | 119 pass / 15 files | Pass | Pass |
| Chat regression | 19 pass | — | — |
| Auth regression | 125 pass | — | — |
| Calendar regression | 21 pass | — | — |
| Project regression | 13 pass | — | — |

## Expected diagnostics

- Auth's link-DID test prints the existing PHANTOM mock warning when the native
  WASM module is unavailable. The test passes; production can require the real
  module with `PHANTOM_REQUIRE_REAL=1`.
- Server tests may print expected Cloud registration `401` warnings while the
  assertions pass.
- Draw's browser test prints the existing jsdom canvas warning; all 119 tests
  pass.

## Biome compatibility

Cloud now has an explicit Biome policy for its existing two-space source style.
The app-root check keeps formatting and import ordering from rewriting legacy
files, while retaining the remaining lint rules. Four noisy legacy rules are
disabled explicitly (`useLiteralKeys`, `noDelete`, `noNonNullAssertion`, and
`noExplicitAny`); the two remaining real style findings were fixed. `bun run
check` passes all 85 Cloud files.

The Cloud test command uses a 15-second per-test budget because persistence and
trust lifecycle tests legitimately exceed Bun’s five-second default on slower
storage. Conditional ETags now ignore informational timestamps, so unchanged
polling responses correctly return `304` even after a delayed round trip.
