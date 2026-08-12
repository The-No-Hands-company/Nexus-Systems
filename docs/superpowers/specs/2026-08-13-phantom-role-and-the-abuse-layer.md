# What Phantom is for, and what it cannot do

**Date:** 2026-08-13
**Status:** findings + recommendation, not yet a plan

Phantom was described as the ecosystem's single security substrate — the thing
guarding the edge at `tnhc.dev`, every hop through the proxy, and the inside of
each app. Reading the protocol and its integration says that is two different
jobs, and Phantom is only one of them.

---

## 1. Phantom is a privacy transport, not a firewall

From its own README: *"Protocol for Hardened Anonymous Networking Through
Oblivious Mathematics."* Fully homomorphic encryption for routing metadata,
zk-SNARK proofs of correct routing, post-quantum primitives throughout.

Its central design property is stated plainly: **"Nodes route packets they
literally cannot decrypt."**

That is the opposite of content inspection. A firewall or antivirus works by
looking at traffic; Phantom's entire value is that looking is impossible. You
cannot scan what you cannot decrypt. This is not an integration gap to close —
it is a category difference, and no amount of wiring changes it.

What Phantom is genuinely for, per its own "Role In Nexus Systems" section:

- control-plane traffic
- service-to-service communication
- edge and federation networking
- secrets and trust-boundary exchanges

That is real and it matters here, because federation is the answer to
single-machine availability. Private, unlinkable, post-quantum links between
nodes is exactly what a federated ecosystem run from someone's home needs.

## 2. The other job still has no owner

Nothing currently in the request path does any of this:

- inspects payloads for malicious content
- detects abuse patterns or automated scraping
- rate-limits by behaviour rather than by route
- constrains a bad actor **holding valid credentials**

Everything hardened in SSO phase 4 — loopback binds, the login gate, identity
tokens, the WebSocket relay — is authentication and authorisation. It proves
who you are and whether you may enter. It says nothing about what you do once
inside, and a stolen or legitimately-held credential passes all of it.

This layer needs its own answer. Phantom will not provide it, and asking it to
would mean weakening the one property that makes Phantom worth having.

## 3. What Phantom enforces today: nothing

`packages/phantom-sdk` is imported by **82 server files** across the ecosystem.
Until this commit, all 82 were running a stand-in:

- `loadWasm()` threw unconditionally — the real path was unreachable even if
  the module *had* been built.
- A bare `catch {}` swallowed that and returned `createMockSDK()`.
- The mock returns `did:phantom:mock:<hash>`, `"ab".repeat(800)` as a
  "Kyber-1024 public key", `"cd".repeat(2592)` as a "Dilithium-5 key", and
  signatures that are the message repeated.
- `PhantomApp.status()` reported `algorithms: "Kyber-1024, Dilithium-5, Blake3"`
  regardless — health endpoints across the ecosystem asserted post-quantum
  protection that was not there.

The failure mode was the worst available one: **open and silent**. Counterfeit
cryptography substituted for real cryptography while every caller reported
success. Absence would have been safer, because absence is visible.

Fixed now: `loadWasm()` actually attempts the load, the fallback prints an
unmissable banner, `PhantomApp.status()` reports `cryptography: "mock"` and
`algorithms: "none (mock)"`, the boot line says the identity is not
cryptographically real, and `PhantomSDK.isMock` lets any caller ask.
`PHANTOM_REQUIRE_REAL=1` refuses to start on the mock.

**That switch is off by default on purpose.** 82 services call this at boot;
turning it on before the module builds would take the ecosystem down rather
than secure it. Turn it on per service as each is verified, and globally once
the build is part of deployment.

## 4. Why the module has never built

Two blockers, one now fixed:

**getrandom (fixed).** `getrandom 0.3` on `wasm32-unknown-unknown` selects its
backend by cfg, not by feature alone. Without
`--cfg getrandom_backend="wasm_js"` it compiles no backend and fails inside its
own internals. Now set in `wasm/.cargo/config.toml`, with the feature enabled,
so it works for anyone who runs the build rather than living in someone's shell
history.

**pqcrypto (open).** `pqcrypto-internals` compiles C — `nistseedexpander.c`
includes `<stdlib.h>` — and `wasm32-unknown-unknown` has no libc. This is not
configuration; the target genuinely has no C standard library.

```
fatal error: 'stdlib.h' file not found
```

### The framing was wrong anyway

Every one of the 82 consumers is a `src/server.ts` — **server-side Bun, not a
browser**. A browser-targeted WASM bundle was never what these needed, which is
also why nobody noticed it had never been built.

Two honest paths, both real work:

| Path | What it means | Cost |
|---|---|---|
| **Native library + `bun:ffi`** | Build the same Rust as a cdylib and call it from Bun. pqcrypto's C compiles natively without complaint. | Needs a C-ABI surface; the crate is `#[wasm_bindgen]`-only today. Fastest, no cross-compilation. |
| **`wasm32-wasip1` + wasi-sdk** | The target the repo's own CI already uses, and it has a libc. | Needs the wasi-sdk toolchain and a WASI loader in Bun. Portable, heavier. |

A third option — swapping `pqcrypto` for pure-Rust ML-KEM/ML-DSA — compiles
anywhere with no C, but would put the browser SDK on different implementations
from the native side and risk keys that do not interoperate. Not worth it
unless both sides move together.

## Recommendation

1. **Do not wire the proxy to `phantomProtectionLevel` yet.** Cloud computes
   `securityTag` and `phantomProtectionLevel` and ships both on every route,
   and the proxy discards them — that observation is correct. But enforcing a
   verdict derived from a profile that nothing produces is enforcement theatre.
   Make Phantom real first, then enforce it.
2. **Pick a path from the table** and make one service — Draw is the smallest —
   run real Kyber/Dilithium end to end. Then `PHANTOM_REQUIRE_REAL=1` on that
   one service.
3. **Treat the abuse layer as a separate project** with its own spec. It is not
   Phantom, it cannot be Phantom, and conflating them leaves the actual gap
   unowned.

## Also worth knowing

- `hosting.tnhc.dev` and `storage.tnhc.dev` are routed by the tunnel straight
  to their origins, bypassing the proxy from the public internet. Anything
  enforced at the proxy — Phantom included — will not see them until that
  ingress moves.
- The apex is Cloudflare Pages and never reaches the node at all, so nothing
  enforced on this machine can run in that path as currently architected.
- Upstream Phantom's own `docs/STATUS.md` claims Phase 2 complete with measured
  figures (46 ms proof generation, 13 ms verification **per hop**), but it is
  dated February 2026 and six months stale. Note the per-hop verification cost:
  on a request path that is a latency budget, not a free win.
