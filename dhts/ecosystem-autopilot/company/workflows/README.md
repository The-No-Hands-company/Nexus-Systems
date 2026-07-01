# Workflow Templates

These workflow templates mirror the autonomous operating model but are stored in `dhts` as helper assets.

To activate in repository CI:

1. Copy selected templates into `.github/workflows/`.
2. Ensure runtime dependencies are available on runner images (`python3`, `bun`, `node`, `npm`, and any tool-specific prerequisites).
3. Start with dry-run variants and enable full execution once command environments are stable.

Templates:

- `nexus-autopilot-heartbeat.yml`: scheduled posture and readiness visibility.
- `nexus-release-gate.yml`: manual release confidence and readiness enforcement.
