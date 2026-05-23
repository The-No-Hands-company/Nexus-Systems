---
description: Run the Nexus-Modeling non-finite-input hardening loop on a module or API area
argument-hint: <module or API area, e.g. 'geometry mesh primitives'>
---

Harden the public API entry points in this area against non-finite / malformed input:

**$ARGUMENTS**

This codebase rejects non-finite floats at API boundaries and builds with `-ffast-math`,
so `std::isfinite`/`std::isnan` are unreliable — detection uses IEEE-754 bit tests (see the
helpers in `src/kernel/src/sim/SimulationCore.cpp`).

1. Route to the owning domain engineer for the area (see the routing table in `/feature`).
   Have them:
   - Enumerate the public API entry points and the float/numeric inputs they accept.
   - Add bit-test finiteness guards that reject NaN/Inf (and other invalid values such as
     negative sizes, non-positive inertia, empty paths) at entry, before any state mutation.
2. Delegate to `test-engineer` to add rejection tests feeding
   `std::numeric_limits<T>::quiet_NaN()/infinity()` and asserting safe rejection with no
   state change.
3. Verify: build `nexus_kernel_tests` (warning-free) and run the suite green.
4. Delegate the diff to `code-reviewer` to confirm no `std::isfinite` slipped in and all
   entry points are covered. Summarize; commit only if the user asks.
