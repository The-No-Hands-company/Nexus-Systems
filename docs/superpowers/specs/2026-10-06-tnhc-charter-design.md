# TNHC Charter — Design

**Date:** 2026-10-06
**Status:** Approved in conversation, awaiting written-spec review
**Chapter:** 1 of the TNHC Handbook (Charter → Brand core → Design system → Voice → Engineering → Rollout)

## Why

The No Hands Company is starting to run public services (sign-in, email,
hosting) and publish apps. Its identity is currently scattered and, in places,
contradicts itself:

- The mission says everything is open and owned by no one, yet the style
  guide, logo guide and mission statement end in `© All rights reserved`, the
  style guide claims a `™` legal name, and the Zajfan Engineering Standard is
  licensed "INTERNAL — All rights reserved".
- Nexus-Systems, tnhc.dev, zajfan.dev and tnhc-community are public with **no
  LICENSE file**, which legally means all rights reserved.
- Privacy claims about the Phantom Protocol exceed what the code does.

The Charter is the document every other handbook chapter answers to. It states
what TNHC is, what it promises, and how it is run — and every sentence in it
must be true when checked against the code.

## Decisions

| Topic | Decision |
|---|---|
| Identity statement | "The No Hands Company offers, for free, the kind of software and services others charge for — self-hosted, federated and open to everyone." The founder's personal motivation is not part of the public text. |
| Licensing | Copyleft for the platform, permissive for building blocks, Creative Commons for words and assets (table below). |
| Name and logo | A light policy: fork, rebrand and refer to TNHC freely; do not present something as TNHC that is not. Purpose: protect users from impostor services (phishing), not restrict competition. |
| AI | Stated openly, not the headline: software is built by AI under human direction; responsibility for what ships stays with people; attribution is visible in repos and commits. |
| Governance | Founder-led today, said plainly. Written trigger: once there are **3 regular contributors**, major changes go through public RFCs and maintainers decide within their areas. The Charter itself then changes only through that process. |
| Money | Donations only — never asked for, never required. Donations buy no features, priority or influence. Every donation and expense is published. Anyone may charge for running their *own* node. |
| User promises | Firm rights, honest availability (below). |
| Conduct | Adopt the Contributor Covenant (current version) rather than writing a custom code. |

### Licences

| What | Licence |
|---|---|
| Platform: Nexus-Systems apps/services | AGPL-3.0-or-later |
| Libraries, SDKs, protocol crates, the design system (`packages/*`, Phantom) | Apache-2.0 |
| Documents, handbook, brand assets | CC BY 4.0 |
| The TNHC name and logo | Name-and-logo policy in the Charter (usage rules, not ownership) |

Nexus-Systems is a monorepo, so the licence is per directory: the root
`LICENSE` is AGPL-3.0, and each package under `packages/` carries its own
Apache-2.0 `LICENSE` and a matching `license` field in its manifest. A
`LICENSING.md` at the root states the split.

### User promises

**Firm (cost nothing to keep, kept always):**
- TNHC collects no data of any kind: no tracking, no telemetry, no analytics, no profiling.
- Your data is exportable at any time in open formats.
- You can move to your own node at any time.
- Deleting your account deletes your data.

**Honest availability:** TNHC's free public services run on TNHC's own
hardware, best effort. There is no uptime or support guarantee; self-hosting
and federation are how you get guarantees.

**Phantom Protocol:** the no-collection promise is a policy commitment kept by
not building collection into anything. Separately: "TNHC is building the
Phantom Protocol to enforce this cryptographically, so that even TNHC's own
infrastructure cannot see what you do." The Charter links to the public
Phantom status page and never claims more than it shows.

## Charter content (charter.md)

Plain-spoken, readable in about five minutes, in this order:

1. **What TNHC is** — the identity statement.
2. **Our promises** — open source, nothing sold, nothing paywalled, no lock-in; the firm user promises; honest availability; the Phantom sentence and link.
3. **Ownership and licences** — nobody owns TNHC's work more than anyone else; the licence table; the name-and-logo policy.
4. **Money** — the donations rules.
5. **How it is built** — the AI statement; link to the Engineering chapter (Zajfan Standard) once it exists.
6. **Who decides** — founder-led now; the 3-contributor trigger; RFCs; how the Charter itself changes.
7. **Taking part** — how to report problems, propose ideas, contribute; the Code of Conduct.

## Where it lives and how it is published

- **Source of truth:** `github.com/The-No-Hands-company/handbook` (public,
  created 2026-10-06, empty). Contents for this chapter: `README.md`,
  `charter.md`, `CODE_OF_CONDUCT.md`, `LICENSE` (CC BY 4.0). Later chapters
  are added as sibling files.
- **Published at `tnhc.dev/charter`**, generated from `charter.md` at build
  time — the same derive-don't-copy rule `tnhc.dev/WEBMASTER.md` already
  enforces for the app directory. The site build fetches a pinned handbook
  commit, so a site rebuild is reproducible.
- **Findable everywhere:** a "Charter" link in the footer of tnhc.dev, the
  dashboard (app.tnhc.dev), zajfan.tnhc.dev, and the tnhc-community app's About
  screen; the GitHub organisation profile (`.github/profile/README.md`) shows
  the promises in short form and links to the full Charter.
- **Phantom status:** `STATUS.md` in the Phantom repo, beside the code. Each
  capability is listed as done / not done, and each line names the test that
  proves it (for unfinished work, the ignored test that will). Published at
  `tnhc.dev/phantom`, generated the same way.
- **Deferred:** a dedicated `handbook.tnhc.dev` site, until the Brand and
  Design chapters exist.

## Making the repos match the Charter

| Repo | Change |
|---|---|
| Nexus-Systems | Root `LICENSE` AGPL-3.0; Apache-2.0 `LICENSE` in each `packages/*`; `LICENSING.md`; manifest `license` fields |
| tnhc.dev | `LICENSE` (AGPL-3.0 for code); Charter + Phantom pages; footer link |
| tnhc-community (Android) | `LICENSE` AGPL-3.0; About-screen Charter link |
| Zajfan/zajfan.dev | `LICENSE`; footer Charter link |
| Zajfan/Phantom | `LICENSE` Apache-2.0; `STATUS.md` |
| Zajfan Standard | Replace "INTERNAL — All rights reserved" with CC BY 4.0 |
| Org docs (style guide, logo guide, brand guidelines, mission statement) | Remove `© All rights reserved` and `™`; point to the Charter |

## Out of scope (recorded for later decisions)

- **Personal-account repos.** Phantom and zajfan.dev live under the `Zajfan`
  account, not the organisation. Whether to transfer them is a separate
  decision; the Charter applies to them either way through their licences.
- **The Zajfan Standard's home.** It has no repository of its own (its folder
  sits in a tree wired to Nexus-Systems). Resolved in the Engineering chapter.
- Brand, design system, voice, engineering and rollout — later chapters.
- Implementing Phantom payload encryption — engineering work the status page
  tracks, not part of this chapter.

## Verification

- Every public TNHC repo has a `LICENSE` matching the table; GitHub's licence
  detection shows it.
- `grep -ri "all rights reserved\|™"` across TNHC repos returns nothing outside
  third-party files.
- `tnhc.dev/charter` renders the exact text at the pinned handbook commit;
  `tnhc.dev/phantom` renders `STATUS.md`.
- Each footer/About link resolves to `tnhc.dev/charter` (200).
- Claims check: every capability the Charter or status page asserts maps to a
  passing test or is explicitly labelled not done.
