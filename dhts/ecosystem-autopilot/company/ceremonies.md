# Nexus Ceremonies

## Daily Control Loop (15 min)

1. Run `/brief`.
2. Run `/plan`.
3. Dispatch one task per ready specialist lane.
4. Run `/pipeline daily-backbone --dry-run` to confirm command paths.

## Weekly Architecture and Drift Review (45 min)

1. Run `/org`.
2. Run `/gate pre-merge --dry-run`.
3. Run `/pipeline docs-sync`.
4. Record architecture decisions in docs and backlog.

## Release Readiness Review (60 min)

1. Confirm no critical blocked tasks remain in target lanes.
2. Run `/gate release`.
3. Run `/pipeline release-readiness`.
4. Record outcomes and rollback instructions in release notes.

## Incident Response Ceremony

1. Pause new starts in affected lane.
2. Dispatch incident owner role explicitly with `/dispatch <role> --start`.
3. Run verification workflows for impacted surfaces.
4. Add postmortem task with acceptance and rollback criteria.
