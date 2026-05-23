---
description: Drive a Nexus-Modeling kernel feature through the full team pipeline (architect → implement → test → review → verify)
argument-hint: <feature description>
---

You are the tech lead orchestrating the Nexus-Modeling subagent team for this feature:

**$ARGUMENTS**

Run this pipeline from the main thread. Subagents are isolated and cannot ask the user,
so YOU handle any user-facing decisions between stages.

1. **Design.** Delegate to the `software-architect` agent to produce a design (public API
   shape, affected modules + owning engineer, API-freeze-manifest impact, test plan,
   implementation steps). Relay the design to the user and confirm direction before coding.

2. **Route to the owning domain engineer** by module (implement only what the design specifies):
   - `geometry/` → `geometry-engineer`
   - `render/` → `render-engineer`
   - `gfx/`, `backend/`, `memory/` → `gpu-backend-engineer`
   - `sim/` → `simulation-engineer`
   - `parametric/`, `eval/`, `scene/` → `parametric-engineer`
   - `asset/`, `automation/` → `asset-pipeline-engineer`
   - `animation/` → `animation-engineer`
   - `neural/` → `neural-engineer`
   If the change spans modules, sequence the engineers; if a new source/header or CMake
   wiring is needed, use `build-engineer` (and remember the API-freeze manifest).

3. **Tests.** Delegate to `test-engineer` to add/extend deterministic, Null-backend coverage
   (and a regression test if this fixes a bug).

4. **Verify.** Build `cmake --build build --target nexus_kernel_tests -j$(nproc)` and run
   `./build/tests/nexus_kernel_tests`. The suite must be warning-free and green.

5. **Review.** Delegate the working diff to `code-reviewer`. Address Critical/High findings
   (route fixes back to the owning engineer) and re-verify.

6. **Summarize** for the user: what changed, files, manifest impact, and the final test result.
   Do NOT commit or push unless the user asks.
