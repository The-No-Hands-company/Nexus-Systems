# Nexus-Terminal: what it is, and what that costs

**Nexus-Terminal runs a real shell on the host, as the user that runs every
Nexus service.** Anything reachable over SSH by that user is reachable from a
browser tab. This was chosen deliberately on 2026-08-21, with the alternatives
on the table. This document exists so the choice stays visible.

## What an attacker gets

Whoever reaches this endpoint with a valid session can:

- read `apps/Nexus-Cloud/.env`, `apps/Nexus-Dashboard/.env` and every other
  credential on the box — the Cloudflare token, the Cloud API key, the GitHub
  issues token, database passwords
- `docker exec` into any container, including the Postgres holding every app's
  data
- edit and run `deploy/production/deploy.sh`
- read the local node's federation private key and sign as this node
- stop every live service

There is no containment. That is not an oversight; it is the shape that was
chosen over a scoped command runner and a containerised shell.

## What actually stands between the internet and that shell

1. **Invite-only accounts.** There is no self-service signup. An operator
   approves each request and the applicant claims it with a code. This is the
   real gate.
2. **A valid Auth session.** The endpoint refuses anything else.
3. **Session ceiling and idle reaping.** At most 8 concurrent shells, each
   killed after 30 minutes, so an abandoned tab does not leave a live shell
   until reboot.
4. **A full audit trail.** Every session start, every byte of input, and every
   exit is recorded with the Auth subject that caused it.

Note what is *not* in that list: origin isolation. The shell endpoint is
same-origin with the dashboard, so **one XSS anywhere on app.tnhc.dev is a
shell on this host**. The per-origin boundary that keeps Draw away from Chat's
cookies does not help here.

## If this becomes uncomfortable

Two changes, in increasing order of effort, neither requiring a rewrite:

- **Restrict by role.** Gate on `founder`/`admin` rather than any signed-in
  user. One condition in the WebSocket upgrade.
- **Move it into a container.** Run the shell in a per-user container with no
  host mount, no docker socket, `--cap-drop ALL` and a memory cap. The
  `spawnShell` interface does not change; only what it spawns does.

## Why `script` and not node-pty

Interactive programs need a pseudo-terminal. Without one there is no job
control, no colour, and anything using readline or curses misbehaves.

`script -qfc` is in util-linux, already present, and allocates a genuine pty —
a shell started this way reports a real `/dev/pts` device. node-pty would need
native compilation on every deploy for the same result.

The cost: `script` has no channel for `TIOCSWINSZ`, so resize is not plumbed
through. `COLUMNS` and `LINES` are set at spawn from the client's size, so a
shell starts correctly sized and does not follow later window changes. Fixing
that properly means node-pty.

## Turning it off

Unset `NEXUS_TERMINAL_ENABLED` (or set it to `false`) and the endpoint refuses
every connection. The service still serves its health and status routes, so
Cloud does not mark the node degraded.
