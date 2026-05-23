---
description: Build, test, and review the current Nexus-Modeling working diff via the code-reviewer agent
argument-hint: "[optional scope/focus, e.g. 'sim' or 'lifetime safety']"
---

Review the current Nexus-Modeling changes before commit. Optional focus: **$ARGUMENTS**

Current diff:

!`cd "$(git rev-parse --show-toplevel)" && git status --short -- apps/Nexus-Modeling && echo "---" && git -c core.pager=cat diff --stat -- apps/Nexus-Modeling`

1. Build and run the suite to establish ground truth:
   `cmake --build build --target nexus_kernel_tests -j$(nproc)` then
   `./build/tests/nexus_kernel_tests`. Report build warnings and pass/fail counts.
2. Delegate the diff to the `code-reviewer` agent (pass along any focus from $ARGUMENTS).
3. Relay the reviewer's severity-rated findings. For each Critical/High, route the fix to
   the owning domain engineer, then re-build and re-run to confirm green.
4. Do NOT commit or push unless the user asks.
