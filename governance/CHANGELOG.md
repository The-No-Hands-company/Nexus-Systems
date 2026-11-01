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
