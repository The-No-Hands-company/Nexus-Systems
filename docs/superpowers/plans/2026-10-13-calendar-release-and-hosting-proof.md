# Calendar Release and Hosting Publish Proof — Implementation Plan

This plan follows the approved design in `docs/superpowers/specs/2026-10-13-calendar-release-and-hosting-proof-design.md`. Work is split into reviewable stages; all integration behavior remains isolated from production.

## Task 1 — Add Nexus-Deploy build-context support

- Add `buildContext` to the project model and API, default old records to `.` and reject absolute paths, traversal, and invalid contexts.
- Pass the resolved context to Docker/Nixpacks while retaining clone cleanup and rollback behavior.
- Add focused tests for defaulting, validation, and context resolution.

## Task 2 — Make Calendar health and registration truthful/configurable

- Add env-configurable tool ID, path, public URL, and integration enablement while preserving current production defaults.
- Probe the service health endpoint with a bounded timeout before sending Cloud health; validate the returned identity/readiness.
- Report healthy/degraded accurately and make `/health` and `/api/v1/status` agree on identity and readiness.
- Add mocked Cloud/health tests for healthy, malformed, timeout, and failed-probe cases.

## Task 3 — Build the isolated release lifecycle integration check

- Create a temporary local Git source and disposable configuration for Nexus-Deploy, Cloud, proxy, and the service.
- Exercise the Deploy API through first release, verify Cloud route/health and proxy request, deploy a changed version, and roll back.
- Guarantee teardown of temporary resources on success and failure; add loopback/disposable-target guards and clear prerequisites.
- Include an unhealthy/failed deployment assertion.

## Task 4 — Prove Hosting website publish separately

- Use the existing Hosting API/test harness to create a disposable user and static site, upload a tiny HTML fixture, publish it, and fetch the served content.
- Assert user ownership and that the test does not go through Nexus-Deploy; clean up temporary records/files.

## Task 5 — Run focused gates and document invocation

- Run Calendar `bun run check && bun test`.
- Run Cloud, Deploy, proxy, and Hosting package-local checks/tests; run Tunnel tests only if integration code touches its contracts.
- Run the lifecycle and Hosting checks locally where Docker/test infrastructure permits; record skipped prerequisites explicitly.
- Document the safe invocation, cleanup guarantees, and the distinction between Cloudflare Tunnel production ingress and the Nexus-Tunnel app.
