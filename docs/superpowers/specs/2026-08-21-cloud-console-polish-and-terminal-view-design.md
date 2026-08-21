# Cloud Console polish and shell-native Terminal

**Date:** 2026-08-21
**Status:** approved design, ready for an implementation plan

## Goal

Finish the existing shell-native Cloud Console integration, then add a
shell-native `/terminal` workspace backed by Nexus-Terminal's authenticated
host PTY. The terminal is available only to `founder` and `admin` users.

## Scope

Work proceeds in this order:

1. Close defects and polish the five existing Cloud Console views.
2. Harden the terminal service's authorization contract.
3. Add the Dashboard's narrow same-origin WebSocket relay.
4. Build and connect the shell-native `/terminal` view.
5. Add production startup/configuration and verify the integrated path.

Cloud Console work is limited to completing the existing design. It includes
response-contract correctness, state handling, responsive behavior,
accessibility, and test coverage. It does not add new operator actions or
expand Cloud's control-plane API.

## Existing foundation

The Dashboard already owns the shell, launcher, Cloud views, Auth proxy, and a
strict read-only Cloud proxy. The Cloud Console lives under `/cloud/*` and has
five views: overview, tools, federation, identity, and API.

Nexus-Terminal already exposes a real host shell at
`/api/v1/terminal/attach`. It uses a WebSocket connected to a PTY created via
`script`, has an explicit enable switch, an eight-session ceiling, idle
reaping, and an audit database. The host-shell risk and containment trade-off
remain documented in `docs/TERMINAL-SECURITY.md`.

The frontend already declares xterm.js and its fit addon as uncommitted user
work. Implementation will preserve and complete those dependency changes
rather than replace them with a second terminal renderer.

## Cloud Console completion

Each Cloud view will be checked against Nexus-Cloud's current response types,
not legacy `status.html` assumptions. Contract adapters remain centralized in
the Dashboard frontend API module. Views must distinguish loading, empty,
unavailable, and ready states; optional card failures may degrade locally,
while load-bearing request failures produce an explicit page-level state.

Polish includes:

- accurate labels and counts based on current Cloud semantics;
- usable tables and navigation at narrow widths;
- semantic headings, tables, navigation, alerts, and status text;
- keyboard-visible interactive elements and non-color status cues;
- consistent Nexus design tokens and shell spacing;
- focused unit tests for real payload shapes and every state transition.

No Cloud mutation is added. The Dashboard's Cloud proxy stays a fixed,
read-only allow-list, and the Cloud API key never reaches the browser.

## Terminal architecture

### Route and presentation

`/terminal` is a Dashboard-owned React route rendered inside the existing
`ShellView`. It is not an iframe and does not introduce a public
`terminal.tnhc.dev` host.

The shell-native app registry gains a Nexus Terminal entry at `/terminal` only
when Auth identifies the app-list caller as `founder` or `admin`. Its health
comes from a bounded loopback probe of Nexus-Terminal's `/health` endpoint. As
with Mail, the native entry wins over any Cloud registry record with the same
id or destination. Other roles receive neither the native tile nor a registry
tile that could reveal the internal service.

### Same-origin WebSocket relay

The browser opens only this endpoint:

```text
wss://app.tnhc.dev/api/terminal/attach?cols=<n>&rows=<n>
```

The Dashboard accepts WebSocket upgrades on exactly that path. It validates
the caller through Nexus-Auth and requires a role of `founder` or `admin`
before contacting Nexus-Terminal. It then opens a loopback WebSocket to
Nexus-Terminal and relays text/binary frames and close/error state in both
directions.

The relay forwards the caller's session cookie only to the configured
loopback Nexus-Terminal upstream. It is not a generic proxy, does not accept an
upstream from the client, and exposes no additional Terminal API paths.

Nexus-Terminal independently sends that cookie to Nexus-Auth and repeats the
`founder`/`admin` check before spawning a PTY. Both boundaries fail closed if
Auth is unavailable. The existing `NEXUS_TERMINAL_ENABLED=true` switch remains
mandatory.

### Data flow

```text
Browser /terminal
  -> Dashboard /api/terminal/attach
       -> Auth /api/v1/auth/me (founder/admin check)
       -> Nexus-Terminal /api/v1/terminal/attach on loopback
            -> Auth /api/v1/auth/me (independent founder/admin check)
            -> audited host PTY
```

This design keeps browser traffic same-origin, avoids widening the frontend
Content Security Policy to another host, and leaves two independently tested
authorization checks between the internet and the host shell.

## Terminal interaction design

### Workspace

The selected layout is a hybrid of a tab-ready terminal and an operator panel:

- the terminal occupies the full content width by default;
- a top bar contains live session tabs, close controls, and a `+` action;
- a slim footer shows connection state, audit state, and session age;
- a Details action opens an on-demand right drawer rather than permanently
  narrowing the terminal.

The drawer shows the current session's authenticated role, state, start time,
audit notice, and an explicit Disconnect action. It contains no secret values,
raw cookies, or internal credentials.

### Multiple sessions

The first authorized visit creates one terminal tab automatically. `+` creates
another independent WebSocket and PTY. Every tab owns its terminal instance,
fit addon, socket, connection state, and timestamps. Switching tabs preserves
the inactive tab's buffer and live connection.

Closing a tab closes its socket and kills its PTY. Navigating away or unmounting
the view closes all sockets. If the last tab is closed, the empty workspace
offers a clear New session action. Reloading starts fresh sessions; sessions
are not persisted or reattached across page loads.

### Terminal dimensions

xterm.js fits the browser canvas and sends initial columns and rows in the
attach URL. The current `script`-based PTY uses those initial dimensions but
cannot accept live `TIOCSWINSZ`. Browser resizes still refit the renderer, but
the interface and documentation must not claim server-side PTY resizing.
Adding node-pty or another resize-capable PTY is separate work.

## Authorization and safety

- `/terminal` renders an explicit founder/admin requirement for other roles
  and makes no attach attempt.
- Dashboard rejects missing sessions, disallowed roles, non-WebSocket methods,
  and every path outside the one fixed attach route.
- Nexus-Terminal independently requires `founder` or `admin` and refuses to
  spawn a PTY when disabled or when Auth cannot validate the caller.
- Terminal output is written only through xterm.js APIs and is never inserted
  as HTML.
- Each PTY remains owned by one authenticated subject and one socket.
- Existing session ceilings, idle reaping, start/input/exit auditing, and
  shutdown cleanup remain enforced.
- Socket URLs, frontend bundles, application logs, and view state contain no
  transport credentials: API keys, cookies, or bearer tokens. Nexus-Terminal's
  deliberate keystroke audit remains unchanged and may record secrets an
  operator types into the shell; the UI and security documentation must state
  that consequence plainly.

## Failure handling

Each terminal tab represents one of these states:

- connecting;
- connected;
- disconnected;
- access refused;
- terminal disabled;
- service unavailable;
- session limit reached.

A failed tab reports its reason and offers to start a new PTY. It does not
close or reset healthy sibling tabs. An upstream failure closes the paired
browser socket with a meaningful close code where possible. Cleanup is
idempotent so simultaneous component unmount, socket close, and PTY exit cannot
leak a process or duplicate an audit end event.

Cloud failures stay isolated to Cloud views; Terminal failures stay isolated
to `/terminal`; neither failure prevents the rest of the shell from loading.

## Production integration

Nexus-Terminal is started on loopback by the production deploy flow before the
Dashboard. Configuration supplies its Auth and Cloud loopback URLs, Cloud API
key, base URL used for registration, and the explicit enable switch. Dashboard
receives only the configured loopback Terminal URL.

Nexus-Terminal continues registering and heartbeating with Cloud as an
unexposed internal tool. The Dashboard creates the clickable `/terminal`
entry itself, so Cloud registration does not need a public URL and cannot make
the host shell independently internet-addressable.

## Testing and verification

### Cloud Console

- API adapters parse current Cloud status, trust, tools, peers, identity, and
  endpoint payloads.
- Every view covers loading, ready, empty, partial degradation where allowed,
  and unavailable states.
- Navigation, tables, alerts, status text, and narrow-screen behavior are
  asserted at the component level.
- The proxy remains GET-only, own-property allow-listed, role-aware where
  required, and never exposes its API key.

### Dashboard relay and route

- non-upgrade, signed-out, and non-admin requests never contact Terminal;
- founder/admin requests reach only the configured loopback attach endpoint;
- cookies are forwarded upstream but never returned or logged;
- frames, binary data, close codes, and upstream errors relay correctly;
- the `/terminal` route renders inside the shell and unauthorized users do not
  construct a WebSocket;
- native app merging produces exactly one `/terminal` entry with truthful
  health.

### Nexus-Terminal

- both `founder` and `admin` can attach when explicitly enabled;
- every other role, missing session, and Auth outage fail closed;
- session ceiling, idle cleanup, socket cleanup, and audit lifecycle remain
  covered;
- independent PTYs prove multiple tabs do not share input or output.

### Frontend

- initial tab creation, additional tabs, switching, close, disconnect, empty
  workspace, and details drawer behavior;
- per-tab state and buffer isolation;
- renderer fitting and initial dimension propagation;
- component unmount closes every socket;
- terminal bytes are written through xterm.js without HTML interpretation.

### Quality gates

Run the focused Dashboard frontend/backend suites, Nexus-Terminal's required
`bun run check && bun test`, production proxy/deploy tests affected by startup
changes, and a real local WebSocket smoke test through the Dashboard relay.

## Out of scope

- new Cloud operator actions or mutations;
- terminal access for ordinary authenticated users;
- a public Terminal hostname;
- a generic Dashboard service/WebSocket proxy;
- persistent or reconnectable browser terminal sessions;
- shared/collaborative PTYs;
- server-side live resize support;
- containerizing or otherwise containing the host shell;
- AI command assistance, SSH target management, file transfer, or command
  autocomplete.

## Acceptance criteria

1. All existing Cloud views match current Cloud contracts and pass their
   state, responsive, and accessibility tests.
2. `/terminal` is visible and attachable only to `founder` and `admin` users.
3. Dashboard and Nexus-Terminal independently enforce that role restriction.
4. Multiple terminal tabs run isolated, audited host PTYs and clean up on
   close or navigation.
5. The Hybrid 1 layout is implemented: full terminal, tabs, status footer, and
   on-demand details drawer.
6. No Cloud key, Auth credential, cookie, or arbitrary upstream route is
   exposed to browser code.
7. Nexus-Terminal stays loopback-only, explicitly enabled, registered with
   Cloud as internal, and started by the production workflow.
8. Targeted quality gates and the end-to-end relay smoke test pass.
