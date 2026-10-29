# Frontend load and app delivery

This document explains how a multi-app platform like Nexus should combine its
apps in the browser without making each one pay for the weight of the others. It
exists because the launcher's original "frame every app" model does not scale,
and new contributors need to understand why before adding the next app.

## The one distinction that governs everything: browser load vs server load

These are two different costs and they are solved in two different places.

- **Server load** (a node's CPU/RAM). Serving a *frontend* is almost free — it is
  shipping static bytes. One small process, or a CDN, can serve the built
  frontends of every app in the ecosystem. The expensive work on a node is the
  **backends**: the database, the realtime gateway, the voice SFU. That is where
  a node actually sweats.
- **Browser load** (the end user's device). This is independent of how powerful
  the node is or how many nodes are in the federation. It is determined entirely
  by how much JavaScript a single browser tab must download, parse, and keep
  running at once.

The practical consequence, which must be internalised: **adding machines to the
network never makes a heavy app lighter in the browser.** Client weight is an
architecture problem, not a capacity problem. Federation scales the backends; it
does nothing for a tab that is running too much JavaScript.

## Why framing does not scale

The launcher originally mounted each app in an `<iframe>` inside the Dashboard
shell (`delivery: "framed"`). Framing a full application means the browser runs
**two complete applications at once** — two JavaScript runtimes, two network
stacks, two component trees, and for a realtime app its own live WebSocket on
top. Two framed apps is roughly twice the cost; it is the single most
resource-expensive way to combine apps in one tab.

No major multi-app platform does this. Google Workspace, Microsoft 365 and Notion
do **not** nest a complete second application inside the first. Clicking Gmail in
Google's launcher *navigates* the tab to Gmail; the previous page and all of its
memory are discarded.

This was the concrete cause of Chat freezing the browser when framed — made worse
at the time by Chat being served as a Vite **dev** build (unminified, dev-mode
React, live HMR, ~5–10× the weight of a production build), but the framing tax
remains even with a production build.

## How industry platforms actually do it

Two models, in increasing order of effort. Pick deliberately per app.

### 1. Navigation model (simplest, lightest — Google Workspace)

The launcher is a *thin* page: effectively a menu. Activating an app navigates
the tab (or opens a new tab) to that app's own origin. Only one app's runtime is
ever live, and the browser reclaims the previous app's memory on navigation. This
scales to any number of apps for free, because the browser only ever runs one.

In this codebase that is `delivery: "external"`: the tile is a real link to the
app's `publicUrl`, not a framed route.

### 2. Micro-frontend shell with a shared runtime (seamless, more effort)

A shell application loads each app as a *module* and they **share a single
framework runtime** — React/ReactDOM is loaded once and reused by every app,
instead of each app shipping its own copy. This gives the "one seamless window"
feel without the double-runtime cost of framing. Tooling: Webpack Module
Federation, or `single-spa`. This is the real answer to "can apps share
resources" — yes, at the *runtime* level, not by becoming one monolithic build.

## Should apps on the same stack share a build?

No — a single monolithic build couples every app's deploy and bloats every page.
What is shared is narrower and more effective:

- **A component library.** `packages/nexus-design` is this layer; every app
  builds against it, so UI code is reused at source and the shared chunk is
  cached once by the CDN.
- **Shared vendor chunks / a shared runtime.** React and other large
  dependencies should be loaded **once** and reused across apps — via an import
  map or a shared CDN URL so the browser cache deduplicates them, or via Module
  Federation which shares the live runtime outright.

Separate builds, shared library and shared runtime. Not one giant build.

## Can a heavy app be made as light as a simple one?

Not in absolute terms. Chat genuinely does more than Draw — realtime messaging,
presence, voice — and that work has a floor. What *is* removable:

- **The framing tax** — do not run it inside another app.
- **Eager loading of rarely-used features.** Code-split so, e.g., the
  voice/WebRTC bundle is not downloaded until a user joins a voice channel.
- **Rendering everything at once.** Virtualise long lists (messages, members) so
  only visible rows are in the DOM. This is how Discord and Slack stay usable
  with huge histories.

## How the server side should be structured

- **Frontends:** build each app to static assets and serve them from a single
  static server, or push them to the edge (Cloudflare Pages/assets) so a node
  serves *zero* frontend bytes. A node does not need one heavy process per
  frontend. `apps/Nexus/packages/nexus-web/serve.ts` is the current static server
  for Chat; several such frontends can sit behind one process or one CDN.
- **Backends:** this is the real load. Keep services **stateless** so any node
  can run any of them, **pool** database connections, and **cache** aggressively.
  Scale by running more replicas of the hot services, and place heavy services
  (voice SFU, database) on the strongest nodes. Federation fits this well:
  different nodes host different backends, and services move to where capacity is.

## What this means for Nexus, concretely

1. **Heavy apps use `delivery: "external"`, not `"framed"`.** Chat, Hosting and
   any other full standalone SPA open in their own tab. The launcher stays
   featherweight and nothing doubles up in one tab. This is the current
   recommended default for any app that has its own `publicUrl` and its own
   substantial runtime.
2. **Shell-native views stay in the shell.** Lightweight surfaces that are part
   of the Dashboard SPA itself (e.g. Mail at `/mail`, the Cloud console views)
   are `delivery: "shell-native"` and remain inside the shell — they do not carry
   a second runtime, so there is nothing to externalise.
3. **If the seamless single-window feel is wanted later,** invest in Module
   Federation with `nexus-design` as the shared component layer and a shared React
   runtime — not more iframes.
4. **Always:** static frontends off the origin where possible, code-split heavy
   features, virtualise long lists.

The framed model feels more integrated, but it is the one part of the stack that
genuinely does not scale — not with more machines, not with more users. The fix
(navigation / `external`) is *less* code than framing, not more.

## Delivery types reference

The dashboard's `AppEntry.delivery` field selects how a catalog tile behaves:

| Delivery | Behaviour | Use for |
| --- | --- | --- |
| `shell-native` | A React view inside the Dashboard SPA at the app's path. No second runtime. | Lightweight first-party surfaces (Mail, Cloud views). |
| `external` | The tile is a link to the app's `publicUrl`, opened in a new tab. One runtime per tab. | Full standalone SPAs with their own origin (Chat, Hosting). **Preferred for anything heavy.** |
| `framed` | The app is mounted in an `<iframe>` inside the shell. Runs a second full runtime in the same tab. | Avoid for heavy apps; only for small embeds where in-shell context is essential. |
| `proxied-app` | Reserved for apps reached through a node proxy rather than a direct origin. | Proxy-routed apps. |
