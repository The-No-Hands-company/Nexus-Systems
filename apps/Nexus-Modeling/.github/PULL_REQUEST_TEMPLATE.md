# Pull Request

## Summary

Describe what changed and why.

## Linked Issue

- Closes #

## Kernel Capability Declaration (Required)

Complete all fields. PRs are not review-ready until this section is filled.

- Capability domain:
  - [ ] Geometry
  - [ ] Rendering
  - [ ] Animation
  - [ ] Simulation
  - [ ] Asset and Data
  - [ ] Scripting
  - [ ] Cross-domain (list domains):

- Owner:
  - Primary owner/domain:
  - Secondary owner/domain (if cross-domain):

- Boundary impact:
  - [ ] Public API contract changed (src/kernel/include/nexus)
  - [ ] Internal implementation only (src/kernel/src)
  - [ ] Rendering synchronization/layout transitions changed
  - [ ] Determinism/headless behavior changed
  - [ ] Non-scheduler fallback path touched
  - Notes:

- Required tests:
  - Behavior tests added/updated:
  - Regression tests added/updated:
  - Build gate run: `cmake --build build -j$(nproc)`
  - Test gate run: `ctest --test-dir build --output-on-failure`
  - If anything was not run, explain exactly what and why:

## Risks and Residual Gaps

List known risks, deferred work, or untested paths.

## Documentation

- [ ] Updated docs where needed (README/docs/)
- [ ] Updated docs/kernel-capability-map.md if boundaries or ownership changed
