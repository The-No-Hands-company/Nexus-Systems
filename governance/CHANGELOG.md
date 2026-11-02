# Nexus Production Changelog

All entries describe released production behavior. Planned work belongs in
`BACKLOG.md`, not here.

## 2026-11-01

### Changed

- Released Nexus Calendar, Nexus Draw, and Nexus Project in the dashboard
  catalog after their healthy runtime records were verified.
- Advanced the Nexus Cloud catalog seed so these releases persist across
  control-plane restarts.

### Known state

- Nexus Chat remains excluded from the registry-projected launcher while its
  heartbeat is stale; the dashboard's existing native fallback remains
  available.
- Nexus Mail P1 lifecycle Task 4 implementation (bounded SMTP-owned Trash
  purge loop, aggregate-only purge logging, and restart/recovery coverage) is
  complete in code on this branch; production release evidence remains pending
  staging journey completion and founder approval.
